cmake_minimum_required(VERSION 3.10)
project(LilyCore VERSION 1.0.0)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Compiler-specific options
if(MSVC)
    add_compile_options(/W4)
else()
    add_compile_options(-Wall -Wextra -pedantic)
endif()

# Include directories
include_directories(include)

# Source files
set(SOURCES
    src/main.cpp
    src/services/AgentLoopService.cpp
    src/services/ChatService.cpp
    src/services/MemoryService.cpp
    src/services/ToolService.cpp
    src/services/TTSService.cpp
    src/services/WebSocketManager.cpp
)

# Create executable
add_executable(lily_core ${SOURCES})
