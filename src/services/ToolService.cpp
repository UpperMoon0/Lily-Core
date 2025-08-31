#include <lily/services/ToolService.hpp>
#include <iostream>

namespace lily {
    namespace services {
        ToolService::ToolService() {
            discover_tools();
        }

        ToolService::~ToolService() {}

        void ToolService::discover_tools() {
            _tools = {
                {
                    {"name", "web_search"},
                    {"description", "Performs a web search."},
                    {"parameters", {
                        {"query", "The search query."}
                    }}
                },
                {
                    {"name", "file_read"},
                    {"description", "Reads a file."},
                    {"parameters", {
                        {"path", "The path to the file."}
                    }}
                }
            };
        }

        nlohmann::json ToolService::execute_tool(const std::string& tool_name, const nlohmann::json& parameters) {
            std::cout << "Executing tool: " << tool_name << std::endl;
            std::cout << "Parameters: " << parameters.dump(4) << std::endl;

            if (tool_name == "web_search") {
                return {
                    {"status", "success"},
                    {"result", "Web search results for '" + parameters.value("query", "") + "'"}
                };
            } else if (tool_name == "file_read") {
                return {
                    {"status", "success"},
                    {"result", "File content for '" + parameters.value("path", "") + "'"}
                };
            }

            return {
                {"status", "error"},
                {"message", "Tool not found."}
            };
        }

        std::vector<nlohmann::json> ToolService::get_available_tools() const {
            return _tools;
        }
    }
}