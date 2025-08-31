#include <lily/services/AgentLoopService.hpp>
#include <iostream>

namespace lily {
    namespace services {
        AgentLoopService::AgentLoopService(MemoryService& memoryService, ToolService& toolService)
            : _memoryService(memoryService), _toolService(toolService) {}

        void AgentLoopService::run_loop(const std::string& user_message, const std::string& user_id) {
            std::cout << "Fetching user input..." << std::endl;
            std::cout << "Thinking..." << std::endl;
            std::cout << "Executing tool..." << std::endl;
        }
    }
}