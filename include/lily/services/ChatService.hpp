#ifndef LILY_CHAT_SERVICE_HPP
#define LILY_CHAT_SERVICE_HPP

#include <lily/services/AgentLoopService.hpp>
#include <lily/services/MemoryService.hpp>
#include <lily/services/ToolService.hpp>
#include <lily/services/TTSService.hpp>
#include <string>

namespace lily {
    namespace services {
        class ChatService {
        public:
            ChatService(
                AgentLoopService& agentLoopService,
                MemoryService& memoryService,
                ToolService& toolService,
                TTSService& ttsService
            );

            std::string handle_chat_message(const std::string& message, const std::string& user_id);

        private:
            AgentLoopService& _agentLoopService;
            MemoryService& _memoryService;
            ToolService& _toolService;
            TTSService& _ttsService;
        };
    }
}

#endif // LILY_CHAT_SERVICE_HPP