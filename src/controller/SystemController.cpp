#include "lily/controller/SystemController.hpp"
#include "lily/config/AppConfig.hpp"
#include "lily/utils/SystemMetrics.hpp"
#include <iostream>

namespace lily {
namespace controller {

    SystemController::SystemController(config::AppConfig& config) : _config(&config) {}

    nlohmann::json SystemController::getHealth() {
        return {{"status", "UP"}};
    }

    nlohmann::json SystemController::getConfig() {
        if (!_config) return {{"error", "Config not initialized"}};
        
        nlohmann::json response;
        std::string api_key = _config->getGeminiApiKey();
        if (api_key.length() > 8) {
            response["gemini_api_key"] = "..." + api_key.substr(api_key.length() - 4);
        } else if (!api_key.empty()) {
            response["gemini_api_key"] = "********";
        } else {
            response["gemini_api_key"] = "";
        }
        
        response["gemini_model"] = _config->getGeminiModel();
        response["gemini_system_prompt"] = _config->getGeminiSystemPrompt();
        return response;
    }

    nlohmann::json SystemController::updateConfig(const nlohmann::json& config) {
        if (!_config) return {{"error", "Config not initialized"}};
        
        bool updated = false;
        if (config.contains("gemini_api_key")) {
            _config->setGeminiApiKey(config["gemini_api_key"]);
            updated = true;
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

}
}
