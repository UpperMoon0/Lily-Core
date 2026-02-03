#ifndef LILY_SERVICES_WEBSOCKETMANAGER_HPP
#define LILY_SERVICES_WEBSOCKETMANAGER_HPP

#include <vector>
#include <string>
#include <memory>
#include <map>
#include <websocketpp/server.hpp>
#include <websocketpp/client.hpp>
#include <websocketpp/config/asio_no_tls.hpp>
#include <functional>
#include <thread>
#include <chrono>
#include <atomic>
#include <nlohmann/json.hpp>

namespace lily {
        
    namespace services {
        // Using a placeholder for the connection handle for now.
        using Server = websocketpp::server<websocketpp::config::asio>;
        using ConnectionHandle = websocketpp::connection_hdl;
        using MessageHandler = std::function<void(const std::string&)>;
        using BinaryMessageHandler = std::function<void(const std::vector<uint8_t>&, const std::string&)>;

        // Echo service WebSocket client
        using EchoClient = websocketpp::client<websocketpp::config::asio>;
        using EchoConnectionHandle = websocketpp::connection_hdl;
        using EchoMessageHandler = std::function<void(const nlohmann::json&)>;

        class WebSocketManager {
        public:
            WebSocketManager();
            ~WebSocketManager();
            void on_message(const ConnectionHandle& conn, Server::message_ptr msg);

            void connect(const ConnectionHandle& conn);
            void disconnect(const ConnectionHandle& conn);
            void set_message_handler(const MessageHandler& handler);
            void set_binary_message_handler(const BinaryMessageHandler& handler);
            void broadcast(const std::string& message);
            void broadcast_binary(const std::vector<uint8_t>& data);
            void send_binary_to_client(const ConnectionHandle& conn, const std::vector<uint8_t>& data);
            void send_binary_to_client_by_id(const std::string& client_id, const std::vector<uint8_t>& data);
            void send_text_to_client_by_id(const std::string& client_id, const std::string& message);
            bool is_connection_registered(const std::string& client_id);
            bool wait_for_connection_registration(const std::string& client_id, int timeout_seconds = 5);
            bool is_connection_alive(const ConnectionHandle& conn);
            std::vector<std::string> get_connected_user_ids();
            void run();
            void stop();
            void set_port(uint16_t port);
            void set_ping_interval(int seconds);
            void set_pong_timeout(int seconds);

            // Echo service WebSocket client methods
            bool connect_to_echo(const std::string& echo_ws_url);
            void disconnect_from_echo();
            void send_audio_to_echo(const std::vector<uint8_t>& audio_data);
            void set_echo_message_handler(const EchoMessageHandler& handler);

        private:
            Server _server;
            MessageHandler _message_handler;
            BinaryMessageHandler _binary_message_handler;
            std::map<std::string, ConnectionHandle> _connections;
            std::map<ConnectionHandle, std::string, std::owner_less<ConnectionHandle>> _connection_to_user;
            std::map<ConnectionHandle, std::chrono::steady_clock::time_point, std::owner_less<ConnectionHandle>> _last_pong_time;
            std::thread _thread;
            std::thread _ping_thread;
            std::atomic<bool> _running;
            uint16_t _port;
            int _ping_interval_seconds;
            int _pong_timeout_seconds;
            void ping_clients();

            // Echo client members
            EchoClient _echo_client;
            EchoConnectionHandle _echo_connection;
            EchoMessageHandler _echo_message_handler;
            std::thread _echo_thread;
            std::atomic<bool> _echo_connected;

            // Echo client event handlers
            void on_echo_open(EchoConnectionHandle conn);
            void on_echo_close(EchoConnectionHandle conn);
            void on_echo_message(EchoConnectionHandle conn, EchoClient::message_ptr msg);
            void on_echo_fail(EchoConnectionHandle conn);
        };
    }
}

#endif // LILY_SERVICES_WEBSOCKETMANAGER_HPP
