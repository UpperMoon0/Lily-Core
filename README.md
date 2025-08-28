# 🌸 Lily-Core: AI-Powered Web Search Client

A client application that demonstrates web search functionality through MCP (Model Context Protocol) integration with Web-Scout.

## Features

- 🌐 **Web Search**: Perform intelligent web searches using DuckDuckGo
- 🤖 **AI Summarization**: Get AI-powered summaries using Google Gemini
- 🔄 **Dual Modes**: Support for both summary and detailed analysis
- 🐳 **Container Ready**: Full Docker support with networking
- 🔧 **MCP Integration**: Seamless communication via MCP protocol

## Prerequisites

- Docker and Docker Compose
- Google Gemini API key

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

This will start both Web-Scout (MCP server) and Lily-Core services.

### 3. Interactive Search

```bash
# Access Lily-Core container
docker-compose exec lily-core bash

# Run interactive search
python main.py

# Or search from command line
python main.py "artificial intelligence"
python main.py "machine learning" detailed
```

## Architecture

```
┌─────────────────┐    MCP/HTTP    ┌─────────────────┐
│   Lily-Core     │◄──────────────►│   Web-Scout     │
│                 │                │                 │
│ • MCP Client    │                │ • MCP Server    │
│ • CLI Interface │                │ • REST API      │
│ • Web Search    │                │ • DuckDuckGo    │
└─────────────────┘                └─────────────────┘
                                        │
                                        ▼
                                 ┌─────────────────┐
                                 │   Gemini AI     │
                                 │ • Summarization │
                                 │ • Analysis      │
                                 └─────────────────┘
```

## Configuration

### Environment Variables

| Variable | Description | Default |
|----------|-------------|---------|
| `GEMINI_API_KEY` | Your Google Gemini API key | Required |
| `DOCKER_MODE` | Run in Docker container mode | `false` |
| `WEB_SCOUT_HOST` | Web-Scout server hostname | `web-scout` |
| `WEB_SCOUT_PORT` | Web-Scout server port | `8000` |
| `DEFAULT_SEARCH_MODE` | Search mode preference | `summary` |
| `LOG_LEVEL` | Logging level | `INFO` |

### Docker Mode vs Local Mode

**Local Mode** (default):
- Uses MCP protocol via subprocess communication
- Requires Web-Scout MCP server to be running
- Best for development

**Docker Mode**:
- Uses HTTP REST API communication
- Automatically detects Docker environment
- Best for production/containerized deployments

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

The `compose.yaml` defines two services:

1. **web-scout**: The MCP server (port 8000)
2. **lily-core**: The client application (port 8001)

Services communicate through a Docker network named `lily-network`.

## Development

### Local Development

```bash
# Install dependencies
pip install -r requirements.txt

# Start Web-Scout MCP server
cd ../Web-Scout
python mcp_server.py

# In another terminal, run Lily-Core
cd ../Lily-Core
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