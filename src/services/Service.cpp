#include <lily/services/Service.hpp>
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
        Service::Service() : _discovery_running(false) {
            load_services();
            discover_tools();
        }

        Service::~Service() {
            stop_periodic_discovery();
        }

        void Service::load_services() {
            try {
                // Clear the existing services before loading new ones
                _services.clear();
                
                std::ifstream file("services.json");
                if (file.is_open()) {
                    nlohmann::json j;
                    file >> j;
                    if (j.contains("services") && j["services"].is_array()) {
                        for (const auto& service : j["services"]) {
                            if (service.is_object() &&
                                service.contains("id") && service.contains("name") && service.contains("http_url") && service.contains("mcp")) {
                                ServiceInfo info;
                                info.id = service["id"].get<std::string>();
                                info.name = service["name"].get<std::string>();
                                info.http_url = service["http_url"].get<std::string>();
                                info.websocket_url = service.value("websocket_url", ""); // Default to empty string if not present
                                info.mcp = service["mcp"].get<bool>();
                                _services.push_back(info);
                            }
                        }
                    }
                    file.close();
                } else {
                    std::cerr << "Failed to open services.json" << std::endl;
                }
            } catch (const std::exception& e) {
                std::cerr << "Error loading services: " << e.what() << std::endl;
            }
        }

        void Service::discover_tools() {
            _tools.clear();
            _discovered_servers.clear();
            _tools_per_server.clear();

            for (const auto& service : _services) {
                // Only discover tools from MCP-enabled services
                if (service.mcp) {
                    try {
                        auto tools = discover_tools_from_server(service.http_url);
                        _tools.insert(_tools.end(), tools.begin(), tools.end());
                        _discovered_servers.push_back(service.http_url);
                        _tools_per_server[service.http_url] = tools;
                    } catch (const std::exception& e) {
                        std::cerr << "Failed to discover tools from " << service.http_url << " (" << service.name << "): " << e.what() << std::endl;
                    }
                }
            }
        }

        std::vector<nlohmann::json> Service::discover_tools_from_server(const std::string& server_url) {
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

        void Service::start_periodic_discovery() {
            if (_discovery_running) {
                return;
            }

            _discovery_running = true;
            _discovery_future = std::async(std::launch::async, [this]() {
                while (_discovery_running) {
                    try {
                        load_services(); // Reload services from services.json
                        discover_tools();
                        std::this_thread::sleep_for(std::chrono::seconds(30)); // Discover every 30 seconds
                    } catch (const std::exception& e) {
                        std::cerr << "Error during periodic discovery: " << e.what() << std::endl;
                        std::this_thread::sleep_for(std::chrono::seconds(5)); // Wait 5 seconds before retrying
                    }
                }
            });
        }

        void Service::stop_periodic_discovery() {
            _discovery_running = false;
            if (_discovery_future.valid()) {
                _discovery_future.wait();
            }
        }

        nlohmann::json Service::execute_tool(const std::string& tool_name, const nlohmann::json& parameters) {
            std::vector<std::string> error_details;
            
            // Try to find the tool in our discovered tools
            for (const auto& server_url : _discovered_servers) {
                try {
                    auto result = execute_tool_on_server(server_url, tool_name, parameters);
                    if (result.value("status", "") == "success" || result.contains("result") || result.contains("content")) {
                        return result;
                    } else {
                        // Capture error details from the result
                        std::string error_msg = "Server: " + server_url + " - ";
                        if (result.contains("message")) {
                            error_msg += "Message: " + result["message"].get<std::string>();
                        } else {
                            error_msg += "Unknown error";
                        }
                        error_details.push_back(error_msg);
                    }
                } catch (const std::exception& e) {
                    std::string error_msg = "Server: " + server_url + " - Exception: " + std::string(e.what());
                    std::cerr << "Failed to execute tool " << tool_name << " on " << server_url << ": " << e.what() << std::endl;
                    error_details.push_back(error_msg);
                }
            }

            // Build detailed error message
            std::string detailed_message = "Tool not found or failed to execute. Details: ";
            if (!error_details.empty()) {
                for (size_t i = 0; i < error_details.size(); i++) {
                    detailed_message += "\n" + std::to_string(i + 1) + ". " + error_details[i];
                }
            } else {
                detailed_message += "No servers available or discovered.";
            }

            return {
                {"status", "error"},
                {"message", detailed_message},
                {"error_details", error_details}
            };
        }

        nlohmann::json Service::execute_tool_on_server(const std::string& server_url, const std::string& tool_name, const nlohmann::json& parameters) {
            try {
                // Create HTTP client with timeout configuration
                http_client_config config;
                config.set_timeout(std::chrono::seconds(30));
                http_client client(U(server_url), config);

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

                // Send request with detailed logging
                std::cout << "[HTTP CLIENT] Sending request to " << server_url << "/mcp" << std::endl;
                auto response = client.request(methods::POST, U("/mcp"), request).get();
                std::cout << "[HTTP CLIENT] Received response with status: " << response.status_code() << std::endl;

                if (response.status_code() == status_codes::OK) {
                    try {
                        auto response_json = response.extract_json().get();
                        std::cout << "[HTTP CLIENT] Successfully extracted JSON response" << std::endl;

                        // Convert cpprest JSON to nlohmann JSON
                        std::string response_str = utility::conversions::to_utf8string(response_json.serialize());
                        return nlohmann::json::parse(response_str);
                    } catch (const std::exception& e) {
                        std::cerr << "[HTTP CLIENT] Error extracting JSON from response: " << e.what() << std::endl;
                        // Try to get the raw response body for debugging
                        std::string raw_response;
                        try {
                            raw_response = utility::conversions::to_utf8string(response.extract_string().get());
                            std::cerr << "[HTTP CLIENT] Raw response body: " << raw_response << std::endl;
                        } catch (const std::exception& ex) {
                            std::cerr << "[HTTP CLIENT] Failed to extract raw response: " << ex.what() << std::endl;
                            raw_response = "Unable to extract response body";
                        }
                        
                        return {
                            {"status", "error"},
                            {"message", std::string("JSON extraction error: ") + e.what()},
                            {"error_type", "json_extraction_error"},
                            {"raw_response", raw_response},
                            {"server_url", server_url},
                            {"tool_name", tool_name}
                        };
                    }
                } else {
                    std::string error_body;
                    try {
                        error_body = utility::conversions::to_utf8string(response.extract_string().get());
                        std::cerr << "[HTTP CLIENT] HTTP error body: " << error_body << std::endl;
                    } catch (...) {
                        error_body = "Unable to extract error body";
                    }
                    
                    return {
                        {"status", "error"},
                        {"message", "HTTP error: " + std::to_string(response.status_code())},
                        {"http_status", response.status_code()},
                        {"error_body", error_body},
                        {"server_url", server_url},
                        {"tool_name", tool_name}
                    };
                }
            } catch (const web::http::http_exception& e) {
                std::cerr << "[HTTP CLIENT] HTTP exception executing tool " << tool_name << " on " << server_url << ": " << e.what() << std::endl;
                return {
                    {"status", "error"},
                    {"message", std::string("HTTP Exception: ") + e.what()},
                    {"error_type", "http_exception"},
                    {"server_url", server_url},
                    {"tool_name", tool_name}
                };
            } catch (const web::uri_exception& e) {
                std::cerr << "[HTTP CLIENT] URI exception executing tool " << tool_name << " on " << server_url << ": " << e.what() << std::endl;
                return {
                    {"status", "error"},
                    {"message", std::string("URI Exception: ") + e.what()},
                    {"error_type", "uri_exception"},
                    {"server_url", server_url},
                    {"tool_name", tool_name}
                };
            } catch (const std::exception& e) {
                std::cerr << "[HTTP CLIENT] General exception executing tool " << tool_name << " on " << server_url << ": " << e.what() << std::endl;
                return {
                    {"status", "error"},
                    {"message", std::string("Exception: ") + e.what()},
                    {"error_type", "general_exception"},
                    {"server_url", server_url},
                    {"tool_name", tool_name}
                };
            }
        }

        std::vector<nlohmann::json> Service::get_available_tools() const {
            return _tools;
        }

        std::vector<std::string> Service::get_discovered_servers() const {
            return _discovered_servers;
        }

        size_t Service::get_tool_count() const {
            return _tools.size();
        }

        std::map<std::string, std::vector<nlohmann::json>> Service::get_tools_per_server() const {
            return _tools_per_server;
        }
    }
}