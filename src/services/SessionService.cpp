#include "lily/services/SessionService.hpp"
#include <nlohmann/json.hpp>

namespace lily {
namespace services {

SessionService::SessionService(WebSocketManager& ws_manager) 
    : _ws_manager(ws_manager), _running(true) {
    _cleanup_thread = std::thread(&SessionService::cleanup_loop, this);
}

SessionService::~SessionService() {
    _running = false;
    if (_cleanup_thread.joinable()) {
        _cleanup_thread.join();
    }
}

void SessionService::start_session(const std::string& user_id) {
    std::lock_guard<std::mutex> lock(_mutex);
    
    SessionInfo session;
    session.user_id = user_id;
    session.start_time = std::chrono::system_clock::now();
    session.last_activity = std::chrono::system_clock::now();
    session.active = true;
    
    _sessions[user_id] = session;
    std::cout << "[SessionService] Started session for user: " << user_id << std::endl;
}

void SessionService::end_session(const std::string& user_id) {
    {
        std::lock_guard<std::mutex> lock(_mutex);
        auto it = _sessions.find(user_id);
        if (it != _sessions.end()) {
            it->second.active = false;
        }
    }
    std::cout << "[SessionService] Ended session for user: " << user_id << std::endl;
}

void SessionService::touch_session(const std::string& user_id) {
    std::lock_guard<std::mutex> lock(_mutex);
    auto it = _sessions.find(user_id);
    if (it != _sessions.end() && it->second.active) {
        it->second.last_activity = std::chrono::system_clock::now();
    }
}

bool SessionService::is_session_active(const std::string& user_id) {
    std::lock_guard<std::mutex> lock(_mutex);
    auto it = _sessions.find(user_id);
    return it != _sessions.end() && it->second.active;
}

std::vector<SessionInfo> SessionService::get_all_sessions() {
    std::lock_guard<std::mutex> lock(_mutex);
    std::vector<SessionInfo> sessions;
    for (const auto& pair : _sessions) {
        sessions.push_back(pair.second);
    }
    return sessions;
}

void SessionService::set_timeout_minutes(int minutes) {
    _timeout_minutes = minutes;
}

void SessionService::cleanup_loop() {
    while (_running) {
        std::this_thread::sleep_for(std::chrono::seconds(60)); // Check every minute
        
        std::vector<std::string> expired_users;
        {
            std::lock_guard<std::mutex> lock(_mutex);
            auto now = std::chrono::system_clock::now();
            
            for (auto& pair : _sessions) {
                if (pair.second.active) {
                    auto elapsed = std::chrono::duration_cast<std::chrono::minutes>(now - pair.second.last_activity).count();
                    if (elapsed >= _timeout_minutes) {
                        pair.second.active = false;
                        expired_users.push_back(pair.first);
                    }
                }
            }
        }
        
        for (const auto& user_id : expired_users) {
            std::cout << "[SessionService] Session expired for user: " << user_id << std::endl;
            broadcast_session_event("session_expired", user_id);
        }
    }
}

void SessionService::broadcast_session_event(const std::string& type, const std::string& user_id) {
    nlohmann::json event = {
        {"type", type},
        {"user_id", user_id},
        {"timestamp", std::chrono::system_clock::to_time_t(std::chrono::system_clock::now())}
    };
    
    // Broadcast to all connected clients (mainly for Discord Adapter to pick up)
    // In a more complex setup, we might target specific connections, but broadcasting session events is fine for now
    _ws_manager.broadcast(event.dump());
}

} // namespace services
} // namespace lily
