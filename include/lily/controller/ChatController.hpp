#ifndef LILY_CONTROLLER_CHAT_CONTROLLER_HPP
#define LILY_CONTROLLER_CHAT_CONTROLLER_HPP

#include <string>
#include <functional>
#include <cpprest/http.h>
#include <nlohmann/json.hpp>

namespace lily {
namespace controller {

/**
 * @brief Request DTO for chat messages
 * 
 * Similar to Spring's @RequestBody DTO.
 */
struct ChatRequest {
    std::string message;
    std::string user_id;
    bool enable_tts = false;
    
    static ChatRequest fromJson(const nlohmann::json& json) {
        ChatRequest request;
        request.message = json.value("message", "");
        request.user_id = json.value("user_id", "default");
        request.enable_tts = json.value("enable_tts", false);
        return request;
    }
};

/**
 * @brief Response DTO for chat messages
 * 
 * Similar to Spring's @ResponseBody DTO.
 */
struct ChatResponse {
    std::string text;
    std::string conversation_id;
    bool success = true;
    std::string error_message;
    
    nlohmann::json toJson() const {
        nlohmann::json json;
        json["text"] = text;
        json["conversation_id"] = conversation_id;
        json["success"] = success;
        if (!success) {
            json["error_message"] = error_message;
        }
        return json;
    }
};

/**
 * @brief Chat Controller interface
 * 
 * Similar to Spring's @RestController, this defines
 * the contract for HTTP request handling.
 */
class IChatController {
public:
    virtual ~IChatController() = default;
    virtual web::http::http_request handleChat(web::http::http_request request) = 0;
    virtual web::http::http_request handleChatWithAudio(web::http::http_request request) = 0;
    virtual web::http::http_request handleHealth(web::http::http_request request) = 0;
};

/**
 * @brief REST Controller for chat operations
 * 
 * This class handles incoming HTTP requests and delegates
 * to the appropriate service layer, similar to Spring's
 * @RestController pattern.
 */
class ChatController : public IChatController {
public:
    // Function type for chat service dependency
    using ChatHandler = std::function<ChatResponse(const ChatRequest&)>;
    using HealthHandler = std::function<nlohmann::json()>;
    
    ChatController() = default;
    
    void setChatHandler(ChatHandler handler) {
        chat_handler_ = handler;
    }
    
    void setChatWithAudioHandler(ChatHandler handler) {
        chat_with_audio_handler_ = handler;
    }
    
    void setHealthHandler(HealthHandler handler) {
        health_handler_ = handler;
    }
    
    web::http::http_request handleChat(web::http::http_request request) override {
        try {
            auto json = request.extract_json().get();
            ChatRequest chat_request = ChatRequest::fromJson(json);
            
            if (chat_handler_) {
                ChatResponse response = chat_handler_(chat_request);
                return createJsonResponse(response.toJson(), web::http::status_codes::OK);
            }
            
            return createErrorResponse("Chat handler not configured", 
                                      web::http::status_codes::ServiceUnavailable);
        } catch (const std::exception& e) {
            return createErrorResponse(e.what(), web::http::status_codes::BadRequest);
        }
    }
    
    web::http::http_request handleChatWithAudio(web::http::http_request request) override {
        try {
            auto json = request.extract_json().get();
            ChatRequest chat_request = ChatRequest::fromJson(json);
            
            if (chat_with_audio_handler_) {
                ChatResponse response = chat_with_audio_handler_(chat_request);
                return createJsonResponse(response.toJson(), web::http::status_codes::OK);
            }
            
            return createErrorResponse("Audio chat handler not configured",
                                      web::http::status_codes::ServiceUnavailable);
        } catch (const std::exception& e) {
            return createErrorResponse(e.what(), web::http::status_codes::BadRequest);
        }
    }
    
    web::http::http_request handleHealth(web::http::http_request request) override {
        try {
            nlohmann::json health;
            health["status"] = "UP";
            health["service"] = "lily-core";
            
            if (health_handler_) {
                health = health_handler_();
            }
            
            return createJsonResponse(health, web::http::status_codes::OK);
        } catch (const std::exception& e) {
            return createErrorResponse(e.what(), web::http::status_codes::InternalError);
        }
    }

private:
    ChatHandler chat_handler_;
    ChatHandler chat_with_audio_handler_;
    HealthHandler health_handler_;
    
    web::http::http_request createJsonResponse(const nlohmann::json& json, 
                                                web::http::status_code status) {
        web::http::http_response response(status);
        response.headers().add(U("Content-Type"), U("application/json"));
        response.set_body(json.dump());
        return response;
    }
    
    web::http::http_request createErrorResponse(const std::string& message,
                                                 web::http::status_code status) {
        nlohmann::json error;
        error["success"] = false;
        error["error"] = message;
        return createJsonResponse(error, status);
    }
};

/**
 * @brief Controller utilities
 * 
 * Helper functions similar to Spring's utility classes.
 */
class ControllerUtils {
public:
    /**
     * @brief Extract path parameter from URI
     */
    static std::string getPathParam(const web::http::http_request& request, 
                                     const std::string& param_name) {
        auto path = request.relative_uri().path();
        // Simple path parameter extraction
        size_t start = path.find("/" + param_name + "/");
        if (start != std::string::npos) {
            size_t end = path.find("/", start + param_name.size() + 2);
            if (end != std::string::npos) {
                return path.substr(start + param_name.size() + 2, end - start - param_name.size() - 2);
            }
        }
        return "";
    }
    
    /**
     * @brief Extract query parameter from URI
     */
    static std::string getQueryParam(const web::http::http_request& request,
                                      const std::string& param_name) {
        auto query = request.relative_uri().query();
        // Simple query parameter extraction
        std::string search = param_name + "=";
        size_t start = query.find(search);
        if (start != std::string::npos) {
            size_t end = query.find("&", start);
            if (end == std::string::npos) {
                end = query.length();
            }
            return query.substr(start + search.size(), end - start - search.size());
        }
        return "";
    }
};

} // namespace controller
} // namespace lily

#endif // LILY_CONTROLLER_CHAT_CONTROLLER_HPP
