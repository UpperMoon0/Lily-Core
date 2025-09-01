#include "lily/services/TTSService.hpp"
#include <iostream>
#include <nlohmann/json.hpp>
#include <cpprest/ws_client.h>
#include <cpprest/json.h>
#include <cpprest/uri_builder.h>
#include <cpprest/containerstream.h>
#include <chrono>
#include <thread>

using namespace web;
using namespace web::websockets::client;

namespace lily {
    namespace services {
        TTSService::TTSService() : _is_connected(false) {
            std::cout << "TTSService initialized." << std::endl;
        }

        TTSService::~TTSService() {
            if (_is_connected) {
                close();
            }
        }

        bool TTSService::connect(const std::string& provider_url) {
            std::cout << "Connecting to TTS provider at " << provider_url << std::endl;
            _provider_url = provider_url;
            _is_connected = initialize_websocket();
            return _is_connected;
        }

        bool TTSService::initialize_websocket() {
            try {
                // Create WebSocket client
                _websocket_client = std::make_unique<websocket_client>();
                
                // Parse the provider URL to extract host and port
                web::uri provider_uri(utility::conversions::to_string_t(_provider_url));
                web::uri_builder builder(provider_uri);
                if (provider_uri.scheme().empty()) {
                    builder.set_scheme(U("ws"));
                }
                if (provider_uri.port() == 0) {
                    builder.set_port(9000);
                }
                auto task = _websocket_client->connect(builder.to_uri());
                task.wait();
                
                std::cout << "WebSocket connection established." << std::endl;
                return true;
            } catch (const std::exception& e) {
                std::cerr << "Error initializing WebSocket: " << e.what() << std::endl;
                return false;
            }
        }

        std::vector<uint8_t> TTSService::synthesize_speech(const std::string& text, const TTSParameters& params) {
            if (!_is_connected || !_websocket_client) {
                std::cerr << "TTS service not connected." << std::endl;
                return {};
            }

            try {
                // Create JSON request
                nlohmann::json request;
                request["text"] = text;
                request["speaker"] = params.speaker;
                request["sample_rate"] = params.sample_rate;
                request["model"] = params.model;
                request["lang"] = params.lang;

                // Send request
                std::string request_str = request.dump();
                websocket_outgoing_message msg;
                msg.set_utf8_message(request_str);
                
                auto send_task = _websocket_client->send(msg);
                send_task.wait();

                // Wait for response
                auto receive_task = _websocket_client->receive();
                receive_task.wait();
                
                auto response_msg = receive_task.get();
                if (response_msg.message_type() == websocket_message_type::text_message) {
                    // First message should be metadata
                    std::string metadata_str = response_msg.extract_string().get();
                    nlohmann::json metadata = nlohmann::json::parse(metadata_str);
                    
                    if (metadata.value("status", "") == "success") {
                        // Wait for audio data
                        auto audio_task = _websocket_client->receive();
                        audio_task.wait();
                        
                        auto audio_msg = audio_task.get();
                        if (audio_msg.message_type() == websocket_message_type::binary_message) {
                            // Get binary data
                            auto audio_buffer = audio_msg.body();
                            concurrency::streams::container_buffer<std::vector<uint8_t>> buffer;
                            audio_buffer.read_to_end(buffer).get();
                            std::vector<uint8_t> audio_data = buffer.collection();
                            
                            std::cout << "Received " << audio_data.size() << " bytes of audio data." << std::endl;
                            return audio_data;
                        } else if (audio_msg.message_type() == websocket_message_type::text_message) {
                            // Handle error message
                            std::string error_str = audio_msg.extract_string().get();
                            nlohmann::json error_json = nlohmann::json::parse(error_str);
                            std::cerr << "TTS Error: " << error_json.value("message", "Unknown error") << std::endl;
                        }
                    } else {
                        std::cerr << "TTS Request failed: " << metadata.value("message", "Unknown error") << std::endl;
                    }
                } else if (response_msg.message_type() == websocket_message_type::binary_message) {
                    // Handle case where we get binary data directly
                    auto audio_buffer = response_msg.body();
                    concurrency::streams::container_buffer<std::vector<uint8_t>> buffer;
                    audio_buffer.read_to_end(buffer).get();
                    std::vector<uint8_t> audio_data = buffer.collection();
                    
                    std::cout << "Received " << audio_data.size() << " bytes of audio data." << std::endl;
                    return audio_data;
                }
            } catch (const std::exception& e) {
                std::cerr << "Error synthesizing speech: " << e.what() << std::endl;
            }
            
            return {};
        }

        void TTSService::close() {
            if (_websocket_client) {
                try {
                    auto task = _websocket_client->close();
                    task.wait();
                    std::cout << "WebSocket connection closed." << std::endl;
                } catch (const std::exception& e) {
                    std::cerr << "Error closing WebSocket connection: " << e.what() << std::endl;
                }
                _websocket_client.reset();
            }
            _is_connected = false;
        }
    }
}