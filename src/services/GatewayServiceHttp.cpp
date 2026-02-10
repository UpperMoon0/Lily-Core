#include "lily/services/GatewayService.hpp"
#include <iostream>

namespace lily {
    namespace services {

        void GatewayService::on_http(ConnectionHandle hdl) {
            Server::connection_ptr con = _server.get_con_from_hdl(hdl);
            std::string path = con->get_resource();
            std::string method = con->get_request().get_method();
            
            con->append_header("Access-Control-Allow-Origin", "*");
            con->append_header("Access-Control-Allow-Methods", "GET, POST, DELETE, OPTIONS");
            con->append_header("Access-Control-Allow-Headers", "Content-Type");
            con->append_header("Content-Type", "application/json");
            
            if (method == "OPTIONS") {
                con->set_status(websocketpp::http::status_code::ok);
                return;
            }

            // --- System Controller Routes ---
            if (_system_controller) {
                if (method == "GET" && (path == "/api/health" || path == "/health")) {
                    con->set_body(_system_controller->getHealth().dump());
                    con->set_status(websocketpp::http::status_code::ok);
                    return;
                }
                if (method == "GET" && (path == "/api/config" || path == "/config")) {
                    con->set_body(_system_controller->getConfig().dump());
                    con->set_status(websocketpp::http::status_code::ok);
                    return;
                }
                if (method == "POST" && (path == "/api/config" || path == "/config")) {
                    try {
                        auto json_value = nlohmann::json::parse(con->get_request_body());
                        con->set_body(_system_controller->updateConfig(json_value).dump());
                        con->set_status(websocketpp::http::status_code::ok);
                    } catch (const std::exception& e) {
                        con->set_body(nlohmann::json({{"error", e.what()}}).dump());
                        con->set_status(websocketpp::http::status_code::internal_server_error);
                    }
                    return;
                }
                if (method == "GET" && (path == "/api/monitoring" || path == "/monitoring")) {
                    con->set_body(_system_controller->getMonitoring().dump());
                    con->set_status(websocketpp::http::status_code::ok);
                    return;
                }
                if (method == "GET" && (path == "/api/tools" || path == "/tools")) {
                    con->set_body(_system_controller->getTools().dump());
                    con->set_status(websocketpp::http::status_code::ok);
                    return;
                }
            }

            // --- Session Controller Routes ---
            if (_session_controller) {
                if (method == "GET" && (path == "/api/connected-users" || path == "/connected-users")) {
                    con->set_body(_session_controller->getConnectedUsers().dump());
                    con->set_status(websocketpp::http::status_code::ok);
                    return;
                }
                if (method == "GET" && (path == "/api/active-sessions" || path == "/active-sessions")) {
                    con->set_body(_session_controller->getActiveSessions().dump());
                    con->set_status(websocketpp::http::status_code::ok);
                    return;
                }
            }

            // --- Chat Controller Routes ---
            if (_chat_controller) {
                if (method == "GET" && (path == "/api/agent-loops" || path == "/agent-loops")) {
                    con->set_body(_chat_controller->getAgentLoops().dump());
                    con->set_status(websocketpp::http::status_code::ok);
                    return;
                }
                
                if (method == "POST" && (path == "/api/chat" || path == "/chat")) {
                    try {
                        auto json_value = nlohmann::json::parse(con->get_request_body());
                        
                        con->defer_http_response();
                        _chat_controller->chat(json_value, [this, con](const nlohmann::json& response, int status) {
                            // Dispatch back to io_service
                            this->_server.get_io_service().dispatch([con, response, status]() {
                                con->set_body(response.dump());
                                con->set_status(static_cast<websocketpp::http::status_code::value>(status));
                                con->send_http_response();
                            });
                        });
                        return; // Deferred
                    } catch (const std::exception& e) {
                        con->set_body(nlohmann::json({{"error", e.what()}}).dump());
                        con->set_status(websocketpp::http::status_code::bad_request);
                        return;
                    }
                }

                if ((path.rfind("/api/conversation/", 0) == 0 || path.rfind("/conversation/", 0) == 0)) {
                    std::string prefix = (path.rfind("/api/", 0) == 0) ? "/api/conversation/" : "/conversation/";
                    std::string user_id = path.substr(prefix.length());
                    
                    if (method == "GET") {
                        con->set_body(_chat_controller->getConversation(user_id).dump());
                        con->set_status(websocketpp::http::status_code::ok);
                        return;
                    } else if (method == "DELETE") {
                        _chat_controller->clearConversation(user_id);
                        con->set_status(websocketpp::http::status_code::ok);
                        return;
                    }
                }
            }

            // Default 404
            con->set_body(nlohmann::json({{"error", "Not Found"}}).dump());
            con->set_status(websocketpp::http::status_code::not_found);
        }
    }
}
