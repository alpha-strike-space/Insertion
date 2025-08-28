#include <iostream>
#include <pqxx/pqxx>
#include <nlohmann/json.hpp>
#include <curl/curl.h>
#include <sstream>
#include <cstdlib> // For getenv
#include <string>
// Making life easy with namespace
using json = nlohmann::json;
// Direct Connection
// Note when using cron jobs, profile is not sourcable. You must find a solution for sourcing the environment variables as it will not work.
/*std::string get_direct_connection_string() {
    const char* dbname = std::getenv("PGDIRECT_DB");
    const char* user = std::getenv("PGDIRECT_USER");
    const char* password = std::getenv("PGDIRECT_PASSWORD");
    const char* host = std::getenv("PGDIRECT_HOST");
    const char* port = std::getenv("PGDIRECT_PORT");

    if (!dbname || !user || !password || !host || !port) {
        throw std::runtime_error("Database environment variables are not set for listener. Please check your .env file.");
    }

    return "dbname=" + std::string(dbname) +
           " user=" + std::string(user) +
           " password=" + std::string(password) +
           " host=" + std::string(host) +
           " port=" + std::string(port);
}*/
// Callback function for cURL response handling
size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    ((std::string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}
// Where everything happens
int main() {
    try {
        // Postgres pointer.
        pqxx::connection C("dbname= user= password= host= port=");
        if (C.is_open()) {
            std::cout << "Opened database successfully: " << C.dbname() << std::endl;
        } else {
            std::cerr << "Can't open database" << std::endl;
            return 1;
        }
        // Prepare statement for setting tribe_id by address
        C.prepare("update_character_tribe",
            "UPDATE character SET tribe_id = $2 WHERE address = $1");
        pqxx::work W(C);
        // Pagination variables
        const int limit = 100;
        int offset = 0;
        bool moreData = true;
        long long defaultTribeId = 1000167; // Clonebank 86
        // Map address to the tribal id.
        std::unordered_map<std::string, long long> address_to_tribe;
        // Do until false
        while (moreData) {
            std::string readBuffer;
            CURL* curl = curl_easy_init();
            if (curl) {
                std::ostringstream url;
                url << "https://world-api-stillness.live.tech.evefrontier.com/v2/tribes?limit=" << limit << "&offset=" << offset;
                curl_easy_setopt(curl, CURLOPT_URL, url.str().c_str());
                curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
                curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);
                CURLcode res = curl_easy_perform(curl);
                curl_easy_cleanup(curl);
                // If not 200, spit error.
                if (res != CURLE_OK) {
                    std::cerr << "cURL error: " << curl_easy_strerror(res) << std::endl;
                    return 1;
                }
                // Parse JSON response
                json response = json::parse(readBuffer);
                if (response["data"].empty()) {
                    moreData = false;
                    break;
                }
                // For each tribe, fetch its members and map address to tribe id
                for (const auto& tribe : response["data"]) {
                    long long tribeId = tribe["id"].get<long long>();
                    // Fetch tribe details like members
                    std::string tribeDetailBuffer;
                    CURL* tribeCurl = curl_easy_init();
                    if (tribeCurl) {
                        std::ostringstream tribeUrl;
                        tribeUrl << "https://world-api-stillness.live.tech.evefrontier.com/v2/tribes/" << tribeId;
                        curl_easy_setopt(tribeCurl, CURLOPT_URL, tribeUrl.str().c_str());
                        curl_easy_setopt(tribeCurl, CURLOPT_WRITEFUNCTION, WriteCallback);
                        curl_easy_setopt(tribeCurl, CURLOPT_WRITEDATA, &tribeDetailBuffer);
                        CURLcode tribeRes = curl_easy_perform(tribeCurl);
                        curl_easy_cleanup(tribeCurl);
                        // If not 200, spit error
                        if (tribeRes != CURLE_OK) {
                            std::cerr << "cURL error (tribe members): " << curl_easy_strerror(tribeRes) << std::endl;
                            continue;
                        }
                        // Json
                        json tribeDetails = json::parse(tribeDetailBuffer);
                        // Check to make sure we aren't empty of members
                        if (!tribeDetails.contains("members") || tribeDetails["members"].empty())
                            continue;
                        // Go through members and get addresses.
                        for (const auto& member : tribeDetails["members"]) {
                            if (member.contains("address")) {
                                address_to_tribe[member["address"].get<std::string>()] = tribeId;
                            }
                        }
                        std::cout << "Processed tribe " << tribeId << " with " << tribeDetails["members"].size() << " members." << std::endl;
                    }
                }
            }
            offset += limit;
        }
        // Update character table: assign tribe_id to each address found in tribes
        for (const auto& entry : address_to_tribe) {
            W.exec_prepared("update_character_tribe", entry.first, entry.second);
        }
        // Set default tribe_id for all characters not found in any tribe, in case we see any change in membership.
        W.exec("UPDATE character SET tribe_id = 1000167 WHERE tribe_id IS NULL");
        // Commit
        W.commit();
        std::cout << "Character tribe assignments updated. All unassigned characters set to Clonebank 86." << std::endl;
    } catch (const std::exception &e) {
        std::cerr << e.what() << std::endl;
        return 1;
    }
    // End of file
    return 0;
}
