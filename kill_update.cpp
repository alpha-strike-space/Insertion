#include <iostream>
#include <string>
#include <curl/curl.h>

// Write callback to capture response
size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    ((std::string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}

int main() {
    CURL* curl;
    CURLcode res;
    struct curl_slist* headers = NULL;
    std::string data = R"([{"address":"0xcdb380e0cd3949caf70c45c67079f2e27a77fc47","query":"SELECT \"evefrontier__KillMail\".\"killMailId\" AS \"killMailId\", \"evefrontier__KillMail\".\"killerCharacterId\" AS \"killerCharacterId\", \"evefrontier__KillMail\".\"victimCharacterId\" AS \"victimCharacterId\", \"evefrontier__KillMail\".\"lossType\" AS \"lossType\", \"evefrontier__KillMail\".\"solarSystemId\" AS \"solarSystemId\", \"evefrontier__KillMail\".\"killTimestamp\" AS \"killTimestamp\" FROM \"evefrontier__KillMail\" ORDER BY \"killMailId\" ASC"}])";
    std::string response_string;

    curl_global_init(CURL_GLOBAL_ALL);
    curl = curl_easy_init();
    if (curl) {
        // Set URL
        curl_easy_setopt(curl, CURLOPT_URL, "https://indexer.mud.pyropechain.com/q");

        // Set headers
        headers = curl_slist_append(headers, "accept: application/json");
        headers = curl_slist_append(headers, "content-type: application/json");
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

        // Set POST data
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, data.c_str());

        // Set write callback to store response
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_string);

        // Perform request
        res = curl_easy_perform(curl);

        // Print response
        std::cout << "Response: " << response_string << std::endl;

        // Cleanup
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
    }
    curl_global_cleanup();

    return 0;
}
