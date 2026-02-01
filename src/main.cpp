#include <iostream>
#include <memory>
#include <csignal>

#include "lily/services/AgentLoopService.hpp"
#include "lily/services/ChatService.hpp"
#include "lily/services/MemoryService.hpp"
#include "lily/services/Service.hpp"
#include "lily/services/TTSService.hpp"
#include "lily/services/EchoService.hpp"
#include "lily/services/WebSocketManager.hpp"
#include "lily/services/HTTPServer.hpp"
#include <thread>
#include <chrono>
#include <future>
#include <nlohmann/json.hpp>
#include <cpprest/http_client.h>
#include <cpprest/filestream.h>

using namespace lily::services;
using namespace web;
using namespace web::http;
using namespace web::http::client;

std::unique_ptr<HTTPServer> http_server_ptr;

void signal_handler(int signal) {
    if (http_server_ptr) {
        http_server_ptr->stop();
    }
    exit(signal);
}

// Background service connector - runs in a separate thread
void connect_services_async(
    std::shared_ptr<TTSService> tts_service,
    std::shared_ptr<EchoService> echo_service,
    std::shared_ptr<Service> tool_service,
    std::atomic<bool>& tts_available,
    std::atomic<bool>& echo_available
) {
    std::cout << "[ServiceConnector] Starting background service discovery..." << std::endl;
    
    int retry_count = 0;
    while (true) {
        // Only check for services if not already connected
        if (!tts_available || !echo_available) {
            auto services = tool_service->get_services_info();
            
            // Try to connect to TTS provider
            if (!tts_available) {
                for (const auto& service : services) {
                    if (service.id == "tts-provider") {
                        if (tts_service->connect(service.websocket_url.empty() ? service.http_url : service.websocket_url)) {
                            tts_available = true;
                            std::cout << "[ServiceConnector] Connected to TTS provider at " << service.http_url << std::endl;
                        }
                    }
                }
            }
            
            // Try to connect to Echo provider
            if (!echo_available) {
                for (const auto& service : services) {
                    if (service.id == "echo") {
                        if (echo_service->connect(service.http_url)) {
                            echo_available = true;
                            std::cout << "[ServiceConnector] Connected to Echo provider at " << service.http_url << std::endl;
                        }
                    }
                }
            }
            
            // Log progress
            if (retry_count % 15 == 0) {
                std::cout << "[ServiceConnector] Status - Echo: " << (echo_available ? "connected" : "waiting")
                          << ", TTS: " << (tts_available ? "connected" : "waiting") << std::endl;
            }
        }
        
        // Try to reconnect every 2 seconds if not connected, otherwise check less frequently
        if (!tts_available || !echo_available) {
            std::this_thread::sleep_for(std::chrono::seconds(2));
        } else {
            std::this_thread::sleep_for(std::chrono::seconds(10));
        }
        retry_count++;
    }
}

int main() {
    // Check for optional environment variables
    bool gemini_available = (getenv("GEMINI_API_KEY") != nullptr);
    if (!gemini_available) {
        std::cerr << "[Main] Warning: GEMINI_API_KEY not set. AI features will be disabled." << std::endl;
    }
    
    // Track service availability for health monitoring (atomic for thread safety)
    std::atomic<bool> tts_available{false};
    std::atomic<bool> echo_available{false};

    auto tts_service = std::make_shared<TTSService>();
    auto echo_service = std::make_shared<EchoService>();
    auto tool_service = std::make_shared<Service>();
    
    // Register Lily-Core with Consul for both HTTP (8000) and WebSocket (9002) endpoints
    std::cout << "[Main] Registering Lily-Core with Consul..." << std::endl;
    tool_service->register_all_services(8000, 9002);
    
    // Start periodic discovery in background (discovers and updates service list)
    tool_service->start_periodic_discovery();
    
    // Also start background service connector for Echo/TTS
    std::thread service_connector(connect_services_async, 
                                   tts_service, echo_service, tool_service,
                                   std::ref(tts_available), std::ref(echo_available));
    service_connector.detach();
    
    // Start HTTP server immediately - don't wait for services
    std::cout << "[Main] Starting HTTP server on port 8000..." << std::endl;
    
    auto memory_service = std::make_shared<MemoryService>();
    auto agent_loop_service = std::make_shared<AgentLoopService>(*memory_service, *tool_service.get());
    auto websocket_manager = std::make_shared<WebSocketManager>();
    auto chat_service = std::make_shared<ChatService>(
        *agent_loop_service,
        *memory_service,
        *tool_service.get(),
        *tts_service,
        *echo_service,
        *websocket_manager
    );

    http_server_ptr = std::make_unique<HTTPServer>("0.0.0.0", 8000, *chat_service, *memory_service, *tool_service.get(), *websocket_manager, *agent_loop_service);
    http_server_ptr->start();

    std::cout << "[Main] Starting WebSocket server on port 9002..." << std::endl;
    websocket_manager->set_binary_message_handler([chat_service](const std::vector<uint8_t>& data, const std::string& user_id) {
        chat_service->handle_audio_stream(data, user_id);
    });
    
    websocket_manager->set_port(9002);
    websocket_manager->set_ping_interval(30);  // Ping every 30 seconds
    websocket_manager->set_pong_timeout(60);  // Timeout after 60 seconds

    // Set Echo message handler to process transcription results
    websocket_manager->set_echo_message_handler([&chat_service, &websocket_manager](const nlohmann::json& message) {
        try {
            std::string message_type = message["type"];
            std::string text = message["text"];

            if (message_type == "interim") {
                std::cout << "Interim transcription: " << text << std::endl;
                // Forward interim transcription to UI for live display
                nlohmann::json ui_message = {
                    {"type", "interim"},
                    {"text", text}
                };
                std::string ui_message_str = ui_message.dump();
                websocket_manager->broadcast("transcription:" + ui_message_str);
            } else if (message_type == "final") {
                std::cout << "Final transcription: " << text << std::endl;
                // Forward final transcription to UI
                nlohmann::json ui_message = {
                    {"type", "final"},
                    {"text", text}
                };
                std::string ui_message_str = ui_message.dump();
                websocket_manager->broadcast("transcription:" + ui_message_str);

                // Send final transcription to chat service (triggers agent loop)
                chat_service->handle_chat_message(text, "default_user");
            }
        } catch (const std::exception& e) {
            std::cerr << "Error processing Echo message: " << e.what() << std::endl;
        }
    });

    // Connect to Echo service
    std::string echo_websocket_url;
    for (const auto& service : tool_service->get_services_info()) {
        if (service.id == "echo") {
            // Use WebSocket URL if available, otherwise construct from HTTP URL
            if (!service.websocket_url.empty()) {
                echo_websocket_url = service.websocket_url + "/ws/transcribe";
            } else {
                // Convert HTTP URL to WebSocket URL
                std::string http_url = service.http_url;
                if (http_url.find("http://") == 0) {
                    echo_websocket_url = "ws://" + http_url.substr(7) + "/ws/transcribe";
                } else if (http_url.find("https://") == 0) {
                    echo_websocket_url = "wss://" + http_url.substr(8) + "/ws/transcribe";
                }
            }
            std::cout << "Found Echo WebSocket endpoint at " << echo_websocket_url << std::endl;
            break;
        }
    }

    if (!echo_websocket_url.empty()) {
        if (!websocket_manager->connect_to_echo(echo_websocket_url)) {
            std::cerr << "Failed to connect to Echo service" << std::endl;
        }
    } else {
        std::cerr << "Echo service not found. Audio transcription will not work." << std::endl;
    }
    
    websocket_manager->run();

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    std::cout << "[Main] Lily-Core is ready! (Gemini: " << (gemini_available ? "available" : "disabled")
              << ", Echo: connecting asynchronously, TTS: connecting asynchronously)" << std::endl;

    // Keep the main thread alive to allow the server to run
    std::promise<void>().get_future().wait();

    return 0;
}
