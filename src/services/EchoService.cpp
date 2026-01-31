#include "lily/services/EchoService.hpp"
#include <iostream>
#include <nlohmann/json.hpp>
#include <cpprest/uri_builder.h>
#include <cpprest/interopstream.h>
#include <cpprest/containerstream.h>

using namespace web;
using namespace web::websockets::client;

namespace lily {
    namespace services {

        EchoService::EchoService() : _is_connected(false) {}

        EchoService::~EchoService() {
            close();
        }

        bool EchoService::connect(const std::string& provider_url) {
            try {
                if (_is_connected) {
                    return true;
                }

                _provider_url = provider_url;
                
                // Construct WebSocket URL
                web::uri provider_uri(utility::conversions::to_string_t(_provider_url));
                web::uri_builder builder(provider_uri);
                
                // Ensure scheme is ws or wss
                std::string scheme = utility::conversions::to_utf8string(provider_uri.scheme());
                if (scheme == "http") {
                    builder.set_scheme(U("ws"));
                } else if (scheme == "https") {
                    builder.set_scheme(U("wss"));
                }
                
                // Ensure port is set (default to 8000 if 0 or missing from simple http url)
                if (provider_uri.port() == 0) {
                    builder.set_port(8000);
                }

                // Append path
                builder.set_path(U("/ws/transcribe"));

                websocket_client_config config;
                config.set_validate_certificates(false);
                _websocket_client = std::make_unique<websocket_client>(config);

                _websocket_client->connect(builder.to_uri()).wait();
                _is_connected = true;
                std::cout << "Connected to Echo service at " << utility::conversions::to_utf8string(builder.to_string()) << std::endl;

                // Start receive loop
                _receive_thread = std::thread(&EchoService::receive_loop, this);

                return true;

            } catch (const std::exception& e) {
                std::cerr << "Failed to connect to Echo service: " << e.what() << std::endl;
                _is_connected = false;
                return false;
            }
        }

        void EchoService::close() {
            _is_connected = false;
            if (_websocket_client) {
                try {
                    _websocket_client->close().wait();
                } catch (...) {}
            }
            if (_receive_thread.joinable()) {
                _receive_thread.join();
            }
            _websocket_client.reset();
        }

        void EchoService::send_audio(const std::vector<uint8_t>& audio_data) {
            if (!_is_connected || !_websocket_client) {
                return;
            }

            try {
                websocket_outgoing_message msg;
                // Create a container buffer from the vector
                concurrency::streams::container_buffer<std::vector<uint8_t>> buffer(audio_data);
                msg.set_binary_message(buffer.create_istream(), audio_data.size());
                std::cout << "[DEBUG] Sending " << audio_data.size() << " bytes to Echo (connected=" << _is_connected << ")" << std::endl;
                _websocket_client->send(msg).wait();
                std::cout << "[DEBUG] Sent audio chunk successfully." << std::endl;
            } catch (const std::exception& e) {
                std::cerr << "Error sending audio to Echo: " << e.what() << std::endl;
            }
        }

        void EchoService::set_transcription_handler(const TranscriptionHandler& handler) {
            _transcription_handler = handler;
        }

        void EchoService::receive_loop() {
            std::cout << "[DEBUG] EchoService receive_loop started." << std::endl;
            while (_is_connected && _websocket_client) {
                try {
                    auto msg = _websocket_client->receive().get();
                    std::cout << "[DEBUG] EchoService received message." << std::endl;
                    on_message(msg);
                } catch (const std::exception& e) {
                    if (_is_connected) {
                        std::cerr << "Error receiving from Echo: " << e.what() << std::endl;
                    }
                    break;
                }
            }
            std::cout << "[DEBUG] EchoService receive_loop ended." << std::endl;
        }

        void EchoService::on_message(websocket_incoming_message msg) {
            try {
                if (msg.message_type() == websocket_message_type::text_message) {
                    std::string payload = msg.extract_string().get();
                    std::cout << "[DEBUG] EchoService processing payload: " << payload << std::endl;
                    
                    // Parse JSON
                    auto json = nlohmann::json::parse(payload);
                    
                    // Echo sends: {'type': 'interim'|'final', 'text': '...', 'client_id': '...'}
                    if (json.contains("text") && json.contains("type")) {
                        // We forward the whole JSON string to let ChatService decide what to do
                        if (_transcription_handler) {
                            _transcription_handler(payload);
                        }
                    }
                }
            } catch (const std::exception& e) {
                std::cerr << "Error processing Echo message: " << e.what() << std::endl;
            }
        }
    }
}