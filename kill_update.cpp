// Header Calls
#include <iostream>
#include <string>
#include <vector>
#include <curl/curl.h>
#include "nlohmann/json.hpp"
#include <pqxx/pqxx>
// Make life easy with namespace.
using json = nlohmann::json;
// Mail Structure
struct KillMailEntry {
    std::string killMailId;
    std::string killerCharacterId;
    std::string victimCharacterId;
    std::string lossType;
    std::string solarSystemId;
    std::string killTimestamp;
};
// Write callback to capture response
size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    ((std::string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}
// Main Parse and Database Entry
int main() {
    // === CURL PART: Get JSON from the API ===
    CURL* curl;
    CURLcode res;
    struct curl_slist* headers = NULL;
    // Post SQL Query to Mud Index
    std::string data = R"([{"address":"0xcdb380e0cd3949caf70c45c67079f2e27a77fc47","query":"SELECT \"evefrontier__KillMail\".\"killMailId\" AS \"killMailId\", \"evefrontier__KillMail\".\"killerCharacterId\" AS \"killerCharacterId\", \"evefrontier__KillMail\".\"victimCharacterId\" AS \"victimCharacterId\", \"evefrontier__KillMail\".\"lossType\" AS \"lossType\", \"evefrontier__KillMail\".\"solarSystemId\" AS \"solarSystemId\", \"evefrontier__KillMail\".\"killTimestamp\" AS \"killTimestamp\" FROM \"evefrontier__KillMail\" ORDER BY \"killMailId\" ASC"}])";
    std::string response_string;
    // Make curl.
    curl_global_init(CURL_GLOBAL_ALL);
    curl = curl_easy_init();
    if (curl) {
        curl_easy_setopt(curl, CURLOPT_URL, "https://indexer.mud.pyropechain.com/q");
        headers = curl_slist_append(headers, "accept: application/json");
        headers = curl_slist_append(headers, "content-type: application/json");
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, data.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_string);
        res = curl_easy_perform(curl);
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
    }
    curl_global_cleanup();
    // If your API returns Response: prefix, strip it
    std::string prefix = "Response: ";
    if (response_string.rfind(prefix, 0) == 0) {
        response_string = response_string.substr(prefix.size());
    }
    // === JSON PARSE PART ===
    json j;
    try {
        j = json::parse(response_string);
    } catch (const std::exception& e) {
        std::cerr << "JSON parse error: " << e.what() << std::endl;
        return 1;
    }
    // Parse json output from curl.
    std::vector<KillMailEntry> entries;
    auto result = j["result"];
    if (result.empty()) {
        std::cerr << "No results found in JSON." << std::endl;
        return 1;
    }
    for (size_t i = 1; i < result[0].size(); ++i) {
        auto& row = result[0][i];
        if (row.size() != 6) continue;
        KillMailEntry entry{
            row[0].get<std::string>(),
            row[1].get<std::string>(),
            row[2].get<std::string>(),
            row[3].get<std::string>(),
            row[4].get<std::string>(),
            row[5].get<std::string>(),
        };
        entries.push_back(std::move(entry));
    }
    // Tell me what program did.
    std::cout << "Parsed " << entries.size() << " killmails, inserting into database..." << std::endl;
    // === POSTGRESQL PART ===
    try {
        // Adjust connection string as needed for your DB
        pqxx::connection c(/*"host= dbname= user= password="*/);
        pqxx::work txn(c);
        // Prepare the statement for efficiency
        c.prepare("insert_incident",
            "INSERT INTO incident (id, killer_id, victim_id, loss_type, solar_system_id, time_stamp) "
            "VALUES ($1, $2, $3, $4, $5, $6) "
            "ON CONFLICT (id) DO NOTHING"
        );
        // Go for insertion into table.
        for (const auto& incident : entries) {
            txn.exec_prepared("insert_incident", incident.killMailId, incident.killerCharacterId, incident.victimCharacterId, incident.lossType, incident.solarSystemId, incident.killTimestamp);
        }
        // Commit Transaction
        txn.commit();
        std::cout << "Inserted killmails uniquely into PostgreSQL!" << std::endl;
    // Error handling.
    } catch (const std::exception& ex) {
        std::cerr << "Database error: " << ex.what() << std::endl;
        return 1;
    }
    // We made it to end.
    return 0;
}
