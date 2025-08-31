#include <lily/services/ChatService.hpp>
#include <iostream>

namespace lily {
    namespace services {
        ChatService::ChatService(
            AgentLoopService& agentLoopService,
            MemoryService& memoryService,
            ToolService& toolService,
            TTSService& ttsService
        ) : _agentLoopService(agentLoopService),
            _memoryService(memoryService),
            _toolService(toolService),
            _ttsService(ttsService) {}

        void ChatService::handle_chat_message(const std::string& message, const std::string& user_id) {
            std::cout << "Handling chat message for user: " << user_id << std::endl;

            // 1. Save user message to memory
            std::cout << "Saving user message to memory..." << std::endl;
            // _memoryService.add_message(user_id, "user", message);

            // 2. Get response from agent loop
            std::cout << "Invoking agent loop to get a response..." << std::endl;
            // auto agent_response = _agentLoopService.execute_agent_loop(message, user_id);
            std::string agent_response = "This is a placeholder response from the agent loop.";

            // 3. Save agent response to memory
            std::cout << "Saving agent response to memory..." << std::endl;
            // _memoryService.add_message(user_id, "assistant", agent_response);

            // 4. Synthesize the response to audio
            std::cout << "Synthesizing agent response to audio..." << std::endl;
            // _ttsService.synthesize(agent_response);

            std::cout << "Chat message processed." << std::endl;
        }
    }
}