#include "lily/controller/SystemController.hpp"
#include "lily/config/AppConfig.hpp"
#include "lily/utils/SystemMetrics.hpp"
#include "lily/services/Service.hpp"
#include <iostream>

namespace lily {
namespace controller {

    SystemController::SystemController(config::AppConfig& config, services::Service& toolService) 
        : _config(&config), _toolService(&toolService) {}

    nlohmann::json SystemController::getHealth() {
        return {{"status", "UP"}};
    }

    nlohmann::json SystemController::getConfig() {
        if (!_config) return {{"error", "Config not initialized"}};
        
        nlohmann::json response;
        auto api_keys = _config->getGeminiApiKeys();
        
        // Return masked keys (last 4 characters) for display
        nlohmann::json masked_keys = nlohmann::json::array();
        for (const auto& key : api_keys) {
            if (key.length() > 4) {
                masked_keys.push_back("..." + key.substr(key.length() - 4));
            } else {
                masked_keys.push_back("****");
            }
        }
        response["gemini_api_keys"] = masked_keys;
        response["gemini_api_key_count"] = api_keys.size();
        
        response["gemini_model"] = _config->getGeminiModel();
        response["gemini_system_prompt"] = _config->getGeminiSystemPrompt();
        return response;
    }

    nlohmann::json SystemController::updateConfig(const nlohmann::json& config) {
        if (!_config) return {{"error", "Config not initialized"}};
        
        bool updated = false;
        
        // Handle multiple API keys
        if (config.contains("gemini_api_keys") && config["gemini_api_keys"].is_array()) {
            std::vector<std::string> keys;
            for (const auto& key : config["gemini_api_keys"]) {
                if (key.is_string()) {
                    keys.push_back(key.get<std::string>());
                }
            }
            if (!keys.empty()) {
                _config->setGeminiApiKeys(keys);
                updated = true;
            }
        }
        
        if (config.contains("gemini_model")) {
            _config->setGeminiModel(config["gemini_model"]);
            updated = true;
        }
        if (config.contains("gemini_system_prompt")) {
            _config->setGeminiSystemPrompt(config["gemini_system_prompt"]);
            updated = true;
        }
        
        if (updated) {
            _config->saveToFile();
            return {{"message", "Configuration updated"}};
        }
        return {{"message", "No changes"}};
    }

    nlohmann::json SystemController::getMonitoring() {
        lily::utils::SystemMetricsCollector metrics_collector;
        auto monitoring_data = metrics_collector.get_monitoring_data("Lily-Core", "1.0.0");
        
        nlohmann::json response;
        response["status"] = monitoring_data.status;
        response["service_name"] = monitoring_data.service_name;
        response["version"] = monitoring_data.version;
        response["timestamp"] = monitoring_data.timestamp;
        
        nlohmann::json metrics;
        metrics["cpu_usage"] = monitoring_data.metrics.cpu_usage;
        metrics["memory_usage"] = monitoring_data.metrics.memory_usage;
        metrics["disk_usage"] = monitoring_data.metrics.disk_usage;
        metrics["uptime"] = monitoring_data.metrics.uptime;
        response["metrics"] = metrics;
        
        nlohmann::json details;
        for (const auto& detail : monitoring_data.details) {
            details[detail.first] = detail.second;
        }
        response["details"] = details;
        return response;
    }

    nlohmann::json SystemController::getTools() {
        if (!_toolService) return {{"error", "Tool service not initialized"}};

        auto tools_per_server = _toolService->get_tools_per_server();
        
        nlohmann::json response;
        for (const auto& server : tools_per_server) {
            nlohmann::json server_entry;
            server_entry["server_url"] = server.first;
            server_entry["tools"] = server.second;
            response.push_back(server_entry);
        }
        
        return {{"servers", response}};
    }

}
}
