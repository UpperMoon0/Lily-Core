#include <iostream>
#include <memory>

#include "lily/services/AgentLoopService.hpp"
#include "lily/services/ChatService.hpp"
#include "lily/services/MemoryService.hpp"
#include "lily/services/ToolService.hpp"
#include "lily/services/TTSService.hpp"
#include "lily/services/WebSocketManager.hpp"

int main() {
    std::cout << "Lily-Core C++ application starting..." << std::endl;

    // Instantiate Services
    auto websocket_manager = std::make_shared<WebSocketManager>();
    auto tts_service = std::make_shared<TTSService>();
    auto tool_service = std::make_shared<ToolService>();
    auto memory_service = std::make_shared<MemoryService>();
    auto agent_loop_service = std::make_shared<AgentLoopService>();
    auto chat_service = std::make_shared<ChatService>(
        *agent_loop_service,
        *memory_service,
        *tool_service,
        *tts_service,
        *websocket_manager
    );

    // Mock application loop
    std::cout << "Simulating user message..." << std::endl;
    std::string user_message = "Hello, Lily!";
    chat_service->handle_chat_message(user_message);

    std::cout << "Lily-Core C++ application shutting down..." << std::endl;

    return 0;
}