#include "lily/services/ToolService.hpp"

#include <iostream>

ToolService::ToolService() {
    // Constructor implementation
}

ToolService::~ToolService() {
    // Destructor implementation
}

void ToolService::executeTool(const char* toolName) {
    std::cout << "Executing tool: " << toolName << std::endl;
}