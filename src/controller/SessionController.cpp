#include "lily/controller/SessionController.hpp"
#include "lily/services/SessionService.hpp"
#include "lily/services/GatewayService.hpp"
#include <iostream>
#include <ctime>
#include <chrono>

namespace lily {
namespace controller {

    SessionController::SessionController(
        std::shared_ptr<services::SessionService> sessionService,
        std::shared_ptr<services::GatewayService> gatewayService
    ) : _sessionService(sessionService), _gatewayService(gatewayService) {}

    nlohmann::json SessionController::getActiveSessions() {
        if (!_sessionService) return {{"error", "SessionService not available"}};
        
        auto sessions = _sessionService->get_all_sessions();
        nlohmann::json response;
        nlohmann::json sessions_json = nlohmann::json::array();
        
        char buf[100];
        for (const auto& session : sessions) {
            nlohmann::json session_json;
            session_json["user_id"] = session.user_id;
            session_json["active"] = session.active;
            
            auto start_time = std::chrono::system_clock::to_time_t(session.start_time);
            if (std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", std::gmtime(&start_time))) {
                session_json["start_time"] = std::string(buf);
            }
            
            auto last_activity = std::chrono::system_clock::to_time_t(session.last_activity);
            if (std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", std::gmtime(&last_activity))) {
                session_json["last_activity"] = std::string(buf);
            }
            
            auto now = std::chrono::system_clock::now();
            auto duration_mins = std::chrono::duration_cast<std::chrono::minutes>(now - session.start_time).count();
            session_json["duration_minutes"] = duration_mins;
            
            sessions_json.push_back(session_json);
        }
        
        response["sessions"] = sessions_json;
        response["count"] = sessions.size();
        return response;
    }

    nlohmann::json SessionController::getConnectedUsers() {
        if (!_gatewayService) return {{"error", "GatewayService not available"}};
        
        auto user_ids = _gatewayService->get_connected_user_ids();
        nlohmann::json response;
        response["user_ids"] = user_ids;
        response["count"] = user_ids.size();
        
        auto now = std::chrono::system_clock::now();
        std::time_t now_time = std::chrono::system_clock::to_time_t(now);
        char buf[100];
        if (std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", std::gmtime(&now_time))) {
            response["timestamp"] = std::string(buf);
        }
        return response;
    }

}
}
