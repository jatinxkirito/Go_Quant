#define CURL_STATICLIB
#pragma once
#include <iostream>
#include <string>
#include "curl/curl.h"
#include <json/json.h>

// Callback function to handle response data
size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* userp) {
    userp->append((char*)contents, size * nmemb);
    return size * nmemb;
}

class DeribitAuth {
private:
    std::string apiUrl;
    std::string clientId;
    std::string clientSecret;
    CURL* curl;

public:
    DeribitAuth(const std::string& id, const std::string& secret)
        : apiUrl("https://test.deribit.com/api/v2/public/auth"),
        clientId(id),
        clientSecret(secret) {
       
        // Initialize CURL
        curl_global_init(CURL_GLOBAL_ALL);
        curl = curl_easy_init();
    }

    ~DeribitAuth() {
        if (curl) {
            curl_easy_cleanup(curl);
        }
        curl_global_cleanup();
    }

    std::pair<bool,std::string> authenticate() {
        if (!curl) {
            std::cerr << "Failed to initialize CURL" << std::endl;
            return {false, "Failed" };
        }

        // Create JSON payload
        Json::Value root;
        root["jsonrpc"] = "2.0";
        root["id"] = 9929;
        root["method"] = "public/auth";

        Json::Value params;
        params["grant_type"] = "client_credentials";
        params["client_id"] = clientId;
        params["client_secret"] = clientSecret;
       /* params["scope"]= "sco "connection mainaccount"*/
        root["params"] = params;

        // Convert JSON to string
        Json::FastWriter writer;
        std::string jsonString = writer.write(root);

        // Response string
        std::string response;

        // Set up CURL options
        curl_easy_setopt(curl, CURLOPT_URL, apiUrl.c_str());
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, jsonString.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

        // Set headers
        struct curl_slist* headers = NULL;
        headers = curl_slist_append(headers, "Content-Type: application/json");
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

        // Perform the request
        CURLcode res = curl_easy_perform(curl);

        // Clean up headers
        curl_slist_free_all(headers);

        if (res != CURLE_OK) {
            std::cerr << "curl_easy_perform() failed: " << curl_easy_strerror(res) << std::endl;
            return {false,"Failed"};
        }

        // Parse response
        try {
            Json::Value jsonResponse;
            Json::Reader reader;

            bool parsingSuccessful = reader.parse(response, jsonResponse);
            if (!parsingSuccessful) {
                std::cerr << "Failed to parse response" << std::endl;
                return { false,"Failed" };;
            }

            // Check for errors in response
            if (jsonResponse.isMember("error")) {
                std::cerr << "API Error: " << jsonResponse["error"].toStyledString() << std::endl;
                return { false,"Failed" };;
            }
           /* std::string s = jsonResponse["result"]["access_token"].asString();
            std::cout << s << std::endl;*/
           
            // Print successful response
           
            return {true,jsonResponse["result"]["access_token"].asString()};

        }
        catch (const std::exception& e) {
            std::cerr << "Exception while parsing response: " << e.what() << std::endl;
            return { false,"Failed" };
        }
    }
};