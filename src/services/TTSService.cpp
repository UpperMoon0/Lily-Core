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
                
                // Set timeout values to prevent premature connection closure
                // These values are in milliseconds
                // Timeout configuration is not available in this version of cpprest
                
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
                
                return true;
            } catch (const std::exception& e) {
                std::cerr << "Error initializing WebSocket: " << e.what() << std::endl;
                return false;
            }
        }

        std::vector<uint8_t> TTSService::synthesize_speech(const std::string& text, const TTSParameters& params) {
            const int max_retries = 3;
            for (int attempt = 0; attempt < max_retries; ++attempt) {
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
                if (!connect(_provider_url, _websocket_url)) {
                    std::cerr << "Failed to connect to TTS service." << std::endl;
                    if (attempt < max_retries - 1) {
                        std::this_thread::sleep_for(std::chrono::seconds(1));
                        continue;
                    }
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
                    
                    // Add a small delay to help with timing issues
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                    auto send_task = _websocket_client->send(msg);
                    send_task.wait();
            
                    // Wait for response
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                    auto receive_task = _websocket_client->receive();
                    try {
                        receive_task.wait();
                    } catch (const std::exception& e) {
                        std::cerr << "Error waiting for first response: " << e.what() << std::endl;
                        if (attempt < max_retries - 1) {
                            std::this_thread::sleep_for(std::chrono::seconds(1));
                            continue; // Retry the entire process
                        }
                        throw;
                    }
                    
                    auto response_msg = receive_task.get();
                    
                    // Handle ping/pong messages by waiting for the actual response
                    // The TTS provider may send ping messages while generating audio
                    bool got_actual_response = false;
                    int ping_pong_count = 0;
                    const int max_ping_pong_wait = 10; // Maximum number of ping/pong messages to wait through
                    
                    while (!got_actual_response && ping_pong_count < max_ping_pong_wait) {
                        switch (response_msg.message_type()) {
                            case websocket_message_type::text_message: {
                                std::string metadata_str = response_msg.extract_string().get();
                                nlohmann::json metadata = nlohmann::json::parse(metadata_str);
                                
                                if (metadata.value("status", "") == "success") {
                                    std::vector<uint8_t> all_audio_data;
                                    
                                    // Continue receiving messages until we get a close message
                                    bool receiving_audio = true;
                                    int audio_chunk_count = 0;
                                    while (receiving_audio) {
                                        try {
                                            // Add a small delay to help with timing issues
                                            std::this_thread::sleep_for(std::chrono::milliseconds(50));
                                            auto audio_task = _websocket_client->receive();
                                            audio_task.wait();
                                            auto audio_msg = audio_task.get();
                                            
                                            switch (audio_msg.message_type()) {
                                                case websocket_message_type::binary_message: {
                                                    auto chunk_buffer = audio_msg.body();
                                                    concurrency::streams::container_buffer<std::vector<uint8_t>> container_buffer;
                                                    chunk_buffer.read_to_end(container_buffer).get();
                                                    std::vector<uint8_t> chunk_data = container_buffer.collection();
                                                    all_audio_data.insert(all_audio_data.end(), chunk_data.begin(), chunk_data.end());
                                                    audio_chunk_count++;
                                                    break;
                                                }
                                                case websocket_message_type::close: {
                                                    // Connection closure after receiving audio is normal, break out of loop
                                                    receiving_audio = false;
                                                    break;
                                                }
                                                case websocket_message_type::text_message: {
                                                    // Handle any text messages during audio streaming
                                                    break;
                                                }
                                                case websocket_message_type::ping: {
                                                    break;
                                                }
                                                case websocket_message_type::pong: {
                                                    break;
                                                }
                                                default: {
                                                    receiving_audio = false;
                                                    break;
                                                }
                                            }
                                        } catch (const std::exception& e) {
                                            std::string error_msg = e.what();
                                            // If we have received audio data and the error is about the connection closing, then it's expected.
                                            if (audio_chunk_count > 0 && (error_msg.find("closed") != std::string::npos || error_msg.find("close") != std::string::npos)) {
                                                // This is expected: the server closes the connection after sending audio.
                                                receiving_audio = false;
                                                break;
                                            } else {
                                                std::cerr << "Error receiving audio data: " << error_msg << std::endl;
                                            }

                                            if (attempt < max_retries - 1) {
                                                std::this_thread::sleep_for(std::chrono::seconds(1));
                                                continue; // Retry the entire process
                                            }
                                            receiving_audio = false;
                                        }
                                    }
                                    
                                    // Check if we received audio data
                                    if (!all_audio_data.empty()) {
                                        return all_audio_data;
                                    } else {
                                        std::cerr << "No audio data received from TTS provider." << std::endl;
                                        if (attempt < max_retries - 1) {
                                            std::this_thread::sleep_for(std::chrono::seconds(1));
                                            continue; // Retry the entire process
                                        }
                                    }
                                }
                                got_actual_response = true;
                                break;
                            }
                            case websocket_message_type::close: {
                                _is_connected = false;
                                std::cerr << "TTS Request failed: Connection closed by server before audio data was received." << std::endl;
                                if (attempt < max_retries - 1) {
                                    std::this_thread::sleep_for(std::chrono::seconds(1));
                                    continue; // Retry the entire process
                                }
                                got_actual_response = true;
                                break;
                            }
                            case websocket_message_type::ping: {
                                ping_pong_count++;
                                // Wait for the next message
                                try {
                                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                                    auto receive_task2 = _websocket_client->receive();
                                    receive_task2.wait();
                                    response_msg = receive_task2.get();
                                } catch (const std::exception& e) {
                                    std::cerr << "Error waiting for response after ping: " << e.what() << std::endl;
                                    if (attempt < max_retries - 1) {
                                        std::this_thread::sleep_for(std::chrono::seconds(1));
                                        continue; // Retry the entire process
                                    }
                                    got_actual_response = true;
                                }
                                break;
                            }
                            case websocket_message_type::pong: {
                                ping_pong_count++;
                                // Wait for the next message
                                try {
                                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                                    auto receive_task2 = _websocket_client->receive();
                                    receive_task2.wait();
                                    response_msg = receive_task2.get();
                                } catch (const std::exception& e) {
                                    std::cerr << "Error waiting for response after pong: " << e.what() << std::endl;
                                    if (attempt < max_retries - 1) {
                                        std::this_thread::sleep_for(std::chrono::seconds(1));
                                        continue; // Retry the entire process
                                    }
                                    got_actual_response = true;
                                }
                                break;
                            }
                            default: {
                                std::cerr << "Unexpected message type: " << static_cast<int>(response_msg.message_type()) << std::endl;
                                if (attempt < max_retries - 1) {
                                    std::this_thread::sleep_for(std::chrono::seconds(1));
                                    continue; // Retry the entire process
                                }
                                got_actual_response = true;
                                break;
                            }
                        }
                    }
                    
                    if (!got_actual_response) {
                        std::cerr << "Exceeded maximum ping/pong wait count (" << max_ping_pong_wait << ")." << std::endl;
                        if (attempt < max_retries - 1) {
                            std::this_thread::sleep_for(std::chrono::seconds(1));
                            continue; // Retry the entire process
                        }
                    }
                } catch (const std::exception& e) {
                    std::cerr << "Error synthesizing speech: " << e.what() << std::endl;
                    if (attempt < max_retries - 1) {
                        std::this_thread::sleep_for(std::chrono::seconds(1));
                        continue; // Retry the entire process
                    }
                }
                
                // If we reach here, this attempt failed, but we might retry
                if (attempt < max_retries - 1) {
                    std::this_thread::sleep_for(std::chrono::seconds(1));
                    continue; // Retry the entire process
                }
            }
            
            // All attempts failed
            return {};
        }

        void TTSService::close() {
            if (_websocket_client) {
                try {
                    auto task = _websocket_client->close();
                    task.wait();
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