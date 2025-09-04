#include <iostream>
#include <memory>
#include <csignal>

#include "lily/services/AgentLoopService.hpp"
#include "lily/services/ChatService.hpp"
#include "lily/services/MemoryService.hpp"
#include "lily/services/Service.hpp"
#include "lily/services/TTSService.hpp"
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
    auto tool_service = std::make_shared<Service>();
    tool_service->start_periodic_discovery();  // Start periodic tool discovery
    
    // Give some time for service discovery to complete
    std::this_thread::sleep_for(std::chrono::seconds(2));
    
    // Try to connect to TTS provider
    auto discovered_servers = tool_service->get_discovered_servers();
    auto tools_per_server = tool_service->get_tools_per_server();
    
    // Look for TTS provider in the discovered services
    for (const auto& service : tool_service->get_services_info()) {
        if (service.id == "tts-provider") {
            std::cout << "Found TTS provider at " << service.http_url << std::endl;
            if (tts_service->connect(service.websocket_url.empty() ? service.http_url : service.websocket_url)) {
                std::cout << "Connected to TTS provider successfully." << std::endl;
            } else {
                std::cerr << "Failed to connect to TTS provider." << std::endl;
            }
            break;
        }
    }
    
    auto memory_service = std::make_shared<MemoryService>();
    auto agent_loop_service = std::make_shared<AgentLoopService>(*memory_service, *tool_service.get());
    auto websocket_manager = std::make_shared<WebSocketManager>();
    auto chat_service = std::make_shared<ChatService>(
        *agent_loop_service,
        *memory_service,
        *tool_service.get(),
        *tts_service,
        *websocket_manager
    );

    http_server_ptr = std::make_unique<HTTPServer>("0.0.0.0", 8000, *chat_service, *memory_service, *tool_service.get(), *websocket_manager, *agent_loop_service);
    http_server_ptr->start();

    websocket_manager->set_port(9002);
    websocket_manager->run();

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    // Keep the main thread alive to allow the server to run
    std::promise<void>().get_future().wait();

    return 0;
}