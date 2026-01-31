#include "lily/services/WebSocketManager.hpp"
#include <iostream>
#include <algorithm>

namespace lily {
    namespace services {

        WebSocketManager::WebSocketManager() : _running(false), _ping_interval_seconds(30), _pong_timeout_seconds(60), _echo_connected(false) {
            _server.init_asio();

            // Initialize Echo client
            _echo_client.init_asio();
            _echo_client.set_open_handler([this](EchoConnectionHandle conn) {
                this->on_echo_open(conn);
            });
            _echo_client.set_close_handler([this](EchoConnectionHandle conn) {
                this->on_echo_close(conn);
            });
            _echo_client.set_message_handler([this](EchoConnectionHandle conn, EchoClient::message_ptr msg) {
                this->on_echo_message(conn, msg);
            });
            _echo_client.set_fail_handler([this](EchoConnectionHandle conn) {
                this->on_echo_fail(conn);
            });
            
            // Configure logging to suppress verbose frame header messages
            _server.set_error_channels(websocketpp::log::elevel::all);
            _server.set_access_channels(websocketpp::log::alevel::connect |
                                  websocketpp::log::alevel::disconnect |
                                  websocketpp::log::alevel::app);
            
            // Suppress frame header and payload logging
            _server.clear_access_channels(websocketpp::log::alevel::frame_header);
            _server.clear_access_channels(websocketpp::log::alevel::frame_payload);
            _server.clear_access_channels(websocketpp::log::alevel::debug_handshake);
            _server.clear_access_channels(websocketpp::log::alevel::debug_close);
            
            _server.set_open_handler([this](ConnectionHandle conn) {
                this->connect(conn);
            });

            _server.set_close_handler([this](ConnectionHandle conn) {
                this->disconnect(conn);
            });

            _server.set_message_handler([this](ConnectionHandle conn, Server::message_ptr msg) {
                this->on_message(conn, msg);
            });
            
            _server.set_pong_handler([this](ConnectionHandle conn, std::string payload) {
                // Pong received, update last pong time
                _last_pong_time[conn] = std::chrono::steady_clock::now();
            });
            
            _server.set_pong_timeout_handler([this](ConnectionHandle conn, std::string payload) {
                // Close the connection on pong timeout
                std::cerr << "Pong timeout for connection, closing connection" << std::endl;
                try {
                    _server.close(conn, websocketpp::close::status::policy_violation, "Pong timeout");
                } catch (const std::exception& e) {
                    std::cerr << "Error closing connection on pong timeout: " << e.what() << std::endl;
                }
            });
        }

        WebSocketManager::~WebSocketManager() {
            stop();
            disconnect_from_echo();
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
    
    // Remove pong time tracking for this connection
    _last_pong_time.erase(conn);
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

        void WebSocketManager::on_message(const ConnectionHandle& conn, Server::message_ptr msg) {
            // Handle text messages
            if (msg->get_opcode() == websocketpp::frame::opcode::text) {
                std::string message = msg->get_payload();
                
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
            // Handle binary messages (audio data)
            else if (msg->get_opcode() == websocketpp::frame::opcode::binary) {
                const auto& payload = msg->get_payload();
                std::vector<uint8_t> binary_data(payload.begin(), payload.end());
                
                if (_binary_message_handler) {
                    _binary_message_handler(binary_data);
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
        
        void WebSocketManager::set_ping_interval(int seconds) {
            _ping_interval_seconds = seconds;
        }
        
        void WebSocketManager::set_pong_timeout(int seconds) {
            _pong_timeout_seconds = seconds;
        }
        
        void WebSocketManager::ping_clients() {
            while (_running) {
                // Sleep for the ping interval
                std::this_thread::sleep_for(std::chrono::seconds(_ping_interval_seconds));
                
                if (!_running) {
                    break;
                }
                
                // Send ping to all connected clients
                auto now = std::chrono::steady_clock::now();
                auto timeout_duration = std::chrono::seconds(_pong_timeout_seconds);
                
                for (const auto& pair : _connections) {
                    const auto& conn = pair.second;
                    
                    // Check if we've received a pong recently
                    auto last_pong_it = _last_pong_time.find(conn);
                    if (last_pong_it != _last_pong_time.end()) {
                        auto time_since_last_pong = now - last_pong_it->second;
                        if (time_since_last_pong > timeout_duration) {
                            // Client hasn't responded to ping, close connection
                            std::cerr << "Client hasn't responded to ping, closing connection" << std::endl;
                            try {
                                _server.close(conn, websocketpp::close::status::policy_violation, "No pong response");
                            } catch (const std::exception& e) {
                                std::cerr << "Error closing connection: " << e.what() << std::endl;
                            }
                            continue;
                        }
                    }
                    
                    // Send ping message
                    try {
                        _server.ping(conn, "keepalive");
                    } catch (const std::exception& e) {
                        std::cerr << "Error sending ping to client: " << e.what() << std::endl;
                    }
                }
            }
        }

        void WebSocketManager::run() {
            try {
                _server.listen(_port);
                _server.start_accept();
                
                // Start the ping thread
                _running = true;
                _ping_thread = std::thread([this]() {
                    try {
                        this->ping_clients();
                    } catch (const std::exception& e) {
                        std::cerr << "Error in ping thread: " << e.what() << std::endl;
                    } catch (...) {
                        std::cerr << "Unknown error in ping thread" << std::endl;
                    }
                });
                
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
                _running = false;

                if (_ping_thread.joinable()) {
                    _ping_thread.join();
                }

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
                    _last_pong_time.clear();
                    _server.stop();
                    _thread.join();
                }
            } catch (const std::exception& e) {
                std::cerr << "Error in WebSocketManager::stop(): " << e.what() << std::endl;
            }
        }

        // Echo client methods
        bool WebSocketManager::connect_to_echo(const std::string& echo_ws_url) {
            try {
                websocketpp::lib::error_code ec;
                EchoClient::connection_ptr con = _echo_client.get_connection(echo_ws_url, ec);
                if (ec) {
                    std::cerr << "Error creating Echo connection: " << ec.message() << std::endl;
                    return false;
                }

                _echo_connection = con->get_handle();
                _echo_client.connect(con);

                // Start Echo client thread
                _echo_thread = std::thread([this]() {
                    try {
                        _echo_client.run();
                    } catch (const std::exception& e) {
                        std::cerr << "Error in Echo client thread: " << e.what() << std::endl;
                    }
                });

                std::cout << "Connecting to Echo service at: " << echo_ws_url << std::endl;
                return true;
            } catch (const std::exception& e) {
                std::cerr << "Error connecting to Echo service: " << e.what() << std::endl;
                return false;
            }
        }

        void WebSocketManager::disconnect_from_echo() {
            try {
                if (_echo_connected) {
                    _echo_client.close(_echo_connection, websocketpp::close::status::normal, "Client disconnecting");
                    _echo_connected = false;
                }
                if (_echo_thread.joinable()) {
                    _echo_thread.join();
                }
            } catch (const std::exception& e) {
                std::cerr << "Error disconnecting from Echo service: " << e.what() << std::endl;
            }
        }

        void WebSocketManager::send_audio_to_echo(const std::vector<uint8_t>& audio_data) {
            if (!_echo_connected) {
                std::cerr << "Not connected to Echo service" << std::endl;
                return;
            }

            try {
                _echo_client.send(_echo_connection, audio_data.data(), audio_data.size(), websocketpp::frame::opcode::binary);
                std::cout << "Sent " << audio_data.size() << " bytes to Echo service" << std::endl;
            } catch (const std::exception& e) {
                std::cerr << "Error sending audio to Echo service: " << e.what() << std::endl;
            }
        }

        void WebSocketManager::set_echo_message_handler(const EchoMessageHandler& handler) {
            _echo_message_handler = handler;
        }

        void WebSocketManager::on_echo_open(EchoConnectionHandle conn) {
            std::cout << "Connected to Echo service" << std::endl;
            _echo_connected = true;
        }

        void WebSocketManager::on_echo_close(EchoConnectionHandle conn) {
            std::cout << "Disconnected from Echo service" << std::endl;
            _echo_connected = false;
        }

        void WebSocketManager::on_echo_message(EchoConnectionHandle conn, EchoClient::message_ptr msg) {
            if (msg->get_opcode() == websocketpp::frame::opcode::text) {
                std::string payload = msg->get_payload();
                try {
                    nlohmann::json message = nlohmann::json::parse(payload);
                    
                    // Check if there's a client_id to forward the message to
                    if (message.contains("client_id")) {
                        std::string client_id = message["client_id"];
                        auto it = _connections.find(client_id);
                        if (it != _connections.end()) {
                            // Forward the original payload to the client
                            _server.send(it->second, payload, websocketpp::frame::opcode::text);
                        }
                    }
                    
                    if (_echo_message_handler) {
                        _echo_message_handler(message);
                    }
                } catch (const std::exception& e) {
                    std::cerr << "Error processing Echo message: " << e.what() << std::endl;
                }
            }
        }

        void WebSocketManager::on_echo_fail(EchoConnectionHandle conn) {
            std::cerr << "Echo connection failed" << std::endl;
            _echo_connected = false;
        }
    }
}