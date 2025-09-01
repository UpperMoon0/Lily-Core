#include <lily/services/ToolService.hpp>
#include <iostream>
#include <fstream>
#include <nlohmann/json.hpp>
#include <cpprest/http_client.h>
#include <cpprest/filestream.h>
#include <thread>
#include <chrono>

using namespace web;
using namespace web::http;
using namespace web::http::client;

namespace lily {
    namespace services {
        ToolService::ToolService() : _discovery_running(false) {
            load_tool_servers();
            discover_tools();
        }

        ToolService::~ToolService() {
            stop_periodic_discovery();
        }

        void ToolService::load_tool_servers() {
            try {
                std::ifstream file("tool_servers.json");
                if (file.is_open()) {
                    nlohmann::json j;
                    file >> j;
                    if (j.contains("tool_servers") && j["tool_servers"].is_array()) {
                        for (const auto& server : j["tool_servers"]) {
                            if (server.is_string()) {
                                _tool_servers.push_back(server.get<std::string>());
                            }
                        }
                    }
                    file.close();
                } else {
                    std::cerr << "Failed to open tool_servers.json" << std::endl;
                }
            } catch (const std::exception& e) {
                std::cerr << "Error loading tool servers: " << e.what() << std::endl;
            }
        }

        void ToolService::discover_tools() {
            _tools.clear();
            _discovered_servers.clear();
            _tools_per_server.clear();
            
            for (const auto& server_url : _tool_servers) {
                try {
                    auto tools = discover_tools_from_server(server_url);
                    _tools.insert(_tools.end(), tools.begin(), tools.end());
                    _discovered_servers.push_back(server_url);
                    _tools_per_server[server_url] = tools;
                } catch (const std::exception& e) {
                    std::cerr << "Failed to discover tools from " << server_url << ": " << e.what() << std::endl;
                }
            }
        }

        std::vector<nlohmann::json> ToolService::discover_tools_from_server(const std::string& server_url) {
            std::vector<nlohmann::json> tools;
            
            try {
                // Create HTTP client
                http_client client(U(server_url));
                
                // Prepare MCP tools/list request
                json::value request;
                request[U("jsonrpc")] = json::value::string(U("2.0"));
                request[U("method")] = json::value::string(U("tools/list"));
                request[U("id")] = json::value::number(1);
                
                // Send request
                auto response = client.request(methods::POST, U("/mcp"), request).get();
                
                if (response.status_code() == status_codes::OK) {
                    auto response_json = response.extract_json().get();
                    
                    if (response_json.has_field(U("result")) &&
                        response_json[U("result")].has_field(U("tools"))) {
                        
                        const auto& tools_array = response_json[U("result")][U("tools")].as_array();
                        
                        for (const auto& tool : tools_array) {
                            // Directly parse cpprest JSON to nlohmann JSON
                            tools.push_back(nlohmann::json::parse(utility::conversions::to_utf8string(tool.serialize())));
                        }
                    }
                }
            } catch (const std::exception& e) {
                std::cerr << "Error discovering tools from " << server_url << ": " << e.what() << std::endl;
                throw;
            }
            
            return tools;
        }

        void ToolService::start_periodic_discovery() {
            if (_discovery_running) {
                return;
            }
            
            _discovery_running = true;
            _discovery_future = std::async(std::launch::async, [this]() {
                while (_discovery_running) {
                    try {
                        discover_tools();
                        std::this_thread::sleep_for(std::chrono::seconds(30)); // Discover every 30 seconds
                    } catch (const std::exception& e) {
                        std::cerr << "Error during periodic discovery: " << e.what() << std::endl;
                        std::this_thread::sleep_for(std::chrono::seconds(5)); // Wait 5 seconds before retrying
                    }
                }
            });
        }

        void ToolService::stop_periodic_discovery() {
            _discovery_running = false;
            if (_discovery_future.valid()) {
                _discovery_future.wait();
            }
        }

        nlohmann::json ToolService::execute_tool(const std::string& tool_name, const nlohmann::json& parameters) {
            std::cout << "Executing tool: " << tool_name << std::endl;
            std::cout << "Parameters: " << parameters.dump(4) << std::endl;

            // Try to find the tool in our discovered tools
            for (const auto& server_url : _discovered_servers) {
                try {
                    auto result = execute_tool_on_server(server_url, tool_name, parameters);
                    if (result.value("status", "") == "success" || result.contains("result") || result.contains("content")) {
                        return result;
                    }
                } catch (const std::exception& e) {
                    std::cerr << "Failed to execute tool " << tool_name << " on " << server_url << ": " << e.what() << std::endl;
                }
            }

            return {
                {"status", "error"},
                {"message", "Tool not found or failed to execute."}
            };
        }

        nlohmann::json ToolService::execute_tool_on_server(const std::string& server_url, const std::string& tool_name, const nlohmann::json& parameters) {
            try {
                // Create HTTP client
                http_client client(U(server_url));
                
                // Prepare MCP tools/call request
                json::value request;
                request[U("jsonrpc")] = json::value::string(U("2.0"));
                request[U("method")] = json::value::string(U("tools/call"));
                request[U("id")] = json::value::number(1);
                
                // Add parameters
                json::value params;
                params[U("name")] = json::value::string(utility::conversions::to_string_t(tool_name));
                
                // Convert nlohmann JSON parameters to cpprest JSON
                std::string params_str = parameters.dump();
                params[U("arguments")] = json::value::parse(utility::conversions::to_string_t(params_str));
                
                request[U("params")] = params;
                
                // Send request
                auto response = client.request(methods::POST, U("/mcp"), request).get();
                
                if (response.status_code() == status_codes::OK) {
                    auto response_json = response.extract_json().get();
                    
                    // Convert cpprest JSON to nlohmann JSON
                    std::string response_str = utility::conversions::to_utf8string(response_json.serialize());
                    return nlohmann::json::parse(response_str);
                } else {
                    return {
                        {"status", "error"},
                        {"message", "HTTP error: " + std::to_string(response.status_code())}
                    };
                }
            } catch (const std::exception& e) {
                std::cerr << "Error executing tool " << tool_name << " on " << server_url << ": " << e.what() << std::endl;
                return {
                    {"status", "error"},
                    {"message", std::string("Exception: ") + e.what()}
                };
            }
        }

        std::vector<nlohmann::json> ToolService::get_available_tools() const {
            return _tools;
        }
        
        std::vector<std::string> ToolService::get_discovered_servers() const {
            return _discovered_servers;
        }
        
        size_t ToolService::get_tool_count() const {
            return _tools.size();
        }
        
        std::map<std::string, std::vector<nlohmann::json>> ToolService::get_tools_per_server() const {
            return _tools_per_server;
        }
    }
}