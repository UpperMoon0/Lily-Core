#ifndef LILY_CONTROLLER_SYSTEM_CONTROLLER_HPP
#define LILY_CONTROLLER_SYSTEM_CONTROLLER_HPP

#include <memory>
#include <string>
#include <nlohmann/json.hpp>

namespace lily {
    namespace config {
        class AppConfig;
    }
    namespace services {
        class Service;
        class AgentLoopService;
    }
}

namespace lily {
namespace controller {

    class SystemController {
    public:
        SystemController(config::AppConfig& config, services::Service& toolService);
        void setAgentLoopService(services::AgentLoopService* agentLoopService);

        nlohmann::json getHealth();
        nlohmann::json getConfig();
        nlohmann::json updateConfig(const nlohmann::json& config);
        nlohmann::json getMonitoring();
        nlohmann::json getTools();
        
        // Agent loop endpoints
        nlohmann::json getUserIdsWithAgentLoops();
        nlohmann::json getAgentLoopsForUser(const std::string& user_id);
        nlohmann::json clearAgentLoopsForUser(const std::string& user_id);

    private:
        config::AppConfig* _config;
        services::Service* _toolService;
        services::AgentLoopService* _agentLoopService;
    };

}
}

#endif // LILY_CONTROLLER_SYSTEM_CONTROLLER_HPP
