#ifndef LILY_CONTROLLER_SESSION_CONTROLLER_HPP
#define LILY_CONTROLLER_SESSION_CONTROLLER_HPP

#include <memory>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

namespace lily {
    namespace services {
        class SessionService;
        class GatewayService;
    }
}

namespace lily {
namespace controller {

    class SessionController {
    public:
        SessionController(
            std::shared_ptr<services::SessionService> sessionService,
            std::shared_ptr<services::GatewayService> gatewayService
        );

        nlohmann::json getActiveSessions();
        nlohmann::json getConnectedUsers();

    private:
        std::shared_ptr<services::SessionService> _sessionService;
        std::shared_ptr<services::GatewayService> _gatewayService;
    };

}
}

#endif // LILY_CONTROLLER_SESSION_CONTROLLER_HPP
