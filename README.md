# Lily-Core: AI-Powered ChatBot with MCP Protocol

A clean architecture implementation of an AI-powered chatbot that provides intelligent web search capabilities through JSON-RPC MCP (Model Context Protocol) integration with Web-Scout.

## Features

- **AI ChatBot**: Powered by Google Gemini with conversation memory
- **Intelligent Web Search**: Context-aware web search using Web-Scout integration via MCP
- **Search Modes**: Configurable summary and detailed analysis modes
- **Clean Architecture**: Well-structured, maintainable, and testable codebase
- **RESTful API**: Complete HTTP API with FastAPI
- **MCP Protocol**: JSON-RPC communication for tool integration
- **Persistent Storage**: Conversation history and state management
- **Testable Design**: Unit and integration test support

## Prerequisites

- Docker and Docker Compose
- Google Gemini API key
- Web-Scout service (optional, for full functionality)

## Quick Start

### 1. Environment Setup

```bash
# Copy environment template
cp .env.template .env

# Edit .env file and add your Gemini API key
# GEMINI_API_KEY=your_actual_gemini_api_key_here
```

### 2. Docker Deployment

```bash
# From the project root directory
docker-compose up --build
```

This will start the Lily-Core service and optionally connect to Web-Scout.

### 3. HTTP API Usage

```bash
# Health check
curl http://localhost:8000/health

# Send a chat message
curl -X POST http://localhost:8000/chat \
  -H "Content-Type: application/json" \
  -d '{"message": "What is artificial intelligence?", "user_id": "test_user"}'

# Get conversation history
curl http://localhost:8000/conversation/test_user

# Clear conversation
curl -X DELETE http://localhost:8000/conversation/test_user
```

## Architecture

```
┌─────────────────────────────────────────────────┐
│                Interface Adapters               │
│  ┌─────────────────┐     ┌─────────────────┐    │
│  │   FastAPI       │     │   Routes        │    │
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

## Configuration

### Environment Variables

| Variable | Description | Default |
|----------|-------------|---------|
| `GEMINI_API_KEY` | Your Google Gemini API key | Required |
| `WEB_SCOUT_MCP_PATH` | Path to Web-Scout MCP server | `../Web-Scout/mcp_server.py` |
| `DEFAULT_SEARCH_MODE` | Search mode preference | `summary` |
| `LOG_LEVEL` | Logging level | `INFO` |

### MCP Protocol

Lily-Core communicates with Web-Scout using JSON-RPC MCP (Model Context Protocol) via subprocess communication:
- Launches Web-Scout MCP server as a subprocess
- Uses JSON-RPC protocol for tool calls and web search
- Ensures consistent behavior across all environments

## Usage Examples

### Command Line

```bash
# Simple search
python main.py "latest AI news"

# Detailed analysis
python main.py "quantum computing" detailed

# Interactive mode
python main.py
```

### Python API

```python
from mcp_client import sync_web_search

# Simple search
result = sync_web_search("artificial intelligence")
print(result['summary'])

# Detailed search
result = sync_web_search("machine learning", mode="detailed")
print(result['summary'])
```

### MCP Client Direct Usage

```python
from mcp_client import WebSearchTool

async def search_example():
    tool = WebSearchTool()
    await tool.initialize()

    result = await tool.search("web development trends")
    print(result['summary'])

    await tool.cleanup()
```

## API Reference

### `sync_web_search(query, mode="summary")`

Perform a synchronous web search.

**Parameters:**
- `query` (str): The search query
- `mode` (str): Response mode - "summary" or "detailed"

**Returns:**
- `dict`: Search results with summary and metadata

### `WebSearchTool` Class

Main class for web search functionality.

**Methods:**
- `initialize()`: Initialize the client connection
- `search(query, mode)`: Perform a web search
- `cleanup()`: Clean up resources

## Docker Services

The `compose.yaml` defines Lily-Core as a single service that launches the Web-Scout MCP server as a subprocess:

1. **lily-core**: The main application that handles HTTP API requests and manages the MCP subprocess

This simplified architecture ensures consistent behavior across all environments using JSON-RPC MCP protocol.

## Development

### Local Development

```bash
# Install dependencies
pip install -r requirements.txt

# Run Lily-Core (automatically starts Web-Scout MCP as subprocess)
python main.py
```

### Testing

```bash
# Run a test search
python mcp_client.py "test query"

# Test Docker integration
docker-compose build
docker-compose up -d
docker-compose logs lily-core
```

## Troubleshooting

### Common Issues

**"Cannot connect to Web-Scout server"**
- Ensure Web-Scout service is running and healthy
- Check Docker network connectivity
- Verify environment variables

**"GEMINI_API_KEY not configured"**
- Copy `.env.template` to `.env`
- Add your actual Gemini API key
- Restart the services

**"MCP initialization failed"**
- Check Web-Scout MCP server logs
- Ensure Gemini API key is valid
- Verify Python dependencies

### Logs and Debugging

```bash
# View service logs
docker-compose logs web-scout
docker-compose logs lily-core

# View specific container logs
docker logs lily-core

# Enter container for debugging
docker-compose exec lily-core bash
```

## Security Notes

- API keys are stored securely in `.env` files
- Never commit `.env` files to version control
- Use environment-specific configurations
- The `.env` file is ignored by Git

## License

This project is part of the Lily AI ecosystem.