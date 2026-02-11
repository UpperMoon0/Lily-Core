#include "lily/controller/ChatController.hpp"
#include "lily/services/ChatService.hpp"
#include "lily/services/AgentLoopService.hpp"
#include "lily/services/MemoryService.hpp"
#include "lily/models/AgentLoop.hpp"
#include <iostream>
#include <ctime>
#include <chrono>

namespace lily {
namespace controller {

    ChatController::ChatController(
        std::shared_ptr<services::ChatService> chatService,
        std::shared_ptr<services::AgentLoopService> agentLoopService,
        std::shared_ptr<services::MemoryService> memoryService
    ) : _chatService(chatService), _agentLoopService(agentLoopService), _memoryService(memoryService) {}

    void ChatController::chat(const nlohmann::json& request, std::function<void(const nlohmann::json&, int)> callback) {
        if (!request.contains("message") || !request.contains("user_id")) {
             nlohmann::json error = {{"error", "Missing 'message' or 'user_id'"}};
             callback(error, 400); 
             return;
        }

        std::string message = request["message"];
        std::string user_id = request["user_id"];
        
        services::ChatParameters chat_params;
        if (request.contains("tts") && request["tts"].is_object()) {
            const auto& tts_json = request["tts"];
            chat_params.enable_tts = tts_json.value("enabled", false);
            
            if (tts_json.contains("params") && tts_json["params"].is_object()) {
                const auto& params_json = tts_json["params"];
                chat_params.tts_params.speaker = params_json.value("speaker", 0);
                chat_params.tts_params.sample_rate = params_json.value("sample_rate", 24000);
                chat_params.tts_params.model = params_json.value("model", "edge");
                chat_params.tts_params.lang = params_json.value("lang", "en-US");
            }
        }
        
        if (chat_params.enable_tts) {
            std::cout << "TTS enabled for user_id: " << user_id << std::endl;
        }

        // ASYNC HANDLING
        _chatService->handle_chat_message_with_audio_async(message, user_id, chat_params, [callback](services::ChatResponse chat_response) {
            nlohmann::json response_json;
            response_json["response"] = chat_response.text_response;
            
            auto now = std::chrono::system_clock::now();
            std::time_t now_time = std::chrono::system_clock::to_time_t(now);
            char buf[100];
            if (std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", std::gmtime(&now_time))) {
                response_json["timestamp"] = std::string(buf);
            }
            
            callback(response_json, 200);
        });
    }

    nlohmann::json ChatController::getAgentLoops() {
        if (!_agentLoopService) {
             return {{"exists", false}, {"message", "AgentLoopService not available"}};
        }
        const auto& last_loop = _agentLoopService->get_last_agent_loop();
        nlohmann::json response;
        
        if (last_loop.user_id.empty()) {
            response["exists"] = false;
            response["message"] = "No agent loops available";
        } else {
            response["exists"] = true;
            response["user_id"] = last_loop.user_id;
            response["user_message"] = last_loop.user_message;
            response["final_response"] = last_loop.final_response;
            response["completed"] = last_loop.completed;
            
            char buf[100];
            auto start_time = std::chrono::system_clock::to_time_t(last_loop.start_time);
            if (std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", std::gmtime(&start_time))) {
                response["start_time"] = std::string(buf);
            }
            
            auto end_time = std::chrono::system_clock::to_time_t(last_loop.end_time);
            if (std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", std::gmtime(&end_time))) {
                response["end_time"] = std::string(buf);
            }
            
            response["duration_seconds"] = last_loop.duration_seconds;
            
            nlohmann::json steps_json = nlohmann::json::array();
            for (const auto& step : last_loop.steps) {
                nlohmann::json step_json;
                step_json["step_number"] = step.step_number;
                
                std::string step_type_str;
                switch (step.type) {
                    case lily::models::AgentStepType::THINKING: step_type_str = "thinking"; break;
                    case lily::models::AgentStepType::TOOL_CALL: step_type_str = "tool_call"; break;
                    case lily::models::AgentStepType::RESPONSE: step_type_str = "response"; break;
                    default: step_type_str = "unknown";
                }
                step_json["type"] = step_type_str;
                step_json["reasoning"] = step.reasoning;
                step_json["tool_name"] = step.tool_name;
                
                step_json["tool_parameters"] = step.tool_parameters;
                step_json["tool_result"] = step.tool_result;
                
                auto step_time = std::chrono::system_clock::to_time_t(step.timestamp);
                if (std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", std::gmtime(&step_time))) {
                    step_json["timestamp"] = std::string(buf);
                }
                
                step_json["duration_seconds"] = step.duration_seconds;
                
                steps_json.push_back(step_json);
            }
            response["steps"] = steps_json;
        }
        return response;
    }

    nlohmann::json ChatController::getConversation(const std::string& userId) {
        if (!_memoryService) {
             return {{"error", "MemoryService not available"}};
        }
        const auto& conversation = _memoryService->get_conversation(userId);
        nlohmann::json response;
        response["user_id"] = userId;
        nlohmann::json conv_json = nlohmann::json::array();
        
        char buf[100];
        for (const auto& msg : conversation) {
            nlohmann::json msg_json;
            msg_json["role"] = msg.role;
            msg_json["content"] = msg.content;
            
            auto time_point = std::chrono::system_clock::to_time_t(msg.timestamp);
            if (std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", std::gmtime(&time_point))) {
                msg_json["timestamp"] = std::string(buf);
            }
            conv_json.push_back(msg_json);
        }
        response["conversation"] = conv_json;
        return response;
    }

    void ChatController::clearConversation(const std::string& userId) {
        if (_memoryService) {
            _memoryService->clear_conversation(userId);
        }
    }

}
}
