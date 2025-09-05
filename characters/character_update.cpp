#include <iostream>
#include <pqxx/pqxx>
#include <nlohmann/json.hpp>
#include <curl/curl.h>
#include <sstream>
#include <string>
#include <cstdlib> // Get environment
// Making life easy with the namespace
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
// cURL response handler
size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    ((std::string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}
// Get the offset for the next API call, based on last inserted record/
int get_last_offset(pqxx::connection& c) {
    pqxx::work W(c);
    pqxx::result R = W.exec("SELECT COUNT(*) FROM characters;"); // Get count
    int offset = R[0][0].as<int>();
    W.commit(); 
    return offset;
}
// Hex conversion to raw bytes for database.
std::string hex_to_bytes(const std::string& hex) {
    std::string bytes;
    // Skip "0x" if present
    size_t start = (hex.rfind("0x", 0) == 0) ? 2 : 0; 
    // Properly convert the hex to raw bytes.
    for (size_t i = start; i < hex.length(); i += 2) {
        std::string byteString = hex.substr(i, 2);
        char byte = (char) strtol(byteString.c_str(), nullptr, 16);
        bytes += byte;
    }
    return bytes; // Send back for insertion.
}
// Where all the good happens
int main() {
    // Try/catch block
    try {
        // Connection pointer
        pqxx::connection c(/*"dbname= user= password= host= port="*/);
        // Check that it opened
        if (!c.is_open()) {
            std::cerr << "Can't open database" << std::endl;
            return 1;
        }
        std::cout << "Opened database successfully: " << c.dbname() << std::endl;
        // Prepare insert statement
        c.prepare("insert_character",
            "INSERT INTO characters (id, address, name) VALUES ($1, $2, $3) ON CONFLICT (id) DO NOTHING;"
        );
        // Start offset based on database
        int offset = get_last_offset(c);
        const int limit = 100;
        bool moreData = true;
        pqxx::work W(c);
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
                    std::string address_bytes = hex_to_bytes(address); // Correctly inserting as bytes into the database.
                    W.exec_prepared("insert_character", id, pqxx::binarystring(address_bytes), name); // Fixed string UTF-8 issue here.
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
