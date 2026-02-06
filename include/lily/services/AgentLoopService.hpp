#ifndef LILY_AGENTLOOPSERVICE_HPP
#define LILY_AGENTLOOPSERVICE_HPP

#include <lily/services/MemoryService.hpp>
#include <lily/services/Service.hpp>
#include <lily/models/AgentLoop.hpp>
#include <lily/config/AppConfig.hpp>
#include <string>
#include <vector>
#include <memory>

namespace lily {
    namespace services {
        class AgentLoopService {
        public:
            AgentLoopService(MemoryService& memoryService, Service& toolService, config::AppConfig& config);
            std::string run_loop(const std::string& user_message, const std::string& user_id);
            
            // New methods for agent loop tracking
            const lily::models::AgentLoop& get_last_agent_loop() const;
            void clear_agent_loops();
            std::vector<lily::models::AgentLoop> get_agent_loops() const;

        private:
            MemoryService& _memoryService;
            Service& _toolService;
            config::AppConfig& _config;
            
            // Agent loop tracking
            std::vector<lily::models::AgentLoop> _agentLoops;
            mutable std::mutex _agentLoopsMutex;
            
            // Helper methods for the step-based loop
            std::string process_with_tools(const std::string& user_message, const std::string& user_id, lily::models::AgentLoop& current_loop);
            std::string execute_agent_step(const std::vector<nlohmann::json>& available_tools, 
                                         const std::string& context, 
                                         lily::models::AgentLoop& current_loop,
                                         int step_number);
            nlohmann::json call_gemini_with_tools(const std::string& prompt, const std::vector<nlohmann::json>& tools);
        };
    }
}

#endif // LILY_AGENTLOOPSERVICE_HPP