#include "lily/services/HTTPServer.hpp"
#include "lily/services/ChatService.hpp"
#include "lily/services/ToolService.hpp"
#include "lily/utils/SystemMetrics.hpp"
#include <cpprest/http_msg.h>
#include <iostream>
#include <chrono>
#include <ctime>

using namespace web;
using namespace web::http;
using namespace web::http::experimental::listener;

namespace lily {
namespace services {

HTTPServer::HTTPServer(const std::string& address, uint16_t port, ChatService& chat_service, MemoryService& memory_service, ToolService& tool_service)
    : _listener(uri_builder().set_scheme("http").set_host(address).set_port(port).to_uri()),
      _chat_service(chat_service),
      _memory_service(memory_service),
      _tool_service(tool_service) {
    _listener.support(methods::POST, std::bind(&HTTPServer::handle_post, this, std::placeholders::_1));
    _listener.support(methods::GET, std::bind(&HTTPServer::handle_get, this, std::placeholders::_1));
    _listener.support(methods::GET, std::bind(&HTTPServer::handle_monitoring, this, std::placeholders::_1));
    // _listener.support(methods::DELETE, std::bind(&HTTPServer::handle_delete, this, std::placeholders::_1));
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
                    
                    std::string response_message = _chat_service.handle_chat_message(message, user_id);

                    json::value response_json;
                    response_json["response"] = json::value::string(response_message);
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
        // POST for /conversation is not used, UI uses GET/DELETE
        request.reply(status_codes::MethodNotAllowed);
    } else {
        request.reply(status_codes::NotFound);
    }
}

void HTTPServer::handle_get(http_request request) {
    auto path = request.relative_uri().path();
    if (path.compare(0, 12, "/conversation") == 0) {
        // Extract user_id from path /conversation/{user_id}
        std::string path_str = path;
        size_t pos = path_str.find_last_of('/');
        if (pos != std::string::npos && pos < path_str.size() - 1) {
            std::string user_id = path_str.substr(pos + 1);

            // Get conversation from memory service
            const auto& conversation = _memory_service.get_conversation(user_id);

            // Build JSON response
            web::json::value response_json = web::json::value::object();
            response_json["user_id"] = web::json::value::string(user_id);
            response_json["conversation"] = web::json::value::array(conversation.size());

            size_t idx = 0;
            for (const auto& msg : conversation) {
                web::json::value msg_json = web::json::value::object();
                msg_json["role"] = web::json::value::string(msg.role);
                msg_json["content"] = web::json::value::string(msg.content);

                // Convert timestamp to ISO string
                auto time_t = std::chrono::system_clock::to_time_t(msg.timestamp);
                char buf[30];
                std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", std::gmtime(&time_t));
                msg_json["timestamp"] = web::json::value::string(buf);

                response_json["conversation"][idx++] = msg_json;
            }

            http_response response(status_codes::OK);
            response.set_body(response_json);
            response.headers().add("Access-Control-Allow-Origin", "*");
            request.reply(response);
        } else {
            request.reply(status_codes::BadRequest, "Invalid conversation path");
        }
    } else {
        request.reply(status_codes::NotFound);
    }
}

void HTTPServer::handle_delete(http_request request) {
    auto path = request.relative_uri().path();
    if (path.compare(0, 12, "/conversation") == 0) {
        // Extract user_id from path /conversation/{user_id}
        std::string path_str = path;
        size_t pos = path_str.find_last_of('/');
        if (pos != std::string::npos && pos < path_str.size() - 1) {
            std::string user_id = path_str.substr(pos + 1);

            // Clear conversation
            _memory_service.clear_conversation(user_id);

            http_response response(status_codes::OK);
            response.headers().add("Access-Control-Allow-Origin", "*");
            request.reply(response);
        } else {
            request.reply(status_codes::BadRequest, "Invalid conversation path");
        }
    } else {
        request.reply(status_codes::NotFound);
    }
}

void HTTPServer::handle_options(http_request request) {
    http_response response(status_codes::OK);
    response.headers().add("Access-Control-Allow-Origin", "*");
    response.headers().add("Access-Control-Allow-Methods", "GET, POST, DELETE, OPTIONS");
    response.headers().add("Access-Control-Allow-Headers", "Content-Type");
    request.reply(response);
}

void HTTPServer::handle_monitoring(http_request request) {
    auto path = request.relative_uri().path();
    if (path == "/monitoring") {
        try {
            // Create metrics collector
            lily::utils::SystemMetricsCollector metrics_collector;
            
            // Get monitoring data
            auto monitoring_data = metrics_collector.get_monitoring_data("Lily-Core", "1.0.0", &_tool_service);
            
            // Build JSON response
            web::json::value response_json = web::json::value::object();
            response_json["status"] = web::json::value::string(monitoring_data.status);
            response_json["service_name"] = web::json::value::string(monitoring_data.service_name);
            response_json["version"] = web::json::value::string(monitoring_data.version);
            response_json["timestamp"] = web::json::value::string(monitoring_data.timestamp);
            
            // Add metrics
            web::json::value metrics_json = web::json::value::object();
            metrics_json["cpu_usage"] = web::json::value::number(monitoring_data.metrics.cpu_usage);
            metrics_json["memory_usage"] = web::json::value::number(monitoring_data.metrics.memory_usage);
            metrics_json["disk_usage"] = web::json::value::number(monitoring_data.metrics.disk_usage);
            metrics_json["uptime"] = web::json::value::string(monitoring_data.metrics.uptime);
            response_json["metrics"] = metrics_json;
            
            // Add services
            web::json::value services_json = web::json::value::array();
            for (size_t i = 0; i < monitoring_data.services.size(); ++i) {
                const auto& service = monitoring_data.services[i];
                web::json::value service_json = web::json::value::object();
                service_json["name"] = web::json::value::string(service.name);
                service_json["status"] = web::json::value::string(service.status);
                
                // Add details
                web::json::value details_json = web::json::value::object();
                for (const auto& detail : service.details) {
                    details_json[detail.first] = web::json::value::string(detail.second);
                }
                service_json["details"] = details_json;
                service_json["last_updated"] = web::json::value::string(service.last_updated);
                
                services_json[i] = service_json;
            }
            response_json["services"] = services_json;
            
            // Add details
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
    } else {
        request.reply(status_codes::NotFound);
    }
}

} // namespace services
} // namespace lily