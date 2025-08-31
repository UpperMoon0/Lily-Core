#ifndef LILY_SERVICES_TOOLSERVICE_HPP
#define LILY_SERVICES_TOOLSERVICE_HPP

#include <string>
#include <vector>
#include <nlohmann/json.hpp>

namespace lily {
    namespace services {
        class ToolService {
        public:
            ToolService();
            ~ToolService();

            void discover_tools();
            nlohmann::json execute_tool(const std::string& tool_name, const nlohmann::json& parameters);
            std::vector<nlohmann::json> get_available_tools() const;

        private:
            std::vector<nlohmann::json> _tools;
        };
    }
}

#endif // LILY_SERVICES_TOOLSERVICE_HPP