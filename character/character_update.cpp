#include <iostream>
#include <pqxx/pqxx>
#include <nlohmann/json.hpp>
#include <curl/curl.h>
#include <sstream>
#include <string>
#include <cstdlib>
// Making life easy with the namespace
using json = nlohmann::json;
// cURL response handler
size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    ((std::string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}
// Get the offset for the next API call, based on last inserted record/
int get_last_offset(pqxx::connection& C) {
    pqxx::work W(C);
    pqxx::result R = W.exec("SELECT COUNT(*) FROM character;"); // Get count
    int offset = R[0][0].as<int>();
    W.commit(); 
    return offset;
}
// Where all the good happens
int main() {
    // Try/catch block
    try {
        // Connection pointer
        pqxx::connection C(/*"dbname= user= password= host= port="*/);
        // Check that it opened
        if (!C.is_open()) {
            std::cerr << "Can't open database" << std::endl;
            return 1;
        }
        std::cout << "Opened database successfully: " << C.dbname() << std::endl;
        // Prepare insert statement
        C.prepare("insert_character",
            "INSERT INTO character (id, address, name) VALUES ($1, $2, $3) ON CONFLICT (id) DO NOTHING;"
        );
        // Start offset based on database
        int offset = get_last_offset(C);
        const int limit = 100;
        bool moreData = true;
        pqxx::work W(C);
        // Do while true, until false
        while (moreData) {
            std::string readBuffer;
            CURL* curl = curl_easy_init();
            if (curl) {
                std::ostringstream url;
                url << "https://world-api-stillness.live.tech.evefrontier.com/v2/smartcharacters?limit=" << limit << "&offset=" << offset;
                curl_easy_setopt(curl, CURLOPT_URL, url.str().c_str());
                curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
                curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);
                CURLcode res = curl_easy_perform(curl);
                curl_easy_cleanup(curl);
                // If not 200, spit error
                if (res != CURLE_OK) {
                    std::cerr << "cURL error: " << curl_easy_strerror(res) << std::endl;
                    break;
                }
                // Parse json
                json response = json::parse(readBuffer);
                // Empty, send packing.
                if (response["data"].empty()) {
                    moreData = false;
                    break;
                }
                // Insert each character
                for (const auto& character : response["data"]) {
                    std::string id = character["id"].get<std::string>();
                    std::string address = character["address"].get<std::string>();
                    std::string name = character["name"].get<std::string>();
                    W.exec_prepared("insert_character", id, address, name);
                }
            }
            offset += limit;
        }
        W.commit();
        std::cout << "Characters inserted successfully (duplicates ignored)." << std::endl;
    } catch (const std::exception &e) {
        std::cerr << e.what() << std::endl;
        return 1;
    }
    return 0;
}
