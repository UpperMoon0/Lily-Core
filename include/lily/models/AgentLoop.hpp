#ifndef LILY_MODELS_AGENTLOOP_HPP
#define LILY_MODELS_AGENTLOOP_HPP

#include <string>
#include <vector>
#include <chrono>
#include <nlohmann/json.hpp>

namespace lily {
    namespace models {

        enum class AgentStepType {
            THINKING,
            TOOL_CALL,
            RESPONSE
        };

        struct AgentStep {
            int step_number;
            AgentStepType type;
            std::string reasoning;
            std::string tool_name;
            nlohmann::json tool_parameters;
            nlohmann::json tool_result;
            std::chrono::system_clock::time_point timestamp;
            double duration_seconds;
        };

        struct AgentLoop {
            std::string user_id;
            std::string user_message;
            std::vector<AgentStep> steps;
            std::string final_response;
            std::chrono::system_clock::time_point start_time;
            std::chrono::system_clock::time_point end_time;
            bool completed;
            double duration_seconds;
        };

    }
}

#endif // LILY_MODELS_AGENTLOOP_HPP