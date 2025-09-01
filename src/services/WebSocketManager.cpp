#include "lily/services/WebSocketManager.hpp"
#include <iostream>
#include <algorithm>

namespace lily {
    namespace services {

        WebSocketManager::WebSocketManager() {
            _server.init_asio();
            _server.set_open_handler([this](ConnectionHandle conn) {
                this->connect(conn);
            });

            _server.set_close_handler([this](ConnectionHandle conn) {
                this->disconnect(conn);
            });

            _server.set_message_handler([this](ConnectionHandle conn, Server::message_ptr msg) {
                this->on_message(conn, msg->get_payload());
            });
            std::cout << "WebSocketManager initialized." << std::endl;
        }

        WebSocketManager::~WebSocketManager() {
            stop();
            std::cout << "WebSocketManager shutting down." << std::endl;
        }

        void WebSocketManager::connect(const ConnectionHandle& conn) {
            // For now, we just log the connection.
            // In a real implementation, we would store the connection pointer.
            std::cout << "Client connected." << std::endl;
            _connections.push_back(conn);
        }

        void WebSocketManager::disconnect(const ConnectionHandle& conn) {
            // For now, we just log the disconnection.
            // In a real implementation, we would find and remove the connection pointer.
            std::cout << "Client disconnected." << std::endl;
            // The following line requires a proper comparison for weak_ptr, which is complex.
            // For this placeholder, we'll just clear the list if it gets too large.
            if (_connections.size() > 100) {
                _connections.clear();
            }
        }

        void WebSocketManager::broadcast(const std::string& message) {
            std::cout << "Broadcasting message: " << message << std::endl;
            // In a real implementation, we would iterate over _connections
            // and send the message to each client.
        }

        void WebSocketManager::on_message(const ConnectionHandle& conn, const std::string& message) {
            if (_message_handler) {
                _message_handler(message);
            }
        }
        void WebSocketManager::set_message_handler(const MessageHandler& handler) {
            _message_handler = handler;
        }

        void WebSocketManager::run(uint16_t port) {
            try {
                _server.listen(port);
                _server.start_accept();
                _thread = std::thread([this]() { _server.run(); });
            } catch (const std::exception& e) {
                std::cerr << "Error in WebSocketManager: " << e.what() << std::endl;
            }
        }

        void WebSocketManager::stop() {
            if (_thread.joinable()) {
                _server.stop_listening();
                for (const auto& conn : _connections) {
                    _server.close(conn, websocketpp::close::status::going_away, "Server shutting down");
                }
                _server.stop();
                _thread.join();
            }
        }
    }
}