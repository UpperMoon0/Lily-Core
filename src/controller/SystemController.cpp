#include "lily/controller/SystemController.hpp"
#include "lily/config/AppConfig.hpp"
#include "lily/utils/SystemMetrics.hpp"
#include "lily/services/Service.hpp"
#include "lily/services/AgentLoopService.hpp"
#include <iostream>

namespace lily {
namespace controller {

    SystemController::SystemController(config::AppConfig& config, services::Service& toolService) 
        : _config(&config), _toolService(&toolService), _agentLoopService(nullptr) {}

    void SystemController::setAgentLoopService(services::AgentLoopService* agentLoopService) {
        _agentLoopService = agentLoopService;
    }

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
    
    // Agent loop endpoints
    
    nlohmann::json SystemController::getUserIdsWithAgentLoops() {
        if (!_agentLoopService) return {{"error", "AgentLoopService not initialized"}};
        
        auto user_ids = _agentLoopService->get_user_ids();
        nlohmann::json response;
        response["user_ids"] = user_ids;
        response["count"] = user_ids.size();
        return response;
    }
    
    nlohmann::json SystemController::getAgentLoopsForUser(const std::string& user_id) {
        if (!_agentLoopService) return {{"error", "AgentLoopService not initialized"}};
        
        auto loops = _agentLoopService->get_agent_loops_for_user(user_id);
        nlohmann::json response;
        response["user_id"] = user_id;
        response["loops"] = nlohmann::json::array();
        
        for (const auto& loop : loops) {
            nlohmann::json loop_json;
            loop_json["user_id"] = loop.user_id;
            loop_json["user_message"] = loop.user_message;
            loop_json["final_response"] = loop.final_response;
            loop_json["completed"] = loop.completed;
            loop_json["duration_seconds"] = loop.duration_seconds;
            
            // Convert times to ISO string
            auto start_time_t = std::chrono::system_clock::to_time_t(loop.start_time);
            auto end_time_t = std::chrono::system_clock::to_time_t(loop.end_time);
            loop_json["start_time"] = std::ctime(&start_time_t);
            loop_json["end_time"] = std::ctime(&end_time_t);
            
            // Convert steps
            loop_json["steps"] = nlohmann::json::array();
            for (const auto& step : loop.steps) {
                nlohmann::json step_json;
                step_json["step_number"] = step.step_number;
                step_json["type"] = static_cast<int>(step.type);
                step_json["reasoning"] = step.reasoning;
                step_json["tool_name"] = step.tool_name;
                step_json["tool_parameters"] = step.tool_parameters;
                step_json["tool_result"] = step.tool_result;
                step_json["duration_seconds"] = step.duration_seconds;
                
                auto step_time_t = std::chrono::system_clock::to_time_t(step.timestamp);
                step_json["timestamp"] = std::ctime(&step_time_t);
                
                loop_json["steps"].push_back(step_json);
            }
            
            response["loops"].push_back(loop_json);
        }
        
        return response;
    }
    
    nlohmann::json SystemController::clearAgentLoopsForUser(const std::string& user_id) {
        if (!_agentLoopService) return {{"error", "AgentLoopService not initialized"}};
        
        _agentLoopService->clear_agent_loops_for_user(user_id);
        return {{"message", "Agent loops cleared for user " + user_id}};
    }

}
}
