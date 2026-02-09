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
include_directories(
    include
    third_party/json/include
    third_party/websocketpp
    /usr/include/cpprest
)

# Source files
set(SOURCES
    src/main.cpp
    src/services/AgentLoopService.cpp
    src/services/ChatService.cpp
    src/services/MemoryService.cpp
    src/services/Service.cpp
    src/services/SessionService.cpp
    src/services/TTSService.cpp
    src/services/EchoService.cpp
    src/services/HTTPServer.cpp
    src/services/WebSocketManager.cpp
    src/utils/SystemMetrics.cpp
)

# Header directories (for IDE support)
file(GLOB_RECURSE HEADERS
    "include/lily/**/*.hpp"
)

# Create executable
add_executable(lily_core ${SOURCES})

# Link libraries
target_link_libraries(lily_core PRIVATE pthread cpprest crypto ssl boost_system boost_thread)
