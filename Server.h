#define CURL_STATICLIB
#pragma once
#include <string>
#include <memory>
#include <map>
#include <set>
#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/server.hpp>
#include <json/json.h>
#include "curl/curl.h"

class DeribitAPI {
private:
    std::string access_token;
    std::string base_url = "https://test.deribit.com/api/v2/";
    CURL* curl;

    static size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* userp) {
        userp->append((char*)contents, size * nmemb);
        return size * nmemb;
    }

    std::string makeRequest(const std::string& endpoint, const std::string& params, const std::string& method = "GET") {
        curl_easy_reset(curl);

        std::string url = base_url + endpoint;
        std::string response;

        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

        // Set headers
        struct curl_slist* headers = NULL;
        std::string authHeader = "Authorization: Bearer " + access_token;
        headers = curl_slist_append(headers, authHeader.c_str());
        headers = curl_slist_append(headers, "Content-Type: application/json");
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

        // Set method and parameters
        if (method == "POST") {
            curl_easy_setopt(curl, CURLOPT_POST, 1L);
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, params.c_str());
        }

        // Execute request
        CURLcode res = curl_easy_perform(curl);

        // Free headers
        curl_slist_free_all(headers);

        if (res != CURLE_OK) {
            throw std::runtime_error("Failed to make request: " + std::string(curl_easy_strerror(res)));
        }

        return response;
     
    }

public:
    DeribitAPI(const std::string& token)
        : access_token(token){
        curl = curl_easy_init();
        if (!curl) {
            throw std::runtime_error("Failed to initialize CURL");
        }
    }

    ~DeribitAPI() {
        if (curl) curl_easy_cleanup(curl);
    }

    // Place a new order
    Json::Value placeOrder(const std::string& instrument, const std::string& side,
        double amount, double price, const std::string& type = "limit") {
        Json::Value requestJson;
        requestJson["jsonrpc"] = "2.0";
        requestJson["id"] = std::time(nullptr);  // Using timestamp as request ID
        requestJson["method"] = "private/" + side;

        // Build parameters as a JSON object
        Json::Value params;
        params["instrument_name"] = instrument;
        params["amount"] = amount;
        params["type"] = type;
        if (type == "limit") {
            params["price"] = price;
        }
        requestJson["params"] = params;

        // Convert to string
        Json::FastWriter writer;
        std::string request = writer.write(requestJson);
      /*  std::cout <<"Json request: "<< "\n";
       std:: cout << request <<"\n";*/

        std::string response = makeRequest("", request, "POST");

        Json::Value root;
        Json::Reader reader;
        reader.parse(response, root);
        return root;
    }

    // Cancel an existing order
    Json::Value cancelOrder(const std::string& order_id) {
        Json::Value root;
        root["jsonrpc"] = "2.0";
        root["id"] = std::time(nullptr);   // Unique ID for the request
        root["method"] = "private/cancel";

        Json::Value params;
        params["order_id"] = order_id;

       

        root["params"] = params;

        // Convert JSON to string
        Json::StreamWriterBuilder writer;
        std::string jsonString = Json::writeString(writer, root);

        std::string response = makeRequest("", jsonString, "POST");


        Json::Reader reader;
        reader.parse(response, root);
        return root;
    }

    // Modify an existing order
    Json::Value modifyOrder(const std::string& order_id, double new_price, double new_amount) {
        Json::Value root;
        root["jsonrpc"] = "2.0";
        root["id"] = std::time(nullptr);   // Unique ID for the request
        root["method"] = "private/edit";

        Json::Value params;
        params["order_id"] = order_id;

        // Only add new amount and price if provided (i.e., non-zero)
        if (new_amount > 0) params["amount"] = new_amount;
        if (new_price > 0.0) params["price"] = new_price;

        root["params"] = params;

        // Convert JSON to string
        Json::StreamWriterBuilder writer;
        std::string jsonString = Json::writeString(writer, root);

        std::string response = makeRequest("", jsonString, "POST");

        
        Json::Reader reader;
        reader.parse(response, root);
        return root;
    }

    // Get orderbook for an instrument
    Json::Value getOrderbook(const std::string& instrument) {
        Json::Value root;
        root["jsonrpc"] = "2.0";
        root["id"] = std::time(nullptr);   // Unique ID for the request
        root["method"] = "public/get_order_book";

        Json::Value params;
        params["instrument_name"] = instrument;



        root["params"] = params;

        // Convert JSON to string
        Json::StreamWriterBuilder writer;
        std::string jsonString = Json::writeString(writer, root);

        std::string response = makeRequest("", jsonString, "POST");


        Json::Reader reader;
        reader.parse(response, root);
        return root;
       
    }

    // Get current positions
    Json::Value getPositions(const std::string& currency, const std::string& kind = "") {
        Json::Value root;
        root["jsonrpc"] = "2.0";
        root["id"] = std::time(nullptr);  // Unique ID for the request
        root["method"] = "private/get_positions";

        Json::Value params;
        params["currency"] = currency;
        if (!kind.empty()) {
            params["kind"] = kind;  // Optional: Set to "option" or "future" if needed
        }
        root["params"] = params;

        // Convert JSON to string
        Json::StreamWriterBuilder writer;
        std::string jsonString = Json::writeString(writer, root);

        // Call makeRequest with the specified endpoint and JSON payload
        std::string response = makeRequest("", jsonString, "POST");

        
        Json::Reader reader;
        reader.parse(response, root);
        return root;
    }
};

// WebSocket server for real-time orderbook updates
class OrderbookServer {
    using websocket_server = websocketpp::server<websocketpp::config::asio>;

private:
    websocket_server server;
    std::map<websocketpp::connection_hdl, std::set<std::string>, std::owner_less<websocketpp::connection_hdl>> subscriptions;
    std::shared_ptr<DeribitAPI> api;

    void onMessage(websocketpp::connection_hdl hdl, websocket_server::message_ptr msg) {
        try {
            Json::Value root;
            Json::Reader reader;
            reader.parse(msg->get_payload(), root);

            std::string action = root["action"].asString();
            std::string symbol = root["symbol"].asString();

            if (action == "subscribe") {
                subscriptions[hdl].insert(symbol);
                // Start sending orderbook updates for this symbol
                startOrderbookUpdates(hdl, symbol);
            }
            else if (action == "unsubscribe") {
                subscriptions[hdl].erase(symbol);
            }
        }
        catch (const std::exception& e) {
            std::cerr << "Error processing message: " << e.what() << std::endl;
        }
    }

    void startOrderbookUpdates(websocketpp::connection_hdl hdl, const std::string& symbol) {
        
        // This is a simplified version that sends updates every few seconds. I think a event loop can also be used.
        std::thread([this, hdl, symbol]() {
            while (subscriptions[hdl].count(symbol) > 0) {
                try {
                    Json::Value orderbook = api->getOrderbook(symbol);
                    server.send(hdl, orderbook.toStyledString(), websocketpp::frame::opcode::text);
                    std::this_thread::sleep_for(std::chrono::seconds(1));
                }
                catch (const std::exception& e) {
                    std::cerr << "Error sending orderbook update: " << e.what() << std::endl;
                }
            }
            }).detach();
    }

public:
    OrderbookServer(std::shared_ptr<DeribitAPI> deribit_api) : api(deribit_api) {
        server.set_message_handler(std::bind(&OrderbookServer::onMessage, this,
            std::placeholders::_1, std::placeholders::_2));
    }

    void run(uint16_t port) {
        server.init_asio();
        server.listen(port);
        server.start_accept();
        server.run();
    }
};
//int main()
//{
//     
//    auto api = std::make_shared<DeribitAPI>("6ynaYERD", "PxofNyS_r5RdXTAp2sGLL3DSQypM5qJewT5rW0bjJ3U");
//    api->placeOrder("BTC-PERPETUAL", "buy", 1.0, 50000.0);
//    OrderbookServer server(api);
//    server.run(9002);
//}