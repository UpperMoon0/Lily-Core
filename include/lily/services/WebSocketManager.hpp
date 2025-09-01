#ifndef LILY_SERVICES_WEBSOCKETMANAGER_HPP
#define LILY_SERVICES_WEBSOCKETMANAGER_HPP

#include <vector>
#include <string>
#include <memory>
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
            void run(uint16_t port);
            void stop();

        private:
            Server _server;
            MessageHandler _message_handler;
            std::vector<ConnectionHandle> _connections;
            std::thread _thread;
        };
    }
}

#endif // LILY_SERVICES_WEBSOCKETMANAGER_HPP