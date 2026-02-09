#ifndef LILY_CONTROLLER_CHAT_CONTROLLER_HPP
#define LILY_CONTROLLER_CHAT_CONTROLLER_HPP

#include <memory>
#include <string>
#include <functional>
#include <nlohmann/json.hpp>

namespace lily {
    namespace services {
        class ChatService;
        class AgentLoopService;
        class MemoryService;
    }
}

namespace lily {
namespace controller {

    class ChatController {
    public:
        ChatController(
            std::shared_ptr<services::ChatService> chatService,
            std::shared_ptr<services::AgentLoopService> agentLoopService,
            std::shared_ptr<services::MemoryService> memoryService
        );

        // POST /api/chat
        // callback(response_json, status_code)
        void chat(const nlohmann::json& request, std::function<void(const nlohmann::json&, int)> callback);

        // GET /api/agent-loops
        nlohmann::json getAgentLoops();

        // GET /api/conversation/{user_id}
        nlohmann::json getConversation(const std::string& userId);

        // DELETE /api/conversation/{user_id}
        void clearConversation(const std::string& userId);

    private:
        std::shared_ptr<services::ChatService> _chatService;
        std::shared_ptr<services::AgentLoopService> _agentLoopService;
        std::shared_ptr<services::MemoryService> _memoryService;
    };

}
}

#endif // LILY_CONTROLLER_CHAT_CONTROLLER_HPP
