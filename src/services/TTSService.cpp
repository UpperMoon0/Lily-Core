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

        bool TTSService::connect(const std::string& provider_url, const std::string& websocket_url) {
            _provider_url = provider_url; // Set the provider URL first
            _websocket_url = websocket_url; // Set the websocket URL
            const int max_retries = 5;
            const int retry_delay_ms = 2000;

            // Check readiness before attempting to connect
            if (!is_ready()) {
                std::cerr << "TTS provider is not ready." << std::endl;
                return false;
            }

            for (int i = 0; i < max_retries; ++i) {
                std::cout << "Connecting to TTS provider at " << _provider_url << " (Attempt " << i + 1 << "/" << max_retries << ")" << std::endl;
                if (initialize_websocket()) {
                    _is_connected = true;
                    return true;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(retry_delay_ms));
            }

            std::cerr << "Failed to connect to TTS provider after " << max_retries << " attempts." << std::endl;
            _is_connected = false;
            return false;
        }

        bool TTSService::initialize_websocket() {
            try {
                // Create WebSocket client with configuration
                websocket_client_config config;
                config.set_validate_certificates(false); // Disable certificate validation for internal communication
                _websocket_client = std::make_unique<websocket_client>(config);
                
                // Use the websocket_url directly if provided, otherwise transform the provider_url
                std::string url_to_use = _websocket_url.empty() ? _provider_url : _websocket_url;
                
                if (!_websocket_url.empty()) {
                    // Use the websocket_url directly
                    web::uri websocket_uri(utility::conversions::to_string_t(url_to_use));
                    auto task = _websocket_client->connect(websocket_uri);
                    task.wait();
                } else {
                    // Transform the provider URL to extract host and port (fallback for backward compatibility)
                    web::uri provider_uri(utility::conversions::to_string_t(_provider_url));
                    web::uri_builder builder(provider_uri);
                    
                    // Convert HTTP/HTTPS schemes to WebSocket schemes
                    std::string scheme = utility::conversions::to_utf8string(provider_uri.scheme());
                    if (scheme == "http") {
                        builder.set_scheme(U("ws"));
                    } else if (scheme == "https") {
                        builder.set_scheme(U("wss"));
                    }
                    
                    // Set default port if not specified
                    if (provider_uri.port() == 0) {
                        builder.set_port(9000);
                    }
                    auto task = _websocket_client->connect(builder.to_uri());
                    task.wait();
                }
                
                std::cout << "WebSocket connection established." << std::endl;
                return true;
            } catch (const std::exception& e) {
                std::cerr << "Error initializing WebSocket: " << e.what() << std::endl;
                return false;
            }
        }

        std::vector<uint8_t> TTSService::synthesize_speech(const std::string& text, const TTSParameters& params) {
            // Always ensure we have a fresh connection for each request since the server closes it after sending audio
            if (_is_connected && _websocket_client) {
                // Close existing connection if it's still open
                try {
                    close();
                } catch (const std::exception& e) {
                    std::cerr << "Error closing existing connection: " << e.what() << std::endl;
                }
            }
            
            // Establish a new connection for this request
            std::cout << "Establishing new connection for TTS request..." << std::endl;
            if (!connect(_provider_url, _websocket_url)) {
                std::cerr << "Failed to connect to TTS service." << std::endl;
                return {};
            }
        
            try {
                std::cout << "=== Starting TTS synthesis ===" << std::endl;
                
                // Create JSON request
                nlohmann::json request;
                request["text"] = text;
                request["speaker"] = params.speaker;
                request["sample_rate"] = params.sample_rate;
                request["model"] = params.model;
                request["lang"] = params.lang;
        
                std::cout << "Sending TTS request with text: " << (text.length() > 50 ? text.substr(0, 50) + "..." : text) << std::endl;
                std::cout << "Request params - Speaker: " << params.speaker << ", Sample rate: " << params.sample_rate
                          << ", Model: " << params.model << ", Lang: " << params.lang << std::endl;
        
                // Send request
                std::string request_str = request.dump();
                std::cout << "Request JSON: " << request_str << std::endl;
                
                websocket_outgoing_message msg;
                msg.set_utf8_message(request_str);
                
                std::cout << "Sending message..." << std::endl;
                auto send_task = _websocket_client->send(msg);
                std::cout << "Waiting for send task to complete..." << std::endl;
                send_task.wait();
                std::cout << "Send task completed." << std::endl;
        
                // Wait for response (no timeout for now to avoid compilation issues)
                std::cout << "Waiting for first response..." << std::endl;
                auto receive_task = _websocket_client->receive();
                std::cout << "Receive task created, waiting..." << std::endl;
                receive_task.wait();
                std::cout << "First response received." << std::endl;
                
                auto response_msg = receive_task.get();
                std::cout << "First response message type: " << static_cast<int>(response_msg.message_type()) << std::endl;
                
                if (response_msg.message_type() == websocket_message_type::text_message) {
                    std::string metadata_str = response_msg.extract_string().get();
                    std::cout << "Received metadata: " << metadata_str << std::endl;
                    nlohmann::json metadata = nlohmann::json::parse(metadata_str);
                    
                    if (metadata.value("status", "") == "success") {
                        std::vector<uint8_t> all_audio_data;
                        
                        // Continue receiving messages until we get a close message
                        bool receiving_audio = true;
                        while (receiving_audio) {
                            try {
                                auto audio_task = _websocket_client->receive();
                                audio_task.wait();
                                auto audio_msg = audio_task.get();
                                
                                if (audio_msg.message_type() == websocket_message_type::binary_message) {
                                    auto chunk_buffer = audio_msg.body();
                                    concurrency::streams::container_buffer<std::vector<uint8_t>> container_buffer;
                                    chunk_buffer.read_to_end(container_buffer).get();
                                    std::vector<uint8_t> chunk_data = container_buffer.collection();
                                    all_audio_data.insert(all_audio_data.end(), chunk_data.begin(), chunk_data.end());
                                    std::cout << "Received chunk of " << chunk_data.size() << " bytes." << std::endl;
                                } else if (static_cast<int>(audio_msg.message_type()) == 3) { // Close message
                                    std::cout << "Connection closed by server after receiving audio data." << std::endl;
                                    receiving_audio = false;
                                } else {
                                    std::cout << "Received non-binary message (type: " << static_cast<int>(audio_msg.message_type()) << ")." << std::endl;
                                    receiving_audio = false;
                                }
                            } catch (const std::exception& e) {
                                std::cerr << "Error receiving audio data: " << e.what() << std::endl;
                                receiving_audio = false;
                            }
                        }
                        
                        if (!all_audio_data.empty()) {
                            std::cout << "Total audio data received: " << all_audio_data.size() << " bytes." << std::endl;
                            std::cout << "=== TTS synthesis completed successfully ===" << std::endl;
                            return all_audio_data;
                        } else {
                            std::cerr << "No audio data received from TTS provider." << std::endl;
                        }
                    }
                } else if (static_cast<int>(response_msg.message_type()) == 3) { // Close message
                    std::cout << "Connection closed by server." << std::endl;
                    _is_connected = false;
                    std::cerr << "TTS Request failed: Connection closed by server before audio data was received." << std::endl;
                } else {
                    std::cerr << "Unexpected message type: " << static_cast<int>(response_msg.message_type()) << std::endl;
                }
            } catch (const std::exception& e) {
                std::cerr << "Error synthesizing speech: " << e.what() << std::endl;
                // Print stack trace if available
                #ifdef _MSC_VER
                // For MSVC, we could use CaptureStackBackTrace but it's complex
                #else
                // For other compilers, we could use backtrace() but it's also complex
                #endif
            }
            
            std::cout << "=== TTS synthesis failed ===" << std::endl;
            
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

        bool TTSService::is_ready() {
            // Check the /ready endpoint of the TTS provider
            try {
                web::uri provider_uri(utility::conversions::to_string_t(_provider_url));
                web::uri_builder builder;
                builder.set_scheme(U("http"));
                builder.set_host(provider_uri.host());
                builder.set_port(8001); // HTTP readiness endpoint is on port 8001
                builder.set_path(U("/ready"));

                web::http::client::http_client client(builder.to_uri());
                web::http::http_request request(web::http::methods::GET);
                auto response = client.request(request).get();
                return response.status_code() == web::http::status_codes::OK;
            } catch (const std::exception& e) {
                std::cerr << "Error checking TTS provider readiness: " << e.what() << std::endl;
                return false;
            }
        }
    }
}