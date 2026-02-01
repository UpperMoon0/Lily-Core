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

int main() {
    // Check for optional environment variables
    bool gemini_available = (getenv("GEMINI_API_KEY") != nullptr);
    if (!gemini_available) {
        std::cerr << "[Main] Warning: GEMINI_API_KEY not set. AI features will be disabled." << std::endl;
    }
    
    // Track service availability for health monitoring
    bool tts_available = false;
    bool echo_available = false;

    auto tts_service = std::make_shared<TTSService>();
    auto echo_service = std::make_shared<EchoService>();
    auto tool_service = std::make_shared<Service>();
    
    // Register Lily-Core with Consul for both HTTP (8000) and WebSocket (9002) endpoints
    std::cout << "[Main] Registering Lily-Core with Consul..." << std::endl;
    tool_service->register_all_services(8000, 9002);
    
    tool_service->start_periodic_discovery();  // Start periodic tool discovery
    
    // Attempt to connect to services with a timeout, but don't block indefinitely
    int max_retries = 30; // 30 retries * 2 seconds = 60 seconds
    int retry_count = 0;

    while (retry_count < max_retries) {
        // Refresh discovered services
        auto services = tool_service->get_services_info();

        // Try to connect to TTS provider if not already connected
        if (!tts_available) {
            for (const auto& service : services) {
                if (service.id == "tts-provider") {
                    std::cout << "Found TTS provider at " << service.http_url << std::endl;
                    if (tts_service->connect(service.websocket_url.empty() ? service.http_url : service.websocket_url)) {
                        std::cout << "Connected to TTS provider successfully." << std::endl;
                        tts_available = true;
                    }
                }
            }
        }

        // Try to connect to Echo provider if not already connected
        if (!echo_available) {
            for (const auto& service : services) {
                if (service.id == "echo") {
                    std::cout << "Found Echo provider at " << service.http_url << std::endl;
                    if (echo_service->connect(service.http_url)) {
                        std::cout << "Connected to Echo provider successfully." << std::endl;
                        echo_available = true;
                    }
                }
            }
        }

        // Log progress periodically
        if (retry_count % 5 == 0) {
            std::cout << "Waiting for services... (Echo: " << (echo_available ? "connected" : "waiting") 
                      << ", TTS: " << (tts_available ? "connected" : "waiting") << ")" << std::endl;
        }
        
        // If we have Gemini key and Echo, we have minimal functionality
        if (gemini_available && echo_available) {
            std::cout << "[Main] Core services connected. Proceeding with full startup." << std::endl;
            break;
        }
        
        // Continue even without dependencies - services may come online later
        std::this_thread::sleep_for(std::chrono::seconds(2));
        retry_count++;
    }

    // Start HTTP server regardless of service availability
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
        std::cout << "[DEBUG] Received " << data.size() << " bytes from user " << user_id << std::endl;
        chat_service->handle_audio_stream(data, user_id);
    });
    
    websocket_manager->set_port(9002);
    websocket_manager->run();

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    std::cout << "[Main] Lily-Core is ready! (Gemini: " << (gemini_available ? "available" : "disabled")
              << ", Echo: " << (echo_available ? "available" : "disabled")
              << ", TTS: " << (tts_available ? "available" : "disabled") << ")" << std::endl;

    // Keep the main thread alive to allow the server to run
    std::promise<void>().get_future().wait();

    return 0;
}