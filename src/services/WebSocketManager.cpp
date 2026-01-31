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
                if (msg->get_opcode() == websocketpp::frame::opcode::binary) {
                    std::string payload_str = msg->get_payload();
                    std::vector<uint8_t> payload(payload_str.begin(), payload_str.end());
                    this->on_message_binary(conn, payload);
                } else {
                    this->on_message(conn, msg->get_payload());
                }
            });
            
            _server.set_pong_handler([this](ConnectionHandle conn, std::string payload) {
                // Pong received, no need for debug log
            });
            
            _server.set_pong_timeout_handler([this](ConnectionHandle conn, std::string payload) {
                // Don't close the connection immediately on pong timeout
                // Let the client handle reconnection if needed
                // Just log the timeout for debugging purposes
            });
        }

        WebSocketManager::~WebSocketManager() {
            stop();
        }

        void WebSocketManager::connect(const ConnectionHandle& conn) {
            // We don't know the user_id yet, so we can't add it to the map here.
            // We'll handle it in the on_message function.
        }
void WebSocketManager::disconnect(const ConnectionHandle& conn) {
    auto it = _connection_to_user.find(conn);
    if (it != _connection_to_user.end()) {
        std::string user_id = it->second;
        _connections.erase(user_id);
        _connection_to_user.erase(it);
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
            try {
                _server.send(conn, data.data(), data.size(), websocketpp::frame::opcode::binary);
            } catch (const std::exception& e) {
                std::cerr << "Error in send_binary_to_client: " << e.what() << std::endl;
                throw;
            }
        }

        void WebSocketManager::send_binary_to_client_by_id(const std::string& client_id, const std::vector<uint8_t>& data) {
            auto it = _connections.find(client_id);
            if (it != _connections.end()) {
                try {
                    send_binary_to_client(it->second, data);
                } catch (const std::exception& e) {
                    std::cerr << "Error sending binary data to client_id: " << client_id << ", error: " << e.what() << std::endl;
                }
            } else {
                std::cerr << "Client not found: " << client_id << std::endl;
            }
        }
        
        bool WebSocketManager::is_connection_registered(const std::string& client_id) {
            // Check if the client_id is in the _connections map
            return _connections.find(client_id) != _connections.end();
        }
        
        bool WebSocketManager::wait_for_connection_registration(const std::string& client_id, int timeout_seconds) {
            // Check if already registered
            if (is_connection_registered(client_id)) {
                return true;
            }
            
            // Wait for registration with timeout
            int wait_time = 0;
            const int check_interval_ms = 100; // Check every 100ms
            const int max_wait_time_ms = timeout_seconds * 1000; // Convert to milliseconds
            
            while (wait_time < max_wait_time_ms) {
                // Check if registered
                if (is_connection_registered(client_id)) {
                    return true;
                }
                
                // Sleep for check_interval_ms
                std::this_thread::sleep_for(std::chrono::milliseconds(check_interval_ms));
                wait_time += check_interval_ms;
            }
            
            // Timeout reached
            std::cerr << "Timeout waiting for client_id " << client_id << " to register" << std::endl;
            return false;
        }
        
        bool WebSocketManager::is_connection_alive(const ConnectionHandle& conn) {
            // Check if the connection exists in our maps
            auto it = _connection_to_user.find(conn);
            if (it == _connection_to_user.end()) {
                return false;
            }
            
            // Check if the connection is still valid using websocketpp
            // This is a basic check - in a more robust implementation,
            // you might want to send a ping and wait for a pong
            try {
                // Get the transport connection to check its state
                // Note: This is a simplified check and might not work in all cases
                return true;
            } catch (const std::exception& e) {
                std::cerr << "Error checking connection health: " << e.what() << std::endl;
                return false;
            }
        }

        std::vector<std::string> WebSocketManager::get_connected_user_ids() {
            std::vector<std::string> user_ids;
            for (const auto& pair : _connections) {
                user_ids.push_back(pair.first);
            }
            return user_ids;
        }

        void WebSocketManager::on_message(const ConnectionHandle& conn, const std::string& message) {
            // Handle ping messages
            if (message == "ping") {
                try {
                    _server.send(conn, "pong", websocketpp::frame::opcode::text);
                } catch (const std::exception& e) {
                    std::cerr << "Error sending pong response: " << e.what() << std::endl;
                }
                return;
            }
            
            // Check for a registration message
            if (message.rfind("register:", 0) == 0) {
                std::string user_id = message.substr(9);
                _connections[user_id] = conn;
                _connection_to_user[conn] = user_id;
                
                // Send registration confirmation back to the client
                try {
                    _server.send(conn, "registered", websocketpp::frame::opcode::text);
                } catch (const std::exception& e) {
                    std::cerr << "Error sending registration confirmation: " << e.what() << std::endl;
                }
            } else {
                if (_message_handler) {
                    _message_handler(message);
                }
            }
        }

        void WebSocketManager::on_message_binary(const ConnectionHandle& conn, const std::vector<uint8_t>& message) {
            // Check if connection is associated with a user
            auto it = _connection_to_user.find(conn);
            std::string user_id = (it != _connection_to_user.end()) ? it->second : "";

            if (_binary_message_handler) {
                _binary_message_handler(message, user_id);
            }
        }

        void WebSocketManager::set_message_handler(const MessageHandler& handler) {
            _message_handler = handler;
        }

        void WebSocketManager::set_binary_message_handler(const BinaryMessageHandler& handler) {
            _binary_message_handler = handler;
        }

        void WebSocketManager::set_port(uint16_t port) {
            _port = port;
        }

        void WebSocketManager::run() {
            try {
                _server.listen(_port);
                _server.start_accept();
                _thread = std::thread([this]() {
                    try {
                        _server.run();
                    } catch (const std::exception& e) {
                        std::cerr << "Error in WebSocket server thread: " << e.what() << std::endl;
                    } catch (...) {
                        std::cerr << "Unknown error in WebSocket server thread" << std::endl;
                    }
                });
            } catch (const std::exception& e) {
                std::cerr << "Error in WebSocketManager: " << e.what() << std::endl;
            }
        }

        void WebSocketManager::stop() {
            try {
                if (_thread.joinable()) {
                    _server.stop_listening();
                    // Close all connections
                    for (const auto& pair : _connections) {
                        try {
                            _server.close(pair.second, websocketpp::close::status::going_away, "Server shutting down");
                        } catch (const std::exception& e) {
                            std::cerr << "Error closing connection for user " << pair.first << ": " << e.what() << std::endl;
                        }
                    }
                    // Clear connection maps
                    _connections.clear();
                    _connection_to_user.clear();
                    _server.stop();
                    _thread.join();
                }
            } catch (const std::exception& e) {
                std::cerr << "Error in WebSocketManager::stop(): " << e.what() << std::endl;
            }
        }
    }
}