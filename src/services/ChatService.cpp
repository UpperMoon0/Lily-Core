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
            GatewayService& webSocketManager,
            SessionService& sessionService,
            utils::ThreadPool& threadPool
        ) : _agentLoopService(agentLoopService),
            _memoryService(memoryService),
            _toolService(toolService),
            _ttsService(ttsService),
            _echoService(echoService),
            _webSocketManager(webSocketManager),
            _sessionService(sessionService),
            _threadPool(threadPool) {
            
            // Set up the transcription handler
            _echoService.set_transcription_handler([this](const std::string& payload) {
                try {
                    auto json = nlohmann::json::parse(payload);
                    std::cout << "Received transcription from Echo: " << payload << std::endl;
                    
                    std::string message = "transcription:" + payload;
                    _webSocketManager.broadcast(message);
                } catch (const std::exception& e) {
                    std::cerr << "Error handling transcription: " << e.what() << std::endl;
                }
            });
        }

        std::string ChatService::handle_chat_message(const std::string& message, const std::string& user_id) {
            ChatParameters params;
            params.enable_tts = false;
            ChatResponse response = handle_chat_message_with_audio(message, user_id, params);
            return response.text_response;
        }

        ChatResponse ChatService::handle_chat_message_with_audio(const std::string& message, const std::string& user_id, const ChatParameters& params) {
            // Update session activity
            _sessionService.touch_session(user_id);
            if (!_sessionService.is_session_active(user_id)) {
                _sessionService.start_session(user_id);
            }

            // 1. Save user message
            _memoryService.add_message(user_id, "user", message);

            // 2. Get response from agent loop (BLOCKING)
            std::string agent_response = _agentLoopService.run_loop(message, user_id);

            // 3. Save agent response
            _memoryService.add_message(user_id, "assistant", agent_response);

            // 4. Prepare response
            ChatResponse response;
            response.text_response = agent_response;

            // 5. Synthesize TTS (BLOCKING)
            if (params.enable_tts) {
                auto audio_data = _ttsService.synthesize_speech(agent_response, params.tts_params);
                if (!audio_data.empty()) {
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

        void ChatService::handle_chat_message_async(const std::string& message, const std::string& user_id, CompletionCallback callback) {
            // Use the audio async version with default params
            ChatParameters params;
            params.enable_tts = false;
            
            handle_chat_message_with_audio_async(message, user_id, params, [callback](ChatResponse response) {
                if (callback) {
                    callback(response.text_response);
                }
            });
        }

        void ChatService::handle_chat_message_with_audio_async(const std::string& message, const std::string& user_id, const ChatParameters& params, AudioCompletionCallback callback) {
            _threadPool.enqueue([this, message, user_id, params, callback]() {
                try {
                    // Reuse the synchronous logic which is now safe to call on worker thread
                    ChatResponse response = handle_chat_message_with_audio(message, user_id, params);
                    
                    if (callback) {
                        callback(response);
                    }
                } catch (const std::exception& e) {
                    std::cerr << "Error in async chat processing: " << e.what() << std::endl;
                    if (callback) {
                        ChatResponse error_response;
                        error_response.text_response = "Error processing request: " + std::string(e.what());
                        callback(error_response);
                    }
                }
            });
        }

        void ChatService::handle_audio_stream(const std::vector<uint8_t>& audio_data, const std::string& user_id) {
            _sessionService.touch_session(user_id);
            _echoService.send_audio(audio_data);
        }
    }
}
