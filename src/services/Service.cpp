#include <lily/services/Service.hpp>
#include <iostream>
#include <fstream>
#include <nlohmann/json.hpp>
#include <cpprest/http_client.h>
#include <cpprest/filestream.h>
#include <thread>
#include <chrono>
#include <cstdlib>
#include <unistd.h>

using namespace web;
using namespace web::http;
using namespace web::http::client;

namespace lily {
    namespace services {
        Service::Service() : _discovery_running(false) {
            discover_services_from_consul();
            discover_tools();
        }

        Service::~Service() {
            stop_periodic_discovery();
            // Deregister all registered services
            for (const auto& service_id : _registered_service_ids) {
                deregister_service(service_id);
            }
        }

        // ==================== SERVICE REGISTRATION ====================

        bool Service::register_service(const std::string& service_name, int port, const std::vector<std::string>& tags) {
            try {
                std::string consul_host = "http://consul:8500";
                if (const char* env_p = std::getenv("CONSUL_HTTP_ADDR")) {
                    std::string env_s(env_p);
                    if (env_s.find("://") == std::string::npos) {
                        consul_host = "http://" + env_s;
                    } else {
                        consul_host = env_s;
                    }
                }

                // Get hostname
                char hostname[256];
                gethostname(hostname, sizeof(hostname));
                std::string hostname_str(hostname);
                
                std::string service_id = service_name + "-" + hostname_str + "-" + std::to_string(port);

                // Build health check URL
                std::string health_check_url = "http://" + hostname_str + ":" + std::to_string(port) + "/health";

                // Build registration payload
                nlohmann::json check_config;
                bool is_websocket = false;
                for (const auto& tag : tags) {
                    if (tag == "websocket") {
                        is_websocket = true;
                        break;
                    }
                }

                if (is_websocket) {
                    check_config = {
                        {"TCP", hostname_str + ":" + std::to_string(port)},
                        {"Interval", "10s"},
                        {"Timeout", "2s"},
                        {"DeregisterCriticalServiceAfter", "1m"}
                    };
                } else {
                    check_config = {
                        {"HTTP", health_check_url},
                        {"Interval", "10s"},
                        {"Timeout", "2s"},
                        {"DeregisterCriticalServiceAfter", "1m"}
                    };
                }

                nlohmann::json payload = {
                    {"ID", service_id},
                    {"Name", service_name},
                    {"Tags", tags},
                    {"Address", hostname_str},
                    {"Port", port},
                    {"Check", check_config}
                };

                std::string url = consul_host + "/v1/agent/service/register";
                http_client client(utility::conversions::to_string_t(url));

                json::value json_payload = json::value::parse(utility::conversions::to_string_t(payload.dump()));
                auto response = client.request(methods::PUT, U(""), json_payload).get();

                if (response.status_code() == status_codes::OK) {
                    _registered_service_ids.push_back(service_id);
                    std::cout << "[ServiceDiscovery] Registered " << service_name << " at " << hostname_str << ":" << port << std::endl;
                    return true;
                } else {
                    std::cerr << "[ServiceDiscovery] Failed to register " << service_name << ": HTTP " << response.status_code() << std::endl;
                    return false;
                }
            } catch (const std::exception& e) {
                std::cerr << "[ServiceDiscovery] Error registering service " << service_name << ": " << e.what() << std::endl;
                return false;
            }
        }

        void Service::register_all_services(int http_port, int /*ws_port*/) {
            std::vector<std::string> tags = {"orchestrator"};
            
            // Add public URL tag if DOMAIN_NAME is set
            if (const char* domain = std::getenv("DOMAIN_NAME")) {
                std::string hostname = "lily-core." + std::string(domain);
                tags.push_back("hostname=" + hostname);
            }

            // Register combined service on http_port (which is now the single entry point)
            // Note: Nginx handles routing /api to HTTP and /ws to WebSocket
            bool registered = register_service("lily-core", http_port, tags);
            
            if (registered) {
                std::cout << "[ServiceDiscovery] Lily-Core fully registered with Consul on port " << http_port << std::endl;
            } else {
                std::cout << "[ServiceDiscovery] Lily-Core registration failed" << std::endl;
            }
        }

        void Service::deregister_service(const std::string& service_id) {
            try {
                std::string consul_host = "http://consul:8500";
                if (const char* env_p = std::getenv("CONSUL_HTTP_ADDR")) {
                    std::string env_s(env_p);
                    if (env_s.find("://") == std::string::npos) {
                        consul_host = "http://" + env_s;
                    } else {
                        consul_host = env_s;
                    }
                }

                std::string url = consul_host + "/v1/agent/service/deregister/" + service_id;
                http_client client(utility::conversions::to_string_t(url));
                auto response = client.request(methods::PUT, U("")).get();

                if (response.status_code() == status_codes::OK) {
                    std::cout << "[ServiceDiscovery] Deregistered service: " << service_id << std::endl;
                }
            } catch (const std::exception& e) {
                std::cerr << "[ServiceDiscovery] Error deregistering service " << service_id << ": " << e.what() << std::endl;
            }
        }

        void Service::discover_services_from_consul() {
            try {
                _services.clear();
                
                std::string consul_host = "http://consul:8500";
                if (const char* env_p = std::getenv("CONSUL_HTTP_ADDR")) {
                     std::string env_s(env_p);
                     if (env_s.find("://") == std::string::npos) {
                         consul_host = "http://" + env_s;
                     } else {
                         consul_host = env_s;
                     }
                }
                
                http_client client(utility::conversions::to_string_t(consul_host));
                
                // 1. Get List of Services
                auto response = client.request(methods::GET, U("/v1/catalog/services")).get();
                if (response.status_code() == status_codes::OK) {
                     auto services_json = response.extract_json().get();
                     auto services_obj = services_json.as_object();
                     
                     for (auto it = services_obj.begin(); it != services_obj.end(); ++it) {
                         std::string service_name = utility::conversions::to_utf8string(it->first);
                         if (service_name == "consul") continue;
                         
                         // 2. Get Healthy Nodes
                         uri_builder builder(U("/v1/health/service/" + utility::conversions::to_string_t(service_name)));
                         builder.append_query(U("passing"), U("true"));
                         
                         auto health_resp = client.request(methods::GET, builder.to_string()).get();
                         if (health_resp.status_code() == status_codes::OK) {
                              auto nodes_json = health_resp.extract_json().get();
                              if (nodes_json.is_array()) {
                                  auto nodes = nodes_json.as_array();
                                  if (nodes.size() > 0) {
                                      auto node = nodes[0]; // Pick first healthy node
                                      auto service_obj = node[U("Service")];
                                      
                                      ServiceInfo info;
                                      info.id = service_name;
                                      info.name = service_name;
                                      
                                      std::string hostname_tag;
                                      info.mcp = false;
                                      if (service_obj.has_field(U("Tags"))) {
                                          auto tags = service_obj[U("Tags")].as_array();
                                          for (const auto& tag : tags) {
                                               std::string tag_str = utility::conversions::to_utf8string(tag.as_string());
                                               if (tag_str == "mcp") {
                                                   info.mcp = true;
                                               }
                                               if (tag_str.rfind("hostname=", 0) == 0) {
                                                   hostname_tag = tag_str.substr(9);
                                               }
                                          }
                                      }
                                      
                                      if (!hostname_tag.empty()) {
                                          info.http_url = "https://" + hostname_tag + "/api";
                                          info.websocket_url = "wss://" + hostname_tag + "/ws";
                                          info.mcp_url = "https://" + hostname_tag + "/mcp";
                                          
                                          _services.push_back(info);
                                          std::cout << "[ServiceDiscovery] Discovered: " << service_name << " at " << info.http_url << std::endl;
                                      }
                                  }
                              }
                         }
                     }
                }
            } catch (const std::exception& e) {
                std::cerr << "Error discovering services from Consul: " << e.what() << std::endl;
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
                        auto tools = discover_tools_from_server(service.mcp_url);
                        _tools.insert(_tools.end(), tools.begin(), tools.end());
                        _discovered_servers.push_back(service.mcp_url);
                        _tools_per_server[service.mcp_url] = tools;
                    } catch (const std::exception& e) {
                        std::cerr << "Failed to discover tools from " << service.mcp_url << " (" << service.name << "): " << e.what() << std::endl;
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
                auto response = client.request(methods::POST, U(""), request).get();

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
                        discover_services_from_consul(); 
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
                std::cout << "[HTTP CLIENT] Sending request to " << server_url << std::endl;
                auto response = client.request(methods::POST, U(""), request).get();
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

        std::string Service::getServiceUrl(const std::string& service_name, const std::string& protocol) const {
            for (const auto& service : _services) {
                if (service.name == service_name || service.id == service_name) {
                    if (protocol == "ws" || protocol == "websocket") {
                        if (!service.websocket_url.empty()) {
                            return service.websocket_url;
                        }
                        // Fallback derivation
                        if (service.http_url.find("https://") == 0) {
                            return "wss://" + service.http_url.substr(8);
                        } else if (service.http_url.find("http://") == 0) {
                            return "ws://" + service.http_url.substr(7);
                        }
                    }
                    return service.http_url;
                }
            }
            return "";
        }
    }
}