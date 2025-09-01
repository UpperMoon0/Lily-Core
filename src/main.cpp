#include <iostream>
#include <memory>
#include <csignal>

#include "lily/services/AgentLoopService.hpp"
#include "lily/services/ChatService.hpp"
#include "lily/services/MemoryService.hpp"
#include "lily/services/ToolService.hpp"
#include "lily/services/TTSService.hpp"
#include "lily/services/HTTPServer.hpp"
#include <thread>
#include <chrono>
#include <future>

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
    auto tool_service = std::make_shared<ToolService>();
    tool_service->start_periodic_discovery();  // Start periodic tool discovery
    auto memory_service = std::make_shared<MemoryService>();
    auto agent_loop_service = std::make_shared<AgentLoopService>(*memory_service, *tool_service);
    auto chat_service = std::make_shared<ChatService>(
        *agent_loop_service,
        *memory_service,
        *tool_service,
        *tts_service
    );

    http_server_ptr = std::make_unique<HTTPServer>("0.0.0.0", 8000, *chat_service, *memory_service, *tool_service);
    http_server_ptr->start();

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    // Keep the main thread alive to allow the server to run
    std::promise<void>().get_future().wait();

    return 0;
}