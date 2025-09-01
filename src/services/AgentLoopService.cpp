#include <lily/services/AgentLoopService.hpp>
#include <lily/services/MemoryService.hpp>
#include <lily/services/Service.hpp>
#include <cpprest/http_client.h>
#include <cpprest/json.h>
#include <iostream>
#include <cstdlib>
#include <sstream>

namespace lily {
    namespace services {
        AgentLoopService::AgentLoopService(MemoryService& memoryService, Service& toolService)
            : _memoryService(memoryService), _toolService(toolService) {}

        std::string AgentLoopService::run_loop(const std::string& user_message, const std::string& user_id) {
            std::cout << "Running agent loop for user: " << user_id << std::endl;

            const char* api_key = std::getenv("GEMINI_API_KEY");
            if (!api_key) {
                std::cerr << "GEMINI_API_KEY not set" << std::endl;
                return "Error: GEMINI_API_KEY not configured";
            }

            // Build conversation history
            auto conversation = _memoryService.get_conversation(user_id);

            web::json::value contents_json = web::json::value::array();
            size_t index = 0;

            for (const auto& msg : conversation) {
                web::json::value content_obj = web::json::value::object();
                // Map "assistant" role to "model" for Gemini API
                std::string role = (msg.role == "assistant") ? "model" : msg.role;
                content_obj["role"] = web::json::value::string(role);
                content_obj["parts"] = web::json::value::array(1);
                content_obj["parts"][0]["text"] = web::json::value::string(msg.content);
                contents_json[index++] = content_obj;
            }

            // Add current user message
            web::json::value current_content = web::json::value::object();
            current_content["role"] = web::json::value::string("user");
            current_content["parts"] = web::json::value::array(1);
            current_content["parts"][0]["text"] = web::json::value::string(user_message);
            contents_json[index] = current_content;

            // Build full request
            web::json::value request_json = web::json::value::object();
            request_json["contents"] = contents_json;

            // Make HTTP request to Gemini
            web::http::client::http_client client(U("https://generativelanguage.googleapis.com"));
            web::http::http_request request(web::http::methods::POST);

            std::string url = "/v1beta/models/gemini-2.5-flash:generateContent?key=" + std::string(api_key);
            request.set_request_uri(web::uri(url));
            request.set_body(request_json);

            try {
                pplx::task<web::http::http_response> response_task = client.request(request);
                response_task.wait();
                web::http::http_response response = response_task.get();

                if (response.status_code() != 200) {
                    std::cerr << "Gemini API error: " << response.status_code() << std::endl;
                    auto error_body = response.extract_string().get();
                    std::cerr << error_body << std::endl;
                    return "Error calling Gemini API";
                }

                auto json_response = response.extract_json().get();
                if (json_response.has_field("candidates") && json_response["candidates"].is_array() && json_response["candidates"].size() > 0) {
                    auto candidate = json_response["candidates"][0];
                    if (candidate.has_field("content")) {
                        auto content = candidate["content"];
                        if (content.has_field("parts") && content["parts"].is_array() && content["parts"].size() > 0) {
                            return content["parts"][0]["text"].as_string();
                        }
                    }
                }
            } catch (const std::exception& e) {
                std::cerr << "Error in Gemini request: " << e.what() << std::endl;
                return "Error processing request";
            }

            return "No response from Gemini";
        }
    }
}