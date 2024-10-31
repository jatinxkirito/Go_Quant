#include <websocketpp/config/asio_no_tls_client.hpp>
#include <websocketpp/client.hpp>
#include <json/json.h>
#include <iostream>
#include <thread>
#include <mutex>
#include <condition_variable>

class OrderbookClient {
    using websocket_client = websocketpp::client<websocketpp::config::asio_client>;

private:
    websocket_client client;
    websocketpp::connection_hdl connection;
    websocket_client::connection_ptr con;
    bool connected = false;
    std::mutex mutex;
    std::condition_variable cv;

    void onMessage(websocketpp::connection_hdl hdl, websocket_client::message_ptr msg) {
        try {
            Json::Value root;
            Json::Reader reader;
            reader.parse(msg->get_payload(), root);

            // Pretty print the orderbook update
            std::cout << "\nReceived orderbook update:\n";
            std::cout << "Timestamp: " << root["timestamp"].asString() << "\n";

            std::cout << "Bids:\n";
            for (const auto& bid : root["bids"]) {
                std::cout << "Price: " << bid[0].asString()
                    << ", Size: " << bid[1].asString() << "\n";
            }

            std::cout << "Asks:\n";
            for (const auto& ask : root["asks"]) {
                std::cout << "Price: " << ask[0].asString()
                    << ", Size: " << ask[1].asString() << "\n";
            }
            std::cout << "------------------------\n";
        }
        catch (const std::exception& e) {
            std::cerr << "Error processing message: " << e.what() << std::endl;
        }
    }

    void onOpen(websocketpp::connection_hdl hdl) {
        std::cout << "Connection established\n";
        {
            std::lock_guard<std::mutex> lock(mutex);
            connected = true;
            connection = hdl;
            con = client.get_con_from_hdl(hdl);
        }
        cv.notify_one();
    }

    void onFail(websocketpp::connection_hdl hdl) {
        std::cout << "Connection failed\n";
        connected = false;
    }

    void onClose(websocketpp::connection_hdl hdl) {
        std::cout << "Connection closed\n";
        connected = false;
    }

public:
    OrderbookClient() {
        client.set_access_channels(websocketpp::log::alevel::none);
        client.clear_access_channels(websocketpp::log::alevel::frame_payload);

        client.init_asio();

        client.set_message_handler(std::bind(&OrderbookClient::onMessage, this,
            std::placeholders::_1, std::placeholders::_2));
        client.set_open_handler(std::bind(&OrderbookClient::onOpen, this,
            std::placeholders::_1));
        client.set_fail_handler(std::bind(&OrderbookClient::onFail, this,
            std::placeholders::_1));
        client.set_close_handler(std::bind(&OrderbookClient::onClose, this,
            std::placeholders::_1));
    }

    bool connect(const std::string& uri) {
        websocketpp::lib::error_code ec;
        websocket_client::connection_ptr connection = client.get_connection(uri, ec);

        if (ec) {
            std::cout << "Could not create connection: " << ec.message() << std::endl;
            return false;
        }

        client.connect(connection);

        // Start the ASIO io_service run loop
        std::thread([this]() {
            try {
                client.run();
            }
            catch (const std::exception& e) {
                std::cerr << "Error in client run loop: " << e.what() << std::endl;
            }
            }).detach();

        // Wait for connection
        std::unique_lock<std::mutex> lock(mutex);
        if (!cv.wait_for(lock, std::chrono::seconds(5), [this]() { return connected; })) {
            std::cout << "Connection timeout\n";
            return false;
        }

        return true;
    }

    void subscribe(const std::string& symbol) {
        if (!connected) {
            std::cout << "Not connected\n";
            return;
        }

        Json::Value message;
        message["action"] = "subscribe";
        message["symbol"] = symbol;

        try {
            client.send(connection, message.toStyledString(), websocketpp::frame::opcode::text);
            std::cout << "Subscribed to " << symbol << std::endl;
        }
        catch (const std::exception& e) {
            std::cerr << "Error sending subscribe message: " << e.what() << std::endl;
        }
    }

    void unsubscribe(const std::string& symbol) {
        if (!connected) {
            std::cout << "Not connected\n";
            return;
        }

        Json::Value message;
        message["action"] = "unsubscribe";
        message["symbol"] = symbol;

        try {
            client.send(connection, message.toStyledString(), websocketpp::frame::opcode::text);
            std::cout << "Unsubscribed from " << symbol << std::endl;
        }
        catch (const std::exception& e) {
            std::cerr << "Error sending unsubscribe message: " << e.what() << std::endl;
        }
    }

    void close() {
        if (connected && con) {
            con->close(websocketpp::close::status::normal, "Closing connection");
        }
    }
};