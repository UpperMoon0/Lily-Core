#include <lily/services/ChatService.hpp>
#include <lily/services/EchoService.hpp>
#include <iostream>
#include <chrono>
#include <nlohmann/json.hpp>

namespace lily {
    namespace services {
        ChatService::ChatService(
            AgentLoopService& agentLoopService,
            MemoryService& memoryService,
            Service& toolService,
            TTSService& ttsService,
            EchoService& echoService,
            WebSocketManager& webSocketManager
        ) : _agentLoopService(agentLoopService),
            _memoryService(memoryService),
            _toolService(toolService),
            _ttsService(ttsService),
            _echoService(echoService),
            _webSocketManager(webSocketManager) {
            
            // Set up the transcription handler
            _echoService.set_transcription_handler([this](const std::string& payload) {
                try {
                    auto json = nlohmann::json::parse(payload);
                    std::cout << "Received transcription from Echo: " << payload << std::endl;
                    
                    // Broadcast to all connected users (since we don't track audio-user mapping perfectly in this simple version)
                    // We'll prefix with "transcription:" as expected by the client/test
                    std::string message = "transcription:" + payload;
                    _webSocketManager.broadcast(message);
                } catch (const std::exception& e) {
                    std::cerr << "Error handling transcription: " << e.what() << std::endl;
                }
            });
        }

        std::string ChatService::handle_chat_message(const std::string& message, const std::string& user_id) {
            // Default to no TTS
            ChatParameters params;
            params.enable_tts = false;
            ChatResponse response = handle_chat_message_with_audio(message, user_id, params);
            return response.text_response;
        }

        ChatResponse ChatService::handle_chat_message_with_audio(const std::string& message, const std::string& user_id, const ChatParameters& params) {
            // 1. Save user message to memory
            _memoryService.add_message(user_id, "user", message);

            // 2. Get response from agent loop
            std::string agent_response = _agentLoopService.run_loop(message, user_id);

            // 3. Save agent response to memory
            _memoryService.add_message(user_id, "assistant", agent_response);

            // 4. Prepare response structure
            ChatResponse response;
            response.text_response = agent_response;

            // 5. Synthesize the response to audio if TTS is enabled
            if (params.enable_tts) {
                auto audio_data = _ttsService.synthesize_speech(agent_response, params.tts_params);
                if (!audio_data.empty()) {
                    // Wait for the connection to be registered before sending data
                    if (_webSocketManager.wait_for_connection_registration(user_id, 10)) {
                        _webSocketManager.send_binary_to_client_by_id(user_id, audio_data);
                    } else {
                        std::cerr << "Connection for user_id " << user_id << " is not registered, unable to send audio data." << std::endl;
                    }
                } else {
                    std::cerr << "Audio synthesis failed." << std::endl;
                }
            }

            return response;
        }

        void ChatService::handle_audio_stream(const std::vector<uint8_t>& audio_data, const std::string& user_id) {
            // Forward audio data to EchoService
            _echoService.send_audio(audio_data);
        }
    }
}