#ifndef LILY_AGENTLOOPSERVICE_HPP
#define LILY_AGENTLOOPSERVICE_HPP

#include <lily/services/MemoryService.hpp>
#include <lily/services/ToolService.hpp>
#include <string>

namespace lily {
    namespace services {
        class AgentLoopService {
        public:
            AgentLoopService(MemoryService& memoryService, ToolService& toolService);
            void run_loop(const std::string& user_message, const std::string& user_id);

        private:
            MemoryService& _memoryService;
            ToolService& _toolService;
        };
    }
}

#endif // LILY_AGENTLOOPSERVICE_HPP