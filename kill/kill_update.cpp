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
    long long killTimestampUnix;
};
// Change killTimestamp from LDAP to UNIX before entry into database.
// --------------------------------------------------------------------------------------------------------------------------
// Number of 100-nanosecond intervals in one second
const long long UNITS_PER_SECOND = 10000000LL;
// Number of seconds between Jan 1, 1601 (LDAP epoch) and Jan 1, 1970 (Unix epoch)
const long long EPOCH_DIFFERENCE_SECONDS = 11644473600LL;
// Converts an LDAP/FILETIME timestamp (100-nanosecond intervals since 1601 UTC) to a Unix timestamp (seconds since 1970 UTC).
long long ldap_100ns_to_unix(long long ldap_timestamp_100ns) {
    long long seconds_since_1601_epoch = ldap_timestamp_100ns / UNITS_PER_SECOND;
    long long unix_timestamp = seconds_since_1601_epoch - EPOCH_DIFFERENCE_SECONDS;
    return unix_timestamp;
}
// Write callback to capture response
size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    ((std::string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}
// Main Parse and Database Entry
int main() {
    // Using cURL library to get information from the Mud Index based on World Address.
    CURL* curl;
    CURLcode res;
    struct curl_slist* headers = NULL;
    // Post SQL Query to Mud Index
    std::string data = R"([{"address":"0x7085f3e652987f656fb8dee5aa6592197bb75de8","query":"SELECT \"evefrontier__KillMail\".\"killMailId\" AS \"killMailId\", \"evefrontier__KillMail\".\"killerCharacterId\" AS \"killerCharacterId\", \"evefrontier__KillMail\".\"victimCharacterId\" AS \"victimCharacterId\", \"evefrontier__KillMail\".\"lossType\" AS \"lossType\", \"evefrontier__KillMail\".\"solarSystemId\" AS \"solarSystemId\", \"evefrontier__KillMail\".\"killTimestamp\" AS \"killTimestamp\" FROM \"evefrontier__KillMail\" ORDER BY \"killMailId\" ASC"}])";
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
    // Json Parsing
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
    // Actual data rows start from index 1 of the first inner array.
    const auto& data_rows = result[0];
    // Check rows for structure.
    for (size_t i = 1; i < data_rows.size(); ++i) { // Start from 1 to skip header row
        const auto& row = data_rows[i];
        // Ensure the row is an array and has enough elements
        if (!row.is_array() || row.size() != 6) {
            std::cerr << "Skipping malformed row (not an array or incorrect size): " << row.dump() << std::endl;
            continue;
    }
    // Unix Conversion and working with JSON structures.
    try {
            // Extract the timestamp string (e.g., "133970895260000000")
            std::string raw_kill_timestamp_str = row[5].get<std::string>();

            // Convert the string to a long long integer
            long long ldap_timestamp_100ns = std::stoll(raw_kill_timestamp_str);

            // Convert the 100-nanosecond LDAP time to Unix timestamp
            long long unix_ts = ldap_100ns_to_unix(ldap_timestamp_100ns);

            KillMailEntry entry{
                row[0].get<std::string>(), // killMailId
                row[1].get<std::string>(), // killerCharacterId
                row[2].get<std::string>(), // victimCharacterId
                row[3].get<std::string>(), // lossType
                row[4].get<std::string>(), // solarSystemId
                unix_ts                    // killTimestampUnix
            };
            entries.push_back(std::move(entry));
        } catch (const std::exception& e) {
            std::cerr << "Error processing row " << i << ": " << e.what() << ". Skipping." << std::endl;
            continue;
        }
    }
    // Tell me what program did.
    std::cout << "Parsed " << entries.size() << " killmails, inserting into database..." << std::endl;
    // Try/catch block for insertion
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
            txn.exec_prepared("insert_incident", incident.killMailId, incident.killerCharacterId, incident.victimCharacterId, incident.lossType, incident.solarSystemId, incident.killTimestampUnix);
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
