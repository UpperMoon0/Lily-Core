# Lily-Core-New: AI-Powered Chat Service in C++

A C++ implementation of the AI-powered chat service that provides intelligent web search capabilities through JSON-RPC MCP (Model Context Protocol) integration, enhanced with advanced Agent Loop Architecture for multi-step reasoning.

## Features

- **AI Chat Service**: Powered by Google Gemini with conversation memory and advanced Agent Loop Architecture
- **Intelligent Web Search**: Context-aware web search using Web-Scout integration via MCP
- **Advanced Agent Loop Architecture**: Multi-step reasoning and planning for complex tasks
- **Search Modes**: Configurable summary and detailed analysis modes
- **Clean Architecture**: Well-structured, maintainable, and testable codebase
- **RESTful API**: Complete HTTP API with Crow (C++ web framework)
- **MCP Protocol**: JSON-RPC communication for tool integration
- **Persistent Storage**: Conversation history and state management

## Prerequisites

- C++17 compatible compiler (GCC 7+, Clang 5+, MSVC 2017+)
- CMake 3.10+
- Crow C++ microframework
- JSON for Modern C++ (nlohmann/json)
- libcurl for HTTP requests
- Google Gemini C++ SDK (or REST API client)

## Quick Start

### 1. Build Dependencies

```bash
# Install dependencies (Ubuntu/Debian)
sudo apt-get update
sudo apt-get install cmake build-essential libcurl4-openssl-dev

# Clone and build Crow
git clone https://github.com/CrowCpp/Crow.git
cd Crow
mkdir build && cd build
cmake ..
make -j$(nproc)
sudo make install
```

### 2. Build Lily-Core

```bash
cd Lily-Core-New
mkdir build && cd build
cmake ..
make -j$(nproc)
```

### 3. Configuration

Create a `.env` file with your configuration:

```bash
# Copy environment template
cp .env.template .env

# Edit .env file and add your Gemini API key
# GEMINI_API_KEY=your_actual_gemini_api_key_here
```

### 4. Run

```bash
# From the build directory
./lily-core
```

## Architecture

```
┌─────────────────────────────────────────────────┐
│                Interface Adapters               │
│  ┌─────────────────┐     ┌─────────────────┐    │
│  │   Crow HTTP     │     │   Routes        │    │
│  │   Controllers   │◄────┤   (HTTP API)    │    │
│  │                 │     │                 │    │
│  │ • HTTP Request  │     │ • JSON Models   │    │
│  │   Handling      │     │ • Endpoints     │    │
│  │ • Response      │     │ • CORS Config   │    │
│  │   Formatting    │     │                 │    │
│  └─────────────────┘     └─────────────────┘    │
└─────────────────────────────────────────────────┘
                         │
                         ▼
┌─────────────────────────────────────────────────┐
│                 Use Cases                       │
│  ┌─────────────────┐     ┌──────────────────┐   │
│  │   Chat Service  │     │   Tool Service   │   │
│  │                 │     │                  │   │
│  │ • Conversation  │     │ • Web Search     │   │
│  │   Orchestration │     │ • Tool Management│   │
│  │ • Response      │     │ • Decision Logic │   │
│  │   Generation    │     │ • Tool Calls     │   │
│  │                 │     │                  │   │
│  └─────────────────┘     └──────────────────┘   │
│  ┌─────────────────┐                            │
│  │ Memory Service  │                            │
│  │                 │                            │
│  │ • Conversation  │                            │
│  │   Storage       │                            │
│  │ • History Mgmt  │                            │
│  │ • Persistence   │                            │
│  └─────────────────┘                            │
└─────────────────────────────────────────────────┘
                                  │
                                  ▼
┌─────────────────────────────────────────────────┐
│                 Domain Entities                 │
│  ┌─────────────────┐     ┌─────────────────┐    │
│  │  Models         │     │ Business Config │    │
│  │                 │     │                 │    │
│  │ • Message       │     │ • ChatSettings  │    │
│  │ • Conversation  │     │ • Configuration │    │
│  │ • API DTOs      │     │ • Core Entities │    │
│  │ • Validation    │     │                 │    │
│  └─────────────────┘     └─────────────────┘    │
└─────────────────────────────────────────────────┘
                         │
                         ▼
┌─────────────────────────────────────────────────┐
│                 External Systems                │
│  ┌─────────────────┐     ┌─────────────────┐    │
│  │   Web-Scout     │     │   Gemini AI     │    │
│  │   (MCP/HTTP)    │     │                 │    │
│  │                 │     │ • AI Model      │    │
│  │ • Web Search    │     │ • Generation    │    │
│  │ • Data Sources  │     │ • Context Mgmt  │    │
│  │                 │     │                 │    │
│  └─────────────────┘     └─────────────────┘    │
└─────────────────────────────────────────────────┘
```

## API Reference

### Health Check
```
GET /health
```

### Chat
```
POST /chat
{
  "message": "Hello, how are you?",
  "user_id": "user123"
}
```

### Get Conversation History
```
GET /conversation/{user_id}
```

### Clear Conversation
```
DELETE /conversation/{user_id}
```

## Development

### Building

```bash
mkdir build && cd build
cmake ..
make
```

### Testing

```bash
# Run unit tests
./tests/lily_core_tests
```

## License

This project is part of the Lily AI ecosystem.