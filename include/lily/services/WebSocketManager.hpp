#ifndef LILY_SERVICES_WEBSOCKETMANAGER_HPP
#define LILY_SERVICES_WEBSOCKETMANAGER_HPP

#include <vector>
#include <string>
#include <memory>
#include <map>
#include <websocketpp/server.hpp>
#include <websocketpp/config/asio_no_tls.hpp>
#include <functional>
#include <thread>

namespace lily {
        
    namespace services {
        // Using a placeholder for the connection handle for now.
        using Server = websocketpp::server<websocketpp::config::asio>;
        using ConnectionHandle = websocketpp::connection_hdl;
        using MessageHandler = std::function<void(const std::string&)>;

        class WebSocketManager {
        public:
            WebSocketManager();
            ~WebSocketManager();
            void on_message(const ConnectionHandle& conn, const std::string& message);

            void connect(const ConnectionHandle& conn);
            void disconnect(const ConnectionHandle& conn);
            void set_message_handler(const MessageHandler& handler);
            void broadcast(const std::string& message);
            void broadcast_binary(const std::vector<uint8_t>& data);
            void send_binary_to_client(const ConnectionHandle& conn, const std::vector<uint8_t>& data);
            void send_binary_to_client_by_id(const std::string& client_id, const std::vector<uint8_t>& data);
            void run();
            void stop();
            void set_port(uint16_t port);

        private:
            Server _server;
            MessageHandler _message_handler;
            std::map<std::string, ConnectionHandle> _connections;
            std::thread _thread;
            uint16_t _port;
        };
    }
}

#endif // LILY_SERVICES_WEBSOCKETMANAGER_HPP