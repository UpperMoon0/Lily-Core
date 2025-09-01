#ifndef LILY_SERVICES_SERVICE_HPP
#define LILY_SERVICES_SERVICE_HPP

#include <string>
#include <vector>
#include <nlohmann/json.hpp>
#include <cpprest/http_client.h>
#include <future>
#include <chrono>
#include <map>
#include <atomic>

namespace lily {
    namespace services {

        struct ServiceInfo {
            std::string id;
            std::string name;
            std::string url;
            bool mcp;
        };

        class Service {
        public:
            Service();
            ~Service();

            void discover_tools();
            void start_periodic_discovery();
            void stop_periodic_discovery();
            nlohmann::json execute_tool(const std::string& tool_name, const nlohmann::json& parameters);
            std::vector<nlohmann::json> get_available_tools() const;
            std::vector<std::string> get_discovered_servers() const;
            size_t get_tool_count() const;
            const std::vector<ServiceInfo>& get_services_info() const { return _services; }

            // Getter for tools per server
            std::map<std::string, std::vector<nlohmann::json>> get_tools_per_server() const;

        private:
            std::vector<nlohmann::json> _tools;
            std::vector<ServiceInfo> _services;
            std::vector<std::string> _discovered_servers;
            std::future<void> _discovery_future;
            std::atomic<bool> _discovery_running;

            void load_services();
            std::vector<nlohmann::json> discover_tools_from_server(const std::string& server_url);
            nlohmann::json execute_tool_on_server(const std::string& server_url, const std::string& tool_name, const nlohmann::json& parameters);

            // Store tools per server for detailed monitoring
            std::map<std::string, std::vector<nlohmann::json>> _tools_per_server;
        };
    }
}

#endif // LILY_SERVICES_SERVICE_HPP