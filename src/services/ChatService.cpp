#include <lily/services/ChatService.hpp>
#include <iostream>
#include <chrono>

namespace lily {
    namespace services {
        ChatService::ChatService(
            AgentLoopService& agentLoopService,
            MemoryService& memoryService,
            Service& toolService,
            TTSService& ttsService,
            WebSocketManager& webSocketManager
        ) : _agentLoopService(agentLoopService),
            _memoryService(memoryService),
            _toolService(toolService),
            _ttsService(ttsService),
            _webSocketManager(webSocketManager) {}

        std::string ChatService::handle_chat_message(const std::string& message, const std::string& user_id) {
            // Default to no TTS
            ChatParameters params;
            params.enable_tts = false;
            ChatResponse response = handle_chat_message_with_audio(message, user_id, params);
            return response.text_response;
        }

        ChatResponse ChatService::handle_chat_message_with_audio(const std::string& message, const std::string& user_id, const ChatParameters& params) {
            std::cout << "Handling chat message for user: " << user_id << std::endl;

            // 1. Save user message to memory
            std::cout << "Saving user message to memory..." << std::endl;
            _memoryService.add_message(user_id, "user", message);

            // 2. Get response from agent loop
            std::cout << "Invoking agent loop to get a response..." << std::endl;
            std::string agent_response = _agentLoopService.run_loop(message, user_id);

            // 3. Save agent response to memory
            std::cout << "Saving agent response to memory..." << std::endl;
            _memoryService.add_message(user_id, "assistant", agent_response);

            // 4. Prepare response structure
            ChatResponse response;
            response.text_response = agent_response;

            // 5. Synthesize the response to audio if TTS is enabled
            if (params.enable_tts) {
                std::cout << "Synthesizing agent response to audio..." << std::endl;
                auto audio_data = _ttsService.synthesize_speech(agent_response, params.tts_params);
                if (!audio_data.empty()) {
                    std::cout << "Audio synthesis successful: " << audio_data.size() << " bytes" << std::endl;
                    _webSocketManager.send_binary_to_client_by_id(user_id, audio_data);
                } else {
                    std::cerr << "Audio synthesis failed." << std::endl;
                }
            }

            std::cout << "Chat message processed." << std::endl;
            return response;
        }
    }
}