#ifndef LILY_CHAT_SERVICE_HPP
#define LILY_CHAT_SERVICE_HPP

#include <lily/services/AgentLoopService.hpp>
#include <lily/services/MemoryService.hpp>
#include <lily/services/Service.hpp>
#include <lily/services/TTSService.hpp>
#include <lily/services/EchoService.hpp>
#include <lily/services/GatewayService.hpp>
#include <lily/services/SessionService.hpp>
#include <lily/utils/ThreadPool.hpp>
#include <string>
#include <vector>
#include <cstdint>
#include <functional>

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
                EchoService& echoService,
                GatewayService& webSocketManager,
                SessionService& sessionService,
                utils::ThreadPool& threadPool
            );

            // Synchronous (Blocking) - Deprecated for high load
            std::string handle_chat_message(const std::string& message, const std::string& user_id);
            ChatResponse handle_chat_message_with_audio(const std::string& message, const std::string& user_id, const ChatParameters& params);
            
            // Asynchronous (Non-Blocking)
            using CompletionCallback = std::function<void(std::string)>;
            void handle_chat_message_async(const std::string& message, const std::string& user_id, CompletionCallback callback);

            using AudioCompletionCallback = std::function<void(ChatResponse)>;
            void handle_chat_message_with_audio_async(const std::string& message, const std::string& user_id, const ChatParameters& params, AudioCompletionCallback callback);

            void handle_audio_stream(const std::vector<uint8_t>& audio_data, const std::string& user_id);

        private:
            AgentLoopService& _agentLoopService;
            MemoryService& _memoryService;
            Service& _toolService;
            TTSService& _ttsService;
            EchoService& _echoService;
            GatewayService& _webSocketManager;
            SessionService& _sessionService;
            utils::ThreadPool& _threadPool;
        };
    }
}

#endif // LILY_CHAT_SERVICE_HPP
