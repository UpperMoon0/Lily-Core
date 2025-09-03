#ifndef LILY_CHAT_SERVICE_HPP
#define LILY_CHAT_SERVICE_HPP

#include <lily/services/AgentLoopService.hpp>
#include <lily/services/MemoryService.hpp>
#include <lily/services/Service.hpp>
#include <lily/services/TTSService.hpp>
#include <lily/services/WebSocketManager.hpp>
#include <string>
#include <vector>
#include <cstdint>

namespace lily {
    namespace services {
        struct ChatParameters {
            bool enable_tts = false;
            TTSParameters tts_params;
        };
        
        struct ChatResponse {
            std::string text_response;
        };

        class ChatService {
        public:
            ChatService(
                AgentLoopService& agentLoopService,
                MemoryService& memoryService,
                Service& toolService,
                TTSService& ttsService,
                WebSocketManager& webSocketManager
            );

            std::string handle_chat_message(const std::string& message, const std::string& user_id);
            ChatResponse handle_chat_message_with_audio(const std::string& message, const std::string& user_id, const ChatParameters& params);

        private:
            AgentLoopService& _agentLoopService;
            MemoryService& _memoryService;
            Service& _toolService;
            TTSService& _ttsService;
            WebSocketManager& _webSocketManager;
        };
    }
}

#endif // LILY_CHAT_SERVICE_HPP