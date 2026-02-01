#include "lily/services/HTTPServer.hpp"
#include "lily/services/ChatService.hpp"
#include "lily/services/Service.hpp"
#include "lily/services/WebSocketManager.hpp"
#include "lily/utils/SystemMetrics.hpp"
#include <cpprest/http_msg.h>
#include <iostream>
#include <chrono>
#include <ctime>
#include <vector>
#include <cstdint>
#include <fstream>
#include <sstream>

using namespace web;
using namespace web::http;
using namespace web::http::experimental::listener;

namespace lily {
namespace services {

// Helper function to read file content
std::string read_file_content(const std::string& file_path) {
    std::ifstream file(file_path);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open file: " + file_path);
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

HTTPServer::HTTPServer(const std::string& address, uint16_t port, ChatService& chat_service, MemoryService& memory_service, Service& tool_service, WebSocketManager& ws_manager)
    : _listener(uri_builder().set_scheme("http").set_host(address).set_port(port).to_uri()),
      _chat_service(chat_service),
      _memory_service(memory_service),
      _tool_service(tool_service),
      _ws_manager(ws_manager) {
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
                    lily::services::ChatResponse chat_response = _chat_service.handle_chat_message_with_audio(message, user_id, chat_params);

                    json::value response_json;
                    response_json["response"] = json::value::string(chat_response.text_response);
                    response_json["timestamp"] = json::value::string(utility::datetime::utc_now().to_string());
                    
                    
                    http_response response(status_codes::OK);
                    response.set_body(response_json);
                    response.headers().add("Access-Control-Allow-Origin", "*");
                    request.reply(response);
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

void HTTPServer::handle_get(http_request request) {
    // Add CORS headers to all responses
    http_response response;
    response.headers().add("Access-Control-Allow-Origin", "*");
    response.headers().add("Access-Control-Allow-Methods", "GET, POST, DELETE, OPTIONS");
    response.headers().add("Access-Control-Allow-Headers", "Content-Type");
    
    auto path = request.relative_uri().path();
    if (path == "/swagger.json") {
        try {
            // Read and serve the Swagger JSON file
            std::string swagger_json = read_file_content("include/lily/api/swagger.json");
            response.set_status_code(status_codes::OK);
            response.set_body(swagger_json);
            response.headers().set_content_type("application/json");
            request.reply(response);
            return;
        } catch (const std::exception& e) {
            response.set_status_code(status_codes::InternalError);
            response.set_body("Error loading Swagger documentation: " + std::string(e.what()));
            request.reply(response);
            return;
        }
    } else if (path == "/swagger-ui") {
        // Serve Swagger UI HTML page
        std::string html_content = R"(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <title>Lily Core API Documentation</title>
    <link rel="stylesheet" type="text/css" href="https://unpkg.com/swagger-ui-dist@3.25.0/swagger-ui.css">
</head>
<body>
    <div id="swagger-ui"></div>
    <script src="https://unpkg.com/swagger-ui-dist@3.25.0/swagger-ui-bundle.js"></script>
    <script>
        window.onload = function() {
            SwaggerUIBundle({
                url: '/swagger.json',
                dom_id: '#swagger-ui',
                presets: [
                    SwaggerUIBundle.presets.apis,
                    SwaggerUIBundle.presets.standalone
                ]
            });
        };
    </script>
</body>
</html>
)";
        response.set_status_code(status_codes::OK);
        response.set_body(html_content);
        response.headers().set_content_type("text/html");
        request.reply(response);
        return;
    } else if (path == "/monitoring") {
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
    } else if (path == "/health") {
        web::json::value response_json = web::json::value::object();
        response_json["status"] = web::json::value::string("ok");
        response_json["timestamp"] = web::json::value::string(utility::datetime::utc_now().to_string());
        
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