#include "lily/services/WebSocketManager.hpp"
#include <iostream>
#include <algorithm>

namespace lily {
    namespace services {

        WebSocketManager::WebSocketManager() {
            _server.init_asio();
            _server.set_open_handler([this](ConnectionHandle conn) {
                std::cout << "DEBUG: WebSocket connection opened." << std::endl;
                this->connect(conn);
            });

            _server.set_close_handler([this](ConnectionHandle conn) {
                std::cout << "DEBUG: WebSocket connection closed." << std::endl;
                this->disconnect(conn);
            });

            _server.set_message_handler([this](ConnectionHandle conn, Server::message_ptr msg) {
                std::cout << "DEBUG: WebSocket message received." << std::endl;
                this->on_message(conn, msg->get_payload());
            });
            
            _server.set_pong_handler([this](ConnectionHandle conn, std::string payload) {
                std::cout << "DEBUG: WebSocket pong received." << std::endl;
            });
            
            _server.set_pong_timeout_handler([this](ConnectionHandle conn, std::string payload) {
                std::cout << "DEBUG: WebSocket pong timeout." << std::endl;
                // Don't close the connection immediately on pong timeout
                // Let the client handle reconnection if needed
                // Just log the timeout for debugging purposes
            });
            
            std::cout << "WebSocketManager initialized." << std::endl;
        }

        WebSocketManager::~WebSocketManager() {
            stop();
            std::cout << "WebSocketManager shutting down." << std::endl;
        }

        void WebSocketManager::connect(const ConnectionHandle& conn) {
            std::cout << "DEBUG: Client connected. Current connections count: " << _connections.size() << std::endl;
            // We don't know the user_id yet, so we can't add it to the map here.
            // We'll handle it in the on_message function.
            // Log connection handle for debugging
            std::cout << "DEBUG: Connection handle: " << &conn << std::endl;
            // Log all current connections
            std::cout << "DEBUG: Current connections in _connection_to_user map:" << std::endl;
            for (const auto& pair : _connection_to_user) {
                std::cout << "DEBUG:   User ID: " << pair.second << ", Conn Handle: " << &(pair.first) << std::endl;
            }
        }

        void WebSocketManager::disconnect(const ConnectionHandle& conn) {
            std::cout << "DEBUG: Client disconnecting. Current connections count: " << _connections.size() << std::endl;
            // Log connection handle for debugging
            std::cout << "DEBUG: Disconnecting connection handle: " << &conn << std::endl;
            // Find and remove the connection from the maps
            auto it = _connection_to_user.find(conn);
            if (it != _connection_to_user.end()) {
                std::string user_id = it->second;
                std::cout << "DEBUG: Removing client with user_id: " << user_id << std::endl;
                _connections.erase(user_id);
                _connection_to_user.erase(it);
                std::cout << "DEBUG: Client removed. Remaining connections count: " << _connections.size() << std::endl;
            } else {
                std::cout << "DEBUG: Disconnecting client not found in connection_to_user map" << std::endl;
                // Debug: Print all connections
                std::cout << "DEBUG: Current connections in _connection_to_user map:" << std::endl;
                for (const auto& pair : _connection_to_user) {
                    std::cout << "DEBUG:   User ID: " << pair.second << ", Conn Handle: " << &(pair.first) << std::endl;
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
            try {
                _server.send(conn, data.data(), data.size(), websocketpp::frame::opcode::binary);
            } catch (const std::exception& e) {
                std::cerr << "DEBUG: Error in send_binary_to_client: " << e.what() << std::endl;
                throw;
            }
        }

        void WebSocketManager::send_binary_to_client_by_id(const std::string& client_id, const std::vector<uint8_t>& data) {
            std::cout << "DEBUG: Attempting to send binary data to client_id: " << client_id << std::endl;
            std::cout << "DEBUG: Current connections count: " << _connections.size() << std::endl;
            // Debug: Print all connections
            std::cout << "DEBUG: Current connections in _connections map:" << std::endl;
            for (const auto& pair : _connections) {
                std::cout << "DEBUG:   User ID: " << pair.first << std::endl;
            }
            auto it = _connections.find(client_id);
            if (it != _connections.end()) {
                std::cout << "DEBUG: Found client, sending binary data (" << data.size() << " bytes)" << std::endl;
                try {
                    send_binary_to_client(it->second, data);
                    std::cout << "DEBUG: Successfully sent binary data to client_id: " << client_id << std::endl;
                } catch (const std::exception& e) {
                    std::cerr << "DEBUG: Error sending binary data to client_id: " << client_id << ", error: " << e.what() << std::endl;
                }
            } else {
                std::cerr << "Client not found: " << client_id << std::endl;
                // Additional debug info
                std::cerr << "DEBUG: Searched for client_id: " << client_id << " but not found in _connections map" << std::endl;
                std::cerr << "DEBUG: _connections map size: " << _connections.size() << std::endl;
                // Debug: Print all connections in _connection_to_user map
                std::cerr << "DEBUG: Current connections in _connection_to_user map:" << std::endl;
                for (const auto& pair : _connection_to_user) {
                    std::cerr << "DEBUG:   User ID: " << pair.second << ", Conn Handle: " << &(pair.first) << std::endl;
                }
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
            std::cerr << "DEBUG: Timeout waiting for client_id " << client_id << " to register" << std::endl;
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
                std::cerr << "DEBUG: Error checking connection health: " << e.what() << std::endl;
                return false;
            }
        }

        void WebSocketManager::on_message(const ConnectionHandle& conn, const std::string& message) {
            std::cout << "DEBUG: Received message from connection handle: " << &conn << ", message: " << message << std::endl;
            
            // Handle ping messages
            if (message == "ping") {
                try {
                    _server.send(conn, "pong", websocketpp::frame::opcode::text);
                    std::cout << "DEBUG: Sent pong response to ping." << std::endl;
                } catch (const std::exception& e) {
                    std::cerr << "DEBUG: Error sending pong response: " << e.what() << std::endl;
                }
                return;
            }
            
            // Check for a registration message
            if (message.rfind("register:", 0) == 0) {
                std::string user_id = message.substr(9);
                std::cout << "DEBUG: Registering client with user_id: " << user_id << std::endl;
                std::cout << "DEBUG: Current connections count before registration: " << _connections.size() << std::endl;
                _connections[user_id] = conn;
                _connection_to_user[conn] = user_id;
                std::cout << "DEBUG: Registered client with user_id: " << user_id << std::endl;
                
                // Send registration confirmation back to the client
                try {
                    _server.send(conn, "registered", websocketpp::frame::opcode::text);
                    std::cout << "DEBUG: Sent registration confirmation to user_id: " << user_id << std::endl;
                } catch (const std::exception& e) {
                    std::cerr << "DEBUG: Error sending registration confirmation: " << e.what() << std::endl;
                }
                std::cout << "DEBUG: Current connections count after registration: " << _connections.size() << std::endl;
                // Debug: Print all connections
                std::cout << "DEBUG: Current connections in _connections map after registration:" << std::endl;
                for (const auto& pair : _connections) {
                    std::cout << "DEBUG:   User ID: " << pair.first << ", Conn Handle: " << &(pair.second) << std::endl;
                }
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