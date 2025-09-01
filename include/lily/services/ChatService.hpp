#ifndef LILY_CHAT_SERVICE_HPP
#define LILY_CHAT_SERVICE_HPP

#include <lily/services/AgentLoopService.hpp>
#include <lily/services/MemoryService.hpp>
#include <lily/services/Service.hpp>
#include <lily/services/TTSService.hpp>
#include <string>

namespace lily {
    namespace services {
        struct ChatParameters {
            bool enable_tts = false;
            TTSParameters tts_params;
        };

        class ChatService {
        public:
            ChatService(
                AgentLoopService& agentLoopService,
                MemoryService& memoryService,
                Service& toolService,
                TTSService& ttsService
            );

            std::string handle_chat_message(const std::string& message, const std::string& user_id);
            std::string handle_chat_message(const std::string& message, const std::string& user_id, const ChatParameters& params);

        private:
            AgentLoopService& _agentLoopService;
            MemoryService& _memoryService;
            Service& _toolService;
            TTSService& _ttsService;
        };
    }
}

#endif // LILY_CHAT_SERVICE_HPP