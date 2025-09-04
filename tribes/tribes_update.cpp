#include <iostream>
#include <pqxx/pqxx>
#include <nlohmann/json.hpp> // Json serialization
#include <curl/curl.h> // Get HTTP
#include <sstream>
#include <cstdlib>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <thread> // Thread management
#include <chrono> // Sleeping
#include <ctime>
// Make life easy with namespace
using json = lohmann::json;
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
// Write callback to capture response
size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    ((std::string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}
// UNIX
long long current_unix_time() {
    return static_cast<long long>(std::time(nullptr));
}
// Where everything happens
int main() {
    try {
        // Database connection pointer
        pqxx::connection c(/* "dbname= user= password= host= port=" */);
        // Check to see if we are open
        if (!c.is_open()) {
            std::cerr << "Can't open database" << std::endl;
            return 1;
        }
        std::cout << "Opened database successfully: " << c.dbname() << std::endl;
        // Prepare all usable statements.
        c.prepare("upsert_tribe",
            "INSERT INTO tribes (id, url, name) VALUES ($1, $2, $3) "
            "ON CONFLICT (id) DO UPDATE SET url = EXCLUDED.url, name = EXCLUDED.name");
        c.prepare("insert_membership_history",
            "INSERT INTO character_tribe_membership (character_id, tribe_id, joined_at, left_at) VALUES ($1, $2, $3, NULL)");
        c.prepare("update_membership_history_left",
            "UPDATE character_tribe_membership SET left_at = $1 WHERE character_id = $2 AND left_at IS NULL");
        pqxx::work W(c);
        // Map player ID (stringified numeric) to itself (for character_id)
        std::unordered_map<std::string, std::string> playerid_to_charid;
        pqxx::result chars = W.exec("SELECT id FROM characters");
        for (const auto& row : chars) {
            std::string playerid = row["id"].as<std::string>(); // numeric as string
            playerid_to_charid[playerid] = playerid; // use id for character_id
        }
        // Latest memberships, player id to tribe_id
        std::unordered_map<std::string, long long> latest_membership;
        // Latest playerids in player owned tribes.
        std::unordered_set<std::string> player_owned_tribe_members;
        // Locals
        const int limit = 100;
        int offset = 0;
        bool moreData = true;
        const long long defaultTribeId = 1000167; // Clonebank 86
        // Fetcher
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
                // Not 200, send error
                if (res != CURLE_OK) {
                    std::cerr << "cURL error: " << curl_easy_strerror(res) << std::endl;
                    return 1;
                }
                // Start reading.
                json response = json::parse(readBuffer);
                if (response["data"].empty()) {
                    moreData = false;
                    break;
                }
                // Run through each tribe.
                for (const auto& tribe : response["data"]) {
                    long long tribeId = tribe["id"].get<long long>();
                    std::string tribeUrl = tribe["tribeUrl"].get<std::string>();
                    if (tribeUrl.empty()) tribeUrl = "NONE";
                    std::string tribeName = tribe["name"].get<std::string>();
                    // Upsert tribe info
                    W.exec_prepared("upsert_tribe", tribeId, tribeUrl, tribeName);
                    // Non-default tribes.
                    if (tribeId == defaultTribeId) { 
                        continue; // Do NOT fetch members for default tribe
                    }
                    // Fetch if we have an update for player tribes.
                    std::string tribeDetailBuffer;
                    CURL* tribeCurl = curl_easy_init();
                    if (tribeCurl) {
                        std::ostringstream tribeUrlStream;
                        tribeUrlStream << "https://world-api-stillness.live.tech.evefrontier.com/v2/tribes/" << tribeId;
                        curl_easy_setopt(tribeCurl, CURLOPT_URL, tribeUrlStream.str().c_str());
                        curl_easy_setopt(tribeCurl, CURLOPT_WRITEFUNCTION, WriteCallback);
                        curl_easy_setopt(tribeCurl, CURLOPT_WRITEDATA, &tribeDetailBuffer);
                        CURLcode tribeRes = curl_easy_perform(tribeCurl);
                        curl_easy_cleanup(tribeCurl);
                        // Errors
                        if (tribeRes != CURLE_OK) {
                            std::cerr << "cURL error (tribe members): " << curl_easy_strerror(tribeRes) << std::endl;
                            continue;
                        }
                        // Read json
                        json tribeDetails = json::parse(tribeDetailBuffer);
                        if (!tribeDetails.contains("members") || tribeDetails["members"].empty())
                            continue;
                        // Map playerids to tribe_id for history.
                        for (const auto& member : tribeDetails["members"]) {
                            // Always use player_id (stringified numeric) for membership
                            std::string playerid;
                            if (member.contains("id")) {
                                // API always gives id as a string
                                if (member["id"].is_string()) {
                                    playerid = member["id"].get<std::string>();
                                } else {
                                    playerid = member["id"].dump();
                                }
                                latest_membership[playerid] = tribeId;
                                player_owned_tribe_members.insert(playerid);
                            }
                        }
                        std::cout << "Processed tribe " << tribeId << " with " << tribeDetails["members"].size() << " members." << std::endl;
                        std::this_thread::sleep_for(std::chrono::milliseconds(300)); // Friendly API usage
                    }
                }
            }
            offset += limit;
        }
        // Get current time in UNIX
        long long now_unix = current_unix_time();
        // Find open memberships.
        pqxx::result open_memberships = W.exec(
            "SELECT character_id, tribe_id FROM character_tribe_membership WHERE left_at IS NULL"
        );
        // Map character_id to tribe_id for open memberships
        std::unordered_map<std::string, long long> open_char_tribe;
        for (const auto& row : open_memberships) {
            open_char_tribe[row["character_id"].as<std::string>()] = row["tribe_id"].as<long long>();
        }
        // Run through any tribal membership changes.
        for (const auto& entry : latest_membership) {
            const std::string& playerid = entry.first;
            long long api_tribe_id = entry.second;
            // Player id to char id
            auto charid_it = playerid_to_charid.find(playerid);
            if (charid_it == playerid_to_charid.end()) continue; // Unknown playerid
            std::string char_id = charid_it->second;
            // Find open memberships.
            auto open_it = open_char_tribe.find(char_id);
            if (open_it == open_char_tribe.end()) {
                // No open membership: joined a player-owned tribe
                W.exec_prepared("insert_membership_history", char_id, api_tribe_id, now_unix);
            } else if (open_it->second != api_tribe_id) {
                // Tribe changed: close old, insert new
                W.exec_prepared("update_membership_history_left", now_unix, char_id);
                W.exec_prepared("insert_membership_history", char_id, api_tribe_id, now_unix);
            }
        }
        // Handle those who leave player owned tribes
        for (const auto& open : open_char_tribe) {
            std::string char_id = open.first;
            long long tribe_id = open.second;
            // If character is not in any player-owned tribe in API
            bool still_member = false;
            for (const auto& am : latest_membership) {
                if (am.first == char_id) {
                    still_member = true;
                    break;
                }
            }
            if (!still_member && tribe_id != defaultTribeId) {
                // Close previous membership, i.e. those who left player-owned tribe
                W.exec_prepared("update_membership_history_left", now_unix, char_id);
                // Insert new membership for default tribe (Clonebank 86)
                W.exec_prepared("insert_membership_history", char_id, defaultTribeId, now_unix);
            }
        }
        // Check characters who have never had a membership record, insert default tribe.
        for (const auto& pair : playerid_to_charid) {
            std::string playerid = pair.first;
            std::string char_id = pair.second;
            if (open_char_tribe.find(char_id) == open_char_tribe.end() &&
                latest_membership.find(playerid) == latest_membership.end()) {
                W.exec_prepared("insert_membership_history", char_id, defaultTribeId, now_unix);
            }
        }
        // Pound the tables
        W.commit();
        std::cout << "Tribe and character membership history updated. Default tribe membership inserted after leaving player-owned tribes." << std::endl;
    } catch (const std::exception &e) {
        std::cerr << e.what() << std::endl;
        return 1;
    }
    // End of file
    return 0;
}
