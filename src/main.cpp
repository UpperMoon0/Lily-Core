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

using namespace lily::services;

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

    http_server_ptr = std::make_unique<HTTPServer>("0.0.0.0", 8000, *chat_service, *memory_service, *tool_service.get(), *websocket_manager);
    http_server_ptr->start();
    
    std::cout << "[Main] Starting WebSocket server on port 9002..." << std::endl;
    websocket_manager->set_binary_message_handler([chat_service](const std::vector<uint8_t>& data, const std::string& user_id) {
        chat_service->handle_audio_stream(data, user_id);
    });
    
    websocket_manager->set_port(9002);
    websocket_manager->run();

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    std::cout << "[Main] Lily-Core is ready! (Gemini: " << (gemini_available ? "available" : "disabled")
              << ", Echo: connecting asynchronously, TTS: connecting asynchronously)" << std::endl;

    // Keep the main thread alive to allow the server to run
    std::promise<void>().get_future().wait();

    return 0;
}
