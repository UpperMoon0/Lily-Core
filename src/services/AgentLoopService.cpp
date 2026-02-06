#include <lily/services/AgentLoopService.hpp>
#include <lily/services/MemoryService.hpp>
#include <lily/services/Service.hpp>
#include <lily/models/AgentLoop.hpp>
#include <cpprest/http_client.h>
#include <cpprest/json.h>
#include <iostream>
#include <cstdlib>
#include <sstream>
#include <mutex>
#include <nlohmann/json.hpp>

namespace lily {
    namespace services {
        AgentLoopService::AgentLoopService(MemoryService& memoryService, Service& toolService, config::AppConfig& config)
            : _memoryService(memoryService), _toolService(toolService), _config(config) {}

        std::string AgentLoopService::run_loop(const std::string& user_message, const std::string& user_id) {
            std::string api_key = _config.getGeminiApiKey();
            if (api_key.empty()) {
                std::cerr << "GEMINI_API_KEY not configured" << std::endl;
                return "Error: GEMINI_API_KEY not configured";
            }

            // Create a new agent loop
            lily::models::AgentLoop current_loop;
            current_loop.user_id = user_id;
            current_loop.user_message = user_message;
            current_loop.start_time = std::chrono::system_clock::now();
            current_loop.completed = false;

            std::cout << "[AGENT LOOP] Starting agent loop for user: " << user_id << std::endl;
            std::cout << "[AGENT LOOP] User message: " << user_message << std::endl;

            // Process the message with step-based agent loop
            std::string response = process_with_tools(user_message, user_id, current_loop);

            // Complete the agent loop
            current_loop.end_time = std::chrono::system_clock::now();
            current_loop.final_response = response;
            current_loop.completed = true;
            
            // Calculate duration in seconds
            auto duration = std::chrono::duration_cast<std::chrono::duration<double>>(
                current_loop.end_time - current_loop.start_time
            );
            current_loop.duration_seconds = duration.count();

            std::cout << "[AGENT LOOP] Completed agent loop with final response: " << response << std::endl;
            std::cout << "[AGENT LOOP] Total steps executed: " << current_loop.steps.size() << std::endl;
            std::cout << "[AGENT LOOP] Total time taken: " << current_loop.duration_seconds << " seconds" << std::endl;

            // Store the agent loop
            {
                std::lock_guard<std::mutex> lock(_agentLoopsMutex);
                _agentLoops.push_back(current_loop);
                // Keep only the last 10 agent loops to prevent memory issues
                if (_agentLoops.size() > 10) {
                    _agentLoops.erase(_agentLoops.begin());
                }
            }

            return response;
        }

        std::string AgentLoopService::process_with_tools(const std::string& user_message, const std::string& user_id, lily::models::AgentLoop& current_loop) {
            // Get available tools
            auto available_tools = _toolService.get_available_tools();
            std::cout << "[AGENT LOOP] Available tools count: " << available_tools.size() << std::endl;
            
            // Build conversation context
            auto conversation = _memoryService.get_conversation(user_id);
            std::string context = "Conversation history:\n";
            for (const auto& msg : conversation) {
                context += msg.role + ": " + msg.content + "\n";
            }
            context += "Current user message: " + user_message + "\n";

            int step_number = 1;
            std::string current_context = context;
            std::string final_response;

            std::cout << "[AGENT LOOP] Starting step-based processing" << std::endl;

            // Agent loop: continue until LLM decides to respond
            while (true) {
                std::cout << "[AGENT LOOP] Executing step " << step_number << std::endl;
                std::string step_result = execute_agent_step(available_tools, current_context, current_loop, step_number);
                
                if (step_result.find("FINAL_RESPONSE:") == 0) {
                    // LLM decided to give final response
                    final_response = step_result.substr(15); // Remove "FINAL_RESPONSE:" prefix
                    std::cout << "[AGENT LOOP] Step " << step_number << ": LLM decided to give final response" << std::endl;
                    break;
                } else {
                    // Tool was called, add result to context and continue
                    std::cout << "[AGENT LOOP] Step " << step_number << ": Tool executed, result: " << step_result << std::endl;
                    current_context += "\nTool execution result: " + step_result + "\n";
                    step_number++;
                    
                    // Add safety check to prevent infinite loops
                    if (step_number > 20) {
                        std::cerr << "[AGENT LOOP] WARNING: Exceeded maximum step limit (20), breaking loop" << std::endl;
                        final_response = "I'm having trouble processing this request. Please try again with a simpler question.";
                        break;
                    }
                }
            }

            std::cout << "[AGENT LOOP] Processing completed after " << (step_number - 1) << " steps" << std::endl;
            return final_response;
        }

        std::string AgentLoopService::execute_agent_step(const std::vector<nlohmann::json>& available_tools,
                                                     const std::string& context,
                                                     lily::models::AgentLoop& current_loop,
                                                     int step_number) {
            // Create step
            lily::models::AgentStep step;
            step.step_number = step_number;
            step.timestamp = std::chrono::system_clock::now();

            // Build prompt for LLM with tool information
            std::string prompt = "You are an AI assistant with access to tools. Analyze the user's request and decide whether to use a tool or provide a response directly.\n\n";
            prompt += "Context:\n" + context + "\n\n";
            
            prompt += "Available tools:\n";
            for (const auto& tool : available_tools) {
                prompt += "- " + tool["name"].get<std::string>() + ": " + tool.value("description", "No description") + "\n";
            }

            prompt += R"(
Instructions:
1. Think step by step about whether a tool is needed
2. If a tool is needed, respond with: TOOL_CALL:{"tool_name": "tool_name", "reasoning": "your reasoning", "parameters": {}}
3. If no tool is needed, respond with: FINAL_RESPONSE: your final response to the user

Your response must be in JSON format if using a tool, or start with FINAL_RESPONSE: if giving a direct response.
)";

            std::cout << "[AGENT LOOP] Step " << step_number << ": Sending prompt to Gemini" << std::endl;
            std::cout << "[AGENT LOOP] Prompt length: " << prompt.length() << " characters" << std::endl;

            // Call Gemini with the prompt
            auto response = call_gemini_with_tools(prompt, available_tools);
            
            std::cout << "[AGENT LOOP] Step " << step_number << ": Received response from Gemini" << std::endl;
            
            if (response.contains("candidates") && response["candidates"].is_array() && response["candidates"].size() > 0) {
                auto candidate = response["candidates"][0];
                if (candidate.contains("content")) {
                    auto content = candidate["content"];
                    if (content.contains("parts") && content["parts"].is_array() && content["parts"].size() > 0) {
                        std::string llm_response = content["parts"][0]["text"].get<std::string>();
                        std::cout << "[AGENT LOOP] Step " << step_number << ": LLM response: " << llm_response << std::endl;

                        if (llm_response.find("TOOL_CALL:") == 0) {
                            // Parse tool call
                            step.type = lily::models::AgentStepType::TOOL_CALL;
                            step.reasoning = "Decided to use tool based on analysis";
                            
                            try {
                                std::string json_str = llm_response.substr(10);
                                nlohmann::json tool_call = nlohmann::json::parse(json_str);
                                
                                step.tool_name = tool_call["tool_name"];
                                step.reasoning = tool_call.value("reasoning", "No reasoning provided");
                                step.tool_parameters = tool_call.value("parameters", nlohmann::json::object());
                                
                                std::cout << "[AGENT LOOP] Step " << step_number << ": Calling tool: " << step.tool_name << std::endl;
                                std::cout << "[AGENT LOOP] Step " << step_number << ": Tool parameters: " << step.tool_parameters.dump() << std::endl;
                                
                                // Execute the tool
                                step.tool_result = _toolService.execute_tool(step.tool_name, step.tool_parameters);
                                
                                std::cout << "[AGENT LOOP] Step " << step_number << ": Tool result: " << step.tool_result.dump() << std::endl;
                                
                                // Add step to loop
                                current_loop.steps.push_back(step);
                                
                                return step.tool_result.dump();
                            } catch (const std::exception& e) {
                                std::cerr << "[AGENT LOOP] Step " << step_number << ": Error parsing tool call: " << e.what() << std::endl;
                                step.type = lily::models::AgentStepType::THINKING;
                                step.reasoning = "Error parsing tool call: " + std::string(e.what());
                                current_loop.steps.push_back(step);
                                return "Error: Failed to parse tool call";
                            }
                        } else if (llm_response.find("FINAL_RESPONSE:") == 0) {
                            // Final response
                            std::cout << "[AGENT LOOP] Step " << step_number << ": LLM decided to give final response" << std::endl;
                            step.type = lily::models::AgentStepType::RESPONSE;
                            step.reasoning = "Decided to provide direct response";
                            current_loop.steps.push_back(step);
                            return llm_response;
                        } else {
                            // Thinking step
                            std::cout << "[AGENT LOOP] Step " << step_number << ": LLM returned thinking response" << std::endl;
                            step.type = lily::models::AgentStepType::THINKING;
                            step.reasoning = llm_response;
                            current_loop.steps.push_back(step);
                            return "Continue analysis";
                        }
                    } else {
                        std::cerr << "[AGENT LOOP] Step " << step_number << ": No parts in content" << std::endl;
                    }
                } else {
                    std::cerr << "[AGENT LOOP] Step " << step_number << ": No content in candidate" << std::endl;
                }
            } else {
                std::cerr << "[AGENT LOOP] Step " << step_number << ": No candidates in response or empty candidates array" << std::endl;
                std::cout << "[AGENT LOOP] Step " << step_number << ": Full response: " << response.dump() << std::endl;
            }

            // Fallback: thinking step
            std::cout << "[AGENT LOOP] Step " << step_number << ": Falling back to thinking step" << std::endl;
            step.type = lily::models::AgentStepType::THINKING;
            step.reasoning = "Analyzing request...";
            current_loop.steps.push_back(step);
            return "Continue analysis";
        }

        // Helper function to convert MCP tool schema to Gemini tool schema
        web::json::value convert_mcp_tool_to_gemini_format(const nlohmann::json& mcp_tool) {
            web::json::value gemini_tool = web::json::value::object();
            
            // Gemini expects function declarations with specific fields
            web::json::value function_decl = web::json::value::object();
            
            // Map MCP tool fields to Gemini function declaration fields
            if (mcp_tool.contains("name")) {
                function_decl["name"] = web::json::value::string(mcp_tool["name"].get<std::string>());
            }
            
            if (mcp_tool.contains("description")) {
                function_decl["description"] = web::json::value::string(mcp_tool["description"].get<std::string>());
            }
            
            // Convert inputSchema to parameters
            if (mcp_tool.contains("inputSchema") && mcp_tool["inputSchema"].is_object()) {
                web::json::value parameters = web::json::value::object();
                parameters["type"] = web::json::value::string("OBJECT");
                
                web::json::value properties = web::json::value::object();
                web::json::value required = web::json::value::array();
                
                const auto& input_schema = mcp_tool["inputSchema"];
                if (input_schema.contains("properties") && input_schema["properties"].is_object()) {
                    const auto& props = input_schema["properties"];
                    for (auto it = props.begin(); it != props.end(); ++it) {
                        web::json::value prop = web::json::value::object();
                        prop["type"] = web::json::value::string(it.value().value("type", "string"));
                        
                        if (it.value().contains("description")) {
                            prop["description"] = web::json::value::string(it.value()["description"].get<std::string>());
                        }
                        
                        properties[utility::conversions::to_string_t(it.key())] = prop;
                    }
                }
                
                if (input_schema.contains("required") && input_schema["required"].is_array()) {
                    const auto& req = input_schema["required"];
                    for (size_t i = 0; i < req.size(); i++) {
                        required[i] = web::json::value::string(req[i].get<std::string>());
                    }
                }
                
                parameters["properties"] = properties;
                if (required.size() > 0) {
                    parameters["required"] = required;
                }
                
                function_decl["parameters"] = parameters;
            }
            
            gemini_tool["functionDeclarations"] = web::json::value::array(1);
            gemini_tool["functionDeclarations"][0] = function_decl;
            
            return gemini_tool;
        }

        nlohmann::json AgentLoopService::call_gemini_with_tools(const std::string& prompt, const std::vector<nlohmann::json>& tools) {
            std::string api_key = _config.getGeminiApiKey();
            if (api_key.empty()) {
                std::cerr << "[GEMINI API] Error: GEMINI_API_KEY not configured" << std::endl;
                return nlohmann::json::object();
            }
            
            std::string model = _config.getGeminiModel();
            if (model.empty()) {
                model = "gemini-2.5-flash"; // Fallback
            }

            web::http::client::http_client client(U("https://generativelanguage.googleapis.com"));
            web::http::http_request request(web::http::methods::POST);

            std::string url = "/v1beta/models/" + model + ":generateContent?key=" + api_key;
            request.set_request_uri(web::uri(url));

            // Build request with tools
            web::json::value request_json = web::json::value::object();
            
            web::json::value contents_json = web::json::value::array(1);
            web::json::value content_obj = web::json::value::object();
            content_obj["role"] = web::json::value::string("user");
            content_obj["parts"] = web::json::value::array(1);
            content_obj["parts"][0]["text"] = web::json::value::string(prompt);
            contents_json[0] = content_obj;
            
            request_json["contents"] = contents_json;

            // Add tools to the request if available
            if (!tools.empty()) {
                web::json::value tools_json = web::json::value::array(tools.size());
                for (size_t i = 0; i < tools.size(); i++) {
                    tools_json[i] = convert_mcp_tool_to_gemini_format(tools[i]);
                }
                request_json["tools"] = tools_json;
                std::cout << "[GEMINI API] Sending request with " << tools.size() << " tools" << std::endl;
            } else {
                std::cout << "[GEMINI API] Sending request without tools" << std::endl;
            }

            request.set_body(request_json);

            try {
                std::cout << "[GEMINI API] Calling Gemini API..." << std::endl;
                pplx::task<web::http::http_response> response_task = client.request(request);
                response_task.wait();
                web::http::http_response response = response_task.get();

                std::cout << "[GEMINI API] Response status: " << response.status_code() << std::endl;

                if (response.status_code() == 200) {
                    auto json_response = response.extract_json().get();
                    std::string response_str = utility::conversions::to_utf8string(json_response.serialize());
                    std::cout << "[GEMINI API] Successfully received response from Gemini" << std::endl;
                    return nlohmann::json::parse(response_str);
                } else {
                    std::cerr << "[GEMINI API] Error: HTTP status " << response.status_code() << std::endl;
                    auto error_body = response.extract_string().get();
                    std::cerr << "[GEMINI API] Error response: " << error_body << std::endl;
                }
            } catch (const std::exception& e) {
                std::cerr << "[GEMINI API] Error calling Gemini: " << e.what() << std::endl;
            }

            return nlohmann::json::object();
        }

        const lily::models::AgentLoop& AgentLoopService::get_last_agent_loop() const {
            std::lock_guard<std::mutex> lock(_agentLoopsMutex);
            if (_agentLoops.empty()) {
                static lily::models::AgentLoop empty_loop;
                return empty_loop;
            }
            return _agentLoops.back();
        }

        void AgentLoopService::clear_agent_loops() {
            std::lock_guard<std::mutex> lock(_agentLoopsMutex);
            _agentLoops.clear();
        }

        std::vector<lily::models::AgentLoop> AgentLoopService::get_agent_loops() const {
            std::lock_guard<std::mutex> lock(_agentLoopsMutex);
            return _agentLoops;
        }
    }
}