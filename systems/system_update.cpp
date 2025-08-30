// Header Calls
#include <iostream>
#include <string>
#include <vector>
#include <curl/curl.h>
#include "nlohmann/json.hpp"
#include <pqxx/pqxx>
#include <sstream>
#include <iomanip>
// Make life easy with namespace.
using json = nlohmann::json;
// Solar System Structure
struct SolarSystemEntry {
    int solarSystemId;
    std::string solarSystemName;
    double x;
    double y;
    double z;
    int constellationId;
    int regionId;
};
// Write callback to capture response
size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    ((std::string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}
// Where all the happenings occur.
int main() {
    // Fetch all solar systems.
    const int limit = 1000;
    const int total_systems = 24502; // Must check before running.
    std::vector<int> offsets;
    for (int offset = 0; offset < total_systems; offset += limit) {
        offsets.push_back(offset);
    }
    std::vector<SolarSystemEntry> systems;
    curl_global_init(CURL_GLOBAL_ALL);
    for (size_t i = 0; i < offsets.size(); ++i) {
        int offset = offsets[i];
        std::ostringstream url;
        url << "https://blockchain-gateway-stillness.live.tech.evefrontier.com/v2/solarsystems?limit=" << limit << "&offset=" << offset;
        CURL* curl = curl_easy_init();
        std::string response_string;
        if (curl) {
            curl_easy_setopt(curl, CURLOPT_URL, url.str().c_str());
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_string);
            CURLcode res = curl_easy_perform(curl);
            long http_code = 0;
            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
            curl_easy_cleanup(curl);
            if (http_code != 200) {
                std::cerr << "Request failed at offset " << offset << ": HTTP " << http_code << std::endl;
                continue;
            }
        }
        // Parse JSON package
        try {
            json j = json::parse(response_string);
            if (!j.contains("data") || !j["data"].is_array()) {
                std::cerr << "Malformed response at offset " << offset << std::endl;
                continue;
            }
            for (const auto& sys : j["data"]) {
                SolarSystemEntry entry;
                entry.solarSystemId = sys.value("id", 0);
                entry.solarSystemName = sys.value("name", "");
                if (sys.contains("location") && sys["location"].is_object()) {
                    entry.x = sys["location"].value("x", 0.0);
                    entry.y = sys["location"].value("y", 0.0);
                    entry.z = sys["location"].value("z", 0.0);
                } else {
                    entry.x = entry.y = entry.z = 0.0;
                }
                entry.constellationId = sys.value("constellationId", 0);
                entry.regionId = sys.value("regionId", 0);
                systems.push_back(entry);
            }
        } catch (const std::exception& e) {
            std::cerr << "JSON parse error at offset " << offset << ": " << e.what() << std::endl;
            continue;
        }
    }
    // Clean up
    curl_global_cleanup();
    std::cout << "Fetched " << systems.size() << " solar systems, inserting into database..." << std::endl;
    // Try/catch block
    try {
        // Adjust connection string as needed for your DB
        pqxx::connection c(/*"host= dbname= user= password="*/);
        pqxx::work txn(c);
        // Delete all rows that exist in the database.
        txn.exec("DELETE FROM systems;");
        // Efficiency through a prepared statement.
        c.prepare("insert_system",
            "INSERT INTO systems (solar_system_id, solar_system_name, x, y, z, constellation_id, region_id) "
            "VALUES ($1, $2, $3, $4, $5, $6, $7)"
        );
        // Run through the data.
        for (const auto& system : systems) {
            txn.exec_prepared("insert_system",
                system.solarSystemId,
                system.solarSystemName,
                system.x,
                system.y,
                system.z,
                system.constellationId,
                system.regionId
            );
        }
        // Transaction commit
        txn.commit();
        std::cout << "Deleted all rows and inserted " << systems.size() << " systems into systems table." << std::endl;
        // Confirm insertion
        pqxx::work check_txn(c);
        pqxx::result r = check_txn.exec("SELECT * FROM systems LIMIT 5;");
        std::cout << "Sample values from systems table:" << std::endl;
        for (auto row : r) {
            std::cout << "ID: " << row["solar_system_id"].as<int>() << ", Name: " << row["solar_system_name"].as<std::string>() << std::endl;
        }
        check_txn.commit();
    } catch (const std::exception& ex) {
        std::cerr << "Database error: " << ex.what() << std::endl;
        return 1;
    }
    // We made it to end.
    return 0;
}
