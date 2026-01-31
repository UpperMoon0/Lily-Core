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
    if (getenv("GEMINI_API_KEY") == nullptr) {
        std::cerr << "Error: GEMINI_API_KEY environment variable not set." << std::endl;
        return 1;
    }

    auto tts_service = std::make_shared<TTSService>();
    auto echo_service = std::make_shared<EchoService>();
    auto tool_service = std::make_shared<Service>();
    tool_service->start_periodic_discovery();  // Start periodic tool discovery
    
    // Retry connection loop to ensure services are discovered
    int max_retries = 30; // 30 retries * 2 seconds = 60 seconds
    int retry_count = 0;
    bool echo_connected = false;
    bool tts_connected = false;

    while (retry_count < max_retries) {
        // Refresh discovered services
        auto services = tool_service->get_services_info();

        // Try to connect to TTS provider if not already connected
        if (!tts_connected) {
            for (const auto& service : services) {
                if (service.id == "tts-provider") {
                    std::cout << "Found TTS provider at " << service.http_url << std::endl;
                    if (tts_service->connect(service.websocket_url.empty() ? service.http_url : service.websocket_url)) {
                        std::cout << "Connected to TTS provider successfully." << std::endl;
                        tts_connected = true;
                    }
                }
            }
        }

        // Try to connect to Echo provider if not already connected
        if (!echo_connected) {
            for (const auto& service : services) {
                if (service.id == "echo") {
                    std::cout << "Found Echo provider at " << service.http_url << std::endl;
                    if (echo_service->connect(service.http_url)) {
                        std::cout << "Connected to Echo provider successfully." << std::endl;
                        echo_connected = true;
                    }
                }
            }
        }

        // If Echo is connected, we can proceed (TTS is optional but good to have)
        if (echo_connected) {
            std::cout << "Critical services (Echo) connected. Proceeding." << std::endl;
            break;
        }

        if (retry_count % 5 == 0) {
            std::cout << "Waiting for critical services... (Echo connected: " << (echo_connected ? "YES" : "NO") << ", TTS connected: " << (tts_connected ? "YES" : "NO") << ")" << std::endl;
        }
        
        std::this_thread::sleep_for(std::chrono::seconds(2));
        retry_count++;
    }

    if (!echo_connected) {
        std::cerr << "Warning: critical services (Echo) failed to connect after timeout. Speech features may not work." << std::endl;
    }
    
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

    websocket_manager->set_binary_message_handler([chat_service](const std::vector<uint8_t>& data, const std::string& user_id) {
        std::cout << "[DEBUG] Received " << data.size() << " bytes from user " << user_id << std::endl;
        chat_service->handle_audio_stream(data, user_id);
    });

    websocket_manager->set_port(9002);
    websocket_manager->run();

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    // Keep the main thread alive to allow the server to run
    std::promise<void>().get_future().wait();

    return 0;
}