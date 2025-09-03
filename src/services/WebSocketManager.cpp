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

        void WebSocketManager::connect(const ConnectionHandle& /*conn*/) {
            std::cout << "Client connected." << std::endl;
            // We don't know the user_id yet, so we can't add it to the map here.
            // We'll handle it in the on_message function.
        }

        void WebSocketManager::disconnect(const ConnectionHandle& conn) {
            std::cout << "Client disconnected." << std::endl;
            // Find and remove the connection from the map
            for (auto it = _connections.begin(); it != _connections.end(); ++it) {
                if (!it->second.owner_before(conn) && !conn.owner_before(it->second)) {
                    _connections.erase(it);
                    break;
                }
            }
        }

        void WebSocketManager::broadcast(const std::string& message) {
            for (const auto& pair : _connections) {
                _server.send(pair.second, message, websocketpp::frame::opcode::text);
            }
        }

        void WebSocketManager::broadcast_binary(const std::vector<uint8_t>& data) {
            for (const auto& pair : _connections) {
                _server.send(pair.second, data.data(), data.size(), websocketpp::frame::opcode::binary);
            }
        }

        void WebSocketManager::send_binary_to_client(const ConnectionHandle& conn, const std::vector<uint8_t>& data) {
            _server.send(conn, data.data(), data.size(), websocketpp::frame::opcode::binary);
        }

        void WebSocketManager::send_binary_to_client_by_id(const std::string& client_id, const std::vector<uint8_t>& data) {
            auto it = _connections.find(client_id);
            if (it != _connections.end()) {
                send_binary_to_client(it->second, data);
            } else {
                std::cerr << "Client not found: " << client_id << std::endl;
            }
        }

        void WebSocketManager::on_message(const ConnectionHandle& conn, const std::string& message) {
            // Check for a registration message
            if (message.rfind("register:", 0) == 0) {
                std::string user_id = message.substr(9);
                _connections[user_id] = conn;
                std::cout << "Registered client with user_id: " << user_id << std::endl;
            } else {
                if (_message_handler) {
                    _message_handler(message);
                }
            }
        }
        void WebSocketManager::set_message_handler(const MessageHandler& handler) {
            _message_handler = handler;
        }

        void WebSocketManager::set_port(uint16_t port) {
            _port = port;
        }

        void WebSocketManager::run() {
            try {
                _server.listen(_port);
                _server.start_accept();
                _thread = std::thread([this]() { _server.run(); });
            } catch (const std::exception& e) {
                std::cerr << "Error in WebSocketManager: " << e.what() << std::endl;
            }
        }

        void WebSocketManager::stop() {
            if (_thread.joinable()) {
                _server.stop_listening();
                for (const auto& pair : _connections) {
                    _server.close(pair.second, websocketpp::close::status::going_away, "Server shutting down");
                }
                _server.stop();
                _thread.join();
            }
        }
    }
}