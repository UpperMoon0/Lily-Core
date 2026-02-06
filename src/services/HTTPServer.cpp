#include "lily/services/HTTPServer.hpp"
#include "lily/services/SessionService.hpp"
#include "lily/services/ChatService.hpp"
#include "lily/services/Service.hpp"
#include "lily/services/WebSocketManager.hpp"
#include "lily/services/AgentLoopService.hpp"
#include "lily/utils/SystemMetrics.hpp"
#include <cpprest/http_msg.h>
#include <iostream>
#include <chrono>
#include <ctime>
#include <vector>
#include <cstdint>
#include <fstream>
#include <sstream>
#include <pplx/pplxtasks.h>

using namespace web;
using namespace web::http;
using namespace web::http::experimental::listener;

namespace lily {
namespace services {

HTTPServer::HTTPServer(const std::string& address, uint16_t port, ChatService& chat_service, MemoryService& memory_service, Service& tool_service, WebSocketManager& ws_manager, AgentLoopService& agent_loop_service, SessionService& session_service, config::AppConfig& config)
    : _listener(uri_builder().set_scheme("http").set_host(address).set_port(port).to_uri()),
      _chat_service(chat_service),
      _memory_service(memory_service),
      _tool_service(tool_service),
      _ws_manager(ws_manager),
      _agent_loop_service(agent_loop_service),
      _session_service(session_service),
      _config(config) {
    _listener.support(methods::POST, std::bind(&HTTPServer::handle_post, this, std::placeholders::_1));
    _listener.support(methods::GET, std::bind(&HTTPServer::handle_get, this, std::placeholders::_1));
    _listener.support(methods::DEL, std::bind(&HTTPServer::handle_delete, this, std::placeholders::_1));
    _listener.support(methods::OPTIONS, std::bind(&HTTPServer::handle_options, this, std::placeholders::_1));
}

HTTPServer::~HTTPServer() {
    stop();
}

void HTTPServer::start() {
    _listener.open().wait();
    std::cout << "HTTP server started on " << _listener.uri().to_string() << std::endl;
}

void HTTPServer::stop() {
    _listener.close().wait();
    std::cout << "HTTP server stopped." << std::endl;
}

    void HTTPServer::handle_post(http_request request) {
    auto path = request.relative_uri().path();
    if (path == "/config") {
        request.extract_json().then([this, request](pplx::task<json::value> task) {
            try {
                const auto& json_value = task.get();
                if (json_value.has_field("gemini_api_key")) {
                    _config.setGeminiApiKey(json_value.at("gemini_api_key").as_string());
                }
                if (json_value.has_field("gemini_model")) {
                    _config.setGeminiModel(json_value.at("gemini_model").as_string());
                }
                
                _config.saveToFile();
                
                http_response response(status_codes::OK);
                response.set_body("Configuration updated");
                response.headers().add("Access-Control-Allow-Origin", "*");
                request.reply(response);
            } catch (const std::exception& e) {
                request.reply(status_codes::InternalError, e.what());
            }
        });
        return;
    }
    
    if (path == "/chat") {
        request.extract_json().then([this, request](pplx::task<json::value> task) {
            try {
                const auto& json_value = task.get();
                if (json_value.has_field("message") && json_value.has_field("user_id")) {
                    auto message = json_value.at("message").as_string();
                    auto user_id = json_value.at("user_id").as_string();
                    
                    lily::services::ChatParameters chat_params;
                    if (json_value.has_field("tts") && json_value.at("tts").is_object()) {
                        const auto& tts_json = json_value.at("tts");
                        chat_params.enable_tts = tts_json.has_field("enabled") ? tts_json.at("enabled").as_bool() : false;
                        
                        if (tts_json.has_field("params") && tts_json.at("params").is_object()) {
                            const auto& params_json = tts_json.at("params");
                            chat_params.tts_params.speaker = params_json.has_field("speaker") ? params_json.at("speaker").as_integer() : 0;
                            chat_params.tts_params.sample_rate = params_json.has_field("sample_rate") ? params_json.at("sample_rate").as_integer() : 24000;
                            chat_params.tts_params.model = params_json.has_field("model") ? params_json.at("model").as_string() : "edge";
                            chat_params.tts_params.lang = params_json.has_field("lang") ? params_json.at("lang").as_string() : "en-US";
                        }
                    }
                    
                    if (chat_params.enable_tts) {
                        std::cout << "TTS enabled for user_id: " << user_id << std::endl;
                    }

                    // Use Async Chat Processing
                    // Create a task completion event to signal when the processing is done
                    auto tce = pplx::task_completion_event<void>();

                    _chat_service.handle_chat_message_with_audio_async(message, user_id, chat_params, [request, tce](lily::services::ChatResponse chat_response) {
                        json::value response_json;
                        response_json["response"] = json::value::string(chat_response.text_response);
                        response_json["timestamp"] = json::value::string(utility::datetime::utc_now().to_string());
                        
                        http_response response(status_codes::OK);
                        response.set_body(response_json);
                        response.headers().add("Access-Control-Allow-Origin", "*");
                        request.reply(response);
                        
                        // Signal completion
                        tce.set();
                    });

                    // We don't wait here on the thread. The request is kept alive by the lambda capture.
                    // However, we need to ensure the request object survives until reply is called.
                    // Since 'request' is passed by value (copy), it should be fine.
                    // But cpprestsdk handlers usually expect the task chain to complete.
                    // Actually, request.reply() returns a task. We should probably return that?
                    // But handle_post returns void.
                    
                } else {
                    request.reply(status_codes::BadRequest, "Missing 'message' or 'user_id' field.");
                }
            } catch (const std::exception& e) {
                request.reply(status_codes::InternalError, e.what());
            }
        });
    } else if (path.compare(0, 12, "/conversation") == 0) {
        request.reply(status_codes::MethodNotAllowed);
    } else {
        http_response response(status_codes::NotFound);
        response.headers().add("Access-Control-Allow-Origin", "*");
        response.headers().add("Access-Control-Allow-Methods", "GET, POST, DELETE, OPTIONS");
        response.headers().add("Access-Control-Allow-Headers", "Content-Type");
        request.reply(response);
    }
}

// ... (rest of file remains same) ...
void HTTPServer::handle_get(http_request request) {
    // Add CORS headers to all responses
    http_response response;
    response.headers().add("Access-Control-Allow-Origin", "*");
    response.headers().add("Access-Control-Allow-Methods", "GET, POST, DELETE, OPTIONS");
    response.headers().add("Access-Control-Allow-Headers", "Content-Type");
    
    auto path = request.relative_uri().path();
    if (path == "/health") {
        web::json::value response_json = web::json::value::object();
        response_json["status"] = web::json::value::string("UP");
        
        http_response response(status_codes::OK);
        response.set_body(response_json);
        response.headers().add("Access-Control-Allow-Origin", "*");
        request.reply(response);
        return;
    }
    
    if (path == "/config") {
        web::json::value response_json = web::json::value::object();
        std::string api_key = _config.getGeminiApiKey();
        
        if (api_key.length() > 8) {
            response_json["gemini_api_key"] = web::json::value::string("..." + api_key.substr(api_key.length() - 4));
        } else if (!api_key.empty()) {
            response_json["gemini_api_key"] = web::json::value::string("********");
        } else {
            response_json["gemini_api_key"] = web::json::value::string("");
        }
        
        response_json["gemini_model"] = web::json::value::string(_config.getGeminiModel());
        
        http_response response(status_codes::OK);
        response.set_body(response_json);
        response.headers().add("Access-Control-Allow-Origin", "*");
        request.reply(response);
        return;
    }

    if (path == "/monitoring") {
        try {
            lily::utils::SystemMetricsCollector metrics_collector;
            auto monitoring_data = metrics_collector.get_monitoring_data("Lily-Core", "1.0.0");
            
            web::json::value response_json = web::json::value::object();
            response_json["status"] = web::json::value::string(monitoring_data.status);
            response_json["service_name"] = web::json::value::string(monitoring_data.service_name);
            response_json["version"] = web::json::value::string(monitoring_data.version);
            response_json["timestamp"] = web::json::value::string(monitoring_data.timestamp);
            
            web::json::value metrics_json = web::json::value::object();
            metrics_json["cpu_usage"] = web::json::value::number(monitoring_data.metrics.cpu_usage);
            metrics_json["memory_usage"] = web::json::value::number(monitoring_data.metrics.memory_usage);
            metrics_json["disk_usage"] = web::json::value::number(monitoring_data.metrics.disk_usage);
            metrics_json["uptime"] = web::json::value::string(monitoring_data.metrics.uptime);
            response_json["metrics"] = metrics_json;
            
            web::json::value details_json = web::json::value::object();
            for (const auto& detail : monitoring_data.details) {
                details_json[detail.first] = web::json::value::string(detail.second);
            }
            response_json["details"] = details_json;
            
            http_response response(status_codes::OK);
            response.set_body(response_json);
            response.headers().add("Access-Control-Allow-Origin", "*");
            request.reply(response);
        } catch (const std::exception& e) {
            request.reply(status_codes::InternalError, e.what());
        }
    } else if (path.compare(0, 13, "/conversation") == 0) {
        std::string path_str = path;
        size_t pos = path_str.find_last_of('/');
        if (pos != std::string::npos && pos < path_str.size() - 1) {
            std::string user_id = path_str.substr(pos + 1);

            const auto& conversation = _memory_service.get_conversation(user_id);

            web::json::value response_json = web::json::value::object();
            response_json["user_id"] = web::json::value::string(user_id);
            response_json["conversation"] = web::json::value::array(conversation.size());

            size_t idx = 0;
            for (const auto& msg : conversation) {
                web::json::value msg_json = web::json::value::object();
                msg_json["role"] = web::json::value::string(msg.role);
                msg_json["content"] = web::json::value::string(msg.content);

                auto time_point = std::chrono::system_clock::to_time_t(msg.timestamp);
                char buf[100];
                if (std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", std::gmtime(&time_point))) {
                    msg_json["timestamp"] = web::json::value::string(std::string(buf));
                }

                response_json["conversation"][idx++] = msg_json;
            }

            http_response response(status_codes::OK);
            response.set_body(response_json);
            response.headers().add("Access-Control-Allow-Origin", "*");
            request.reply(response);
        } else {
            request.reply(status_codes::BadRequest, "Invalid conversation path");
        }
    } else if (path == "/connected-users") {
        // Get all connected user IDs from WebSocketManager
        auto user_ids = _ws_manager.get_connected_user_ids();
        
        web::json::value response_json = web::json::value::object();
        web::json::value user_ids_json = web::json::value::array(user_ids.size());
        
        for (size_t i = 0; i < user_ids.size(); ++i) {
            user_ids_json[i] = web::json::value::string(user_ids[i]);
        }
        
        response_json["user_ids"] = user_ids_json;
        response_json["count"] = web::json::value::number(user_ids.size());
        response_json["timestamp"] = web::json::value::string(utility::datetime::utc_now().to_string());
        
        http_response response(status_codes::OK);
        response.set_body(response_json);
        response.headers().add("Access-Control-Allow-Origin", "*");
        request.reply(response);
    } else if (path == "/agent-loops") {
        // Get the last agent loop information
        const auto& last_loop = _agent_loop_service.get_last_agent_loop();
        
        web::json::value response_json = web::json::value::object();
        
        if (last_loop.user_id.empty()) {
            // No agent loops exist
            response_json["exists"] = web::json::value::boolean(false);
            response_json["message"] = web::json::value::string("No agent loops available");
        } else {
            response_json["exists"] = web::json::value::boolean(true);
            response_json["user_id"] = web::json::value::string(last_loop.user_id);
            response_json["user_message"] = web::json::value::string(last_loop.user_message);
            response_json["final_response"] = web::json::value::string(last_loop.final_response);
            response_json["completed"] = web::json::value::boolean(last_loop.completed);
            
            // Convert timestamps
            auto start_time = std::chrono::system_clock::to_time_t(last_loop.start_time);
            auto end_time = std::chrono::system_clock::to_time_t(last_loop.end_time);
            char buf[100];
            
            if (std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", std::gmtime(&start_time))) {
                response_json["start_time"] = web::json::value::string(std::string(buf));
            }
            
            if (std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", std::gmtime(&end_time))) {
                response_json["end_time"] = web::json::value::string(std::string(buf));
            }
            
            // Add duration
            response_json["duration_seconds"] = web::json::value::number(last_loop.duration_seconds);
            
            // Add steps
            web::json::value steps_json = web::json::value::array(last_loop.steps.size());
            for (size_t i = 0; i < last_loop.steps.size(); ++i) {
                const auto& step = last_loop.steps[i];
                web::json::value step_json = web::json::value::object();
                
                step_json["step_number"] = web::json::value::number(step.step_number);
                
                // Convert step type to string
                std::string step_type_str;
                switch (step.type) {
                    case lily::models::AgentStepType::THINKING:
                        step_type_str = "thinking";
                        break;
                    case lily::models::AgentStepType::TOOL_CALL:
                        step_type_str = "tool_call";
                        break;
                    case lily::models::AgentStepType::RESPONSE:
                        step_type_str = "response";
                        break;
                    default:
                        step_type_str = "unknown";
                }
                step_json["type"] = web::json::value::string(step_type_str);
                
                step_json["reasoning"] = web::json::value::string(step.reasoning);
                step_json["tool_name"] = web::json::value::string(step.tool_name);
                
                // Convert tool parameters
                std::string tool_params_str = step.tool_parameters.dump();
                step_json["tool_parameters"] = web::json::value::parse(utility::conversions::to_string_t(tool_params_str));
                
                // Convert tool result
                std::string tool_result_str = step.tool_result.dump();
                step_json["tool_result"] = web::json::value::parse(utility::conversions::to_string_t(tool_result_str));
                
                // Convert timestamp
                auto step_time = std::chrono::system_clock::to_time_t(step.timestamp);
                if (std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", std::gmtime(&step_time))) {
                    step_json["timestamp"] = web::json::value::string(std::string(buf));
                }
                
                steps_json[i] = step_json;
            }
            response_json["steps"] = steps_json;
        }
        
        http_response response(status_codes::OK);
        response.set_body(response_json);
        response.headers().add("Access-Control-Allow-Origin", "*");
        request.reply(response);
    } else if (path == "/active-sessions") {
        auto sessions = _session_service.get_all_sessions();
        
        web::json::value response_json = web::json::value::object();
        web::json::value sessions_json = web::json::value::array(sessions.size());
        
        size_t idx = 0;
        char buf[100];
        
        for (const auto& session : sessions) {
            web::json::value session_json = web::json::value::object();
            session_json["user_id"] = web::json::value::string(session.user_id);
            session_json["active"] = web::json::value::boolean(session.active);
            
            auto start_time = std::chrono::system_clock::to_time_t(session.start_time);
            if (std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", std::gmtime(&start_time))) {
                session_json["start_time"] = web::json::value::string(std::string(buf));
            }
            
            auto last_activity = std::chrono::system_clock::to_time_t(session.last_activity);
            if (std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", std::gmtime(&last_activity))) {
                session_json["last_activity"] = web::json::value::string(std::string(buf));
            }
            
            // Calculate duration in minutes
            auto now = std::chrono::system_clock::now();
            auto duration_mins = std::chrono::duration_cast<std::chrono::minutes>(now - session.start_time).count();
            session_json["duration_minutes"] = web::json::value::number(duration_mins);
            
            sessions_json[idx++] = session_json;
        }
        
        response_json["sessions"] = sessions_json;
        response_json["count"] = web::json::value::number(sessions.size());
        
        http_response response(status_codes::OK);
        response.set_body(response_json);
        response.headers().add("Access-Control-Allow-Origin", "*");
        request.reply(response);
    } else {
        http_response response(status_codes::NotFound);
        response.headers().add("Access-Control-Allow-Origin", "*");
        response.headers().add("Access-Control-Allow-Methods", "GET, POST, DELETE, OPTIONS");
        response.headers().add("Access-Control-Allow-Headers", "Content-Type");
        request.reply(response);
    }
}

void HTTPServer::handle_delete(http_request request) {
    auto path = request.relative_uri().path();
    if (path.compare(0, 13, "/conversation") == 0) {
        std::string path_str = path;
        size_t pos = path_str.find_last_of('/');
        if (pos != std::string::npos && pos < path_str.size() - 1) {
            std::string user_id = path_str.substr(pos + 1);

            _memory_service.clear_conversation(user_id);

            http_response response(status_codes::OK);
            response.headers().add("Access-Control-Allow-Origin", "*");
            request.reply(response);
        } else {
            request.reply(status_codes::BadRequest, "Invalid conversation path");
        }
    } else {
        http_response response(status_codes::NotFound);
        response.headers().add("Access-Control-Allow-Origin", "*");
        response.headers().add("Access-Control-Allow-Methods", "GET, POST, DELETE, OPTIONS");
        response.headers().add("Access-Control-Allow-Headers", "Content-Type");
        request.reply(response);
    }
}

void HTTPServer::handle_options(http_request request) {
    http_response response(status_codes::OK);
    response.headers().add("Access-Control-Allow-Origin", "*");
    response.headers().add("Access-Control-Allow-Methods", "GET, POST, DELETE, OPTIONS");
    response.headers().add("Access-Control-Allow-Headers", "Content-Type");
    request.reply(response);
}

} 
}
