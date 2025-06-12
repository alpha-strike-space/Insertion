#include <iostream>
#include <pqxx/pqxx>
#include <nlohmann/json.hpp>
#include <curl/curl.h>
#include <sstream>
#include <cstdlib> // For getenv
#include <string>
#include <ctime>

using json = nlohmann::json;
// Direct Connection
std::string get_direct_connection_string() {
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
}
// Callback function for cURL response handling
size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    ((std::string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}

int main() {
    try {
        // PostgreSQL connection details
        pqxx::connection C(get_direct_connection_string());
        if (C.is_open()) {
            std::cout << "Opened database successfully: " << C.dbname() << std::endl;
        } else {
            std::cerr << "Can't open database" << std::endl;
            return 1;
        }

        // Prepare statement for inserting incidents with conflict resolution
        C.prepare("insert_incident",
                    "INSERT INTO incident "
                    "(victim_id, victim_address, victim_name, killer_id, killer_address, killer_name, solar_system_id, loss_type, time_stamp) "
                    "VALUES ($1, $2, $3, $4, $5, $6, $7, $8, $9) "
                    "ON CONFLICT (victim_name, time_stamp) DO NOTHING;");

        pqxx::work W(C);

        // Pagination variables
        const int limit = 100;
        int offset = 0;
        bool moreData = true;

        while (moreData) {
            std::string readBuffer;
            CURL* curl = curl_easy_init();
            if (curl) {
                std::ostringstream url;
                url << "https://world-api-stillness.live.tech.evefrontier.com/v2/killmails?limit=" << limit << "&offset=" << offset;
                curl_easy_setopt(curl, CURLOPT_URL, url.str().c_str());
                curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
                curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);
                CURLcode res = curl_easy_perform(curl);
                curl_easy_cleanup(curl);

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

                // Insert each incident in the "data" array
                for (const auto& incident : response["data"]) {
                    // Strip CCP time to UNIX
                    std::string time_str = incident["time"].get<std::string>();
                    struct tm tm = {};
                    strptime(time_str.c_str(), "%Y-%m-%dT%H:%M:%SZ", &tm);
                    time_t unix_time = timegm(&tm);
                    // JSON to INSERT
                    W.exec_prepared("insert_incident",
                        incident["victim"]["id"].get<std::string>(),
                        incident["victim"]["address"].get<std::string>(),
                        incident["victim"]["name"].get<std::string>(),
                        incident["killer"]["id"].get<std::string>(),
                        incident["killer"]["address"].get<std::string>(),
                        incident["killer"]["name"].get<std::string>(),
                        incident["solarSystemId"].get<int>(),
                        incident["lossType"].get<std::string>(),
                        static_cast<long long>(unix_time) // UNIX timestamp
                    );
                }
            }
            offset += limit;
        }

        // Commit the transaction
        W.commit();
        std::cout << "Records inserted successfully (duplicates ignored)." << std::endl;

    } catch (const std::exception &e) {
        std::cerr << e.what() << std::endl;
        return 1;
    }
    return 0;
}
