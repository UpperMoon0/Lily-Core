#ifndef LILY_SERVICES_GATEWAY_SERVICE_HPP
#define LILY_SERVICES_GATEWAY_SERVICE_HPP

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
#include <mutex>
#include <nlohmann/json.hpp>

#include "lily/controller/ChatController.hpp"
#include "lily/controller/SystemController.hpp"
#include "lily/controller/SessionController.hpp"

// Forward declarations
namespace lily {
    namespace config {
        class AppConfig;
    }
    namespace services {
        class ChatService;
        class MemoryService; // No longer needed directly here?
        class Service; // ToolService
        class AgentLoopService; // No longer needed directly here?
        class SessionService;
    }
}

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

        class GatewayService {
        public:
            GatewayService();
            ~GatewayService();
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

            // Dependency injection for HTTP Controllers
            void set_controllers(
                std::shared_ptr<controller::ChatController> chat_controller,
                std::shared_ptr<controller::SystemController> system_controller,
                std::shared_ptr<controller::SessionController> session_controller
            );

            // Dependency injection for WebSocket handling (Legacy/Direct)
            void set_dependencies(
                std::shared_ptr<ChatService> chat_service,
                std::shared_ptr<SessionService> session_service,
                config::AppConfig& config
            );

        private:
            Server _server;
            
            // Controllers
            std::shared_ptr<controller::ChatController> _chat_controller;
            std::shared_ptr<controller::SystemController> _system_controller;
            std::shared_ptr<controller::SessionController> _session_controller;

            // Dependencies for WebSocket
            std::shared_ptr<ChatService> _chat_service;
            std::shared_ptr<SessionService> _session_service;
            config::AppConfig* _config = nullptr;

            // HTTP Handler
            void on_http(ConnectionHandle hdl);

            MessageHandler _message_handler;
            BinaryMessageHandler _binary_message_handler;
            std::map<std::string, ConnectionHandle> _connections;
            std::map<ConnectionHandle, std::string, std::owner_less<ConnectionHandle>> _connection_to_user;
            std::map<ConnectionHandle, std::chrono::steady_clock::time_point, std::owner_less<ConnectionHandle>> _last_pong_time;
            std::recursive_mutex _mutex;
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

#endif // LILY_SERVICES_GATEWAY_SERVICE_HPP

