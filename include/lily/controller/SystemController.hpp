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
    }
}

namespace lily {
namespace controller {

    class SystemController {
    public:
        SystemController(config::AppConfig& config, services::Service& toolService);

        nlohmann::json getHealth();
        nlohmann::json getConfig();
        nlohmann::json updateConfig(const nlohmann::json& config);
        nlohmann::json getMonitoring();
        nlohmann::json getTools();

    private:
        config::AppConfig* _config;
        services::Service* _toolService;
    };

}
}

#endif // LILY_CONTROLLER_SYSTEM_CONTROLLER_HPP
