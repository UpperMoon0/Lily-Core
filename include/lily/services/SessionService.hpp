#ifndef LILY_SERVICES_SESSION_SERVICE_HPP
#define LILY_SERVICES_SESSION_SERVICE_HPP

#include <string>
#include <map>
#include <mutex>
#include <chrono>
#include <thread>
#include <atomic>
#include <functional>
#include <iostream>

#include "lily/services/GatewayService.hpp"

namespace lily {
namespace services {

struct SessionInfo {
    std::string user_id;
    std::chrono::system_clock::time_point start_time;
    std::chrono::system_clock::time_point last_activity;
    bool active;
};

class SessionService {
public:
    SessionService(GatewayService& ws_manager);
    ~SessionService();

    // Session lifecycle
    void start_session(const std::string& user_id);
    void end_session(const std::string& user_id);
    void touch_session(const std::string& user_id);
    bool is_session_active(const std::string& user_id);

    // Get all sessions
    std::vector<SessionInfo> get_all_sessions();
    
    // Configuration
    void set_timeout_minutes(int minutes);

private:
    void cleanup_loop();
    void broadcast_session_event(const std::string& type, const std::string& user_id);

    GatewayService& _ws_manager;
    std::map<std::string, SessionInfo> _sessions;
    std::mutex _mutex;
    
    std::atomic<bool> _running;
    std::thread _cleanup_thread;
    int _timeout_minutes = 30;
};

} // namespace services
} // namespace lily

#endif // LILY_SERVICES_SESSION_SERVICE_HPP
