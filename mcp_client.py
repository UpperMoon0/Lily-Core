#!/usr/bin/env python3
"""
MCP Client for Lily-Core.

This client connects to the Web-Scout MCP server to perform web searches.
Uses JSON-RPC MCP protocol over HTTP for distributed deployment.
"""

import asyncio
import json
import sys
import os
from typing import Dict, Any, Optional
import requests
from dotenv import load_dotenv
from core.config import config


class WebScoutMCPClient:
    """Client for Web-Scout MCP server using JSON-RPC over HTTP."""

    def __init__(self):
        # Use MCP over HTTP to Web-Scout
        self.web_scout_url = config.WEB_SCOUT_URL
        self.mcp_url = f"{self.web_scout_url}/mcp"
        self.session = requests.Session()
        self.request_id = 0

    def _get_next_id(self) -> str:
        """Get next request ID."""
        self.request_id += 1
        return str(self.request_id)

    async def start(self):
        """Start the HTTP MCP connection."""
        try:
            # Test HTTP connection to MCP endpoint
            response = self.session.get(f"{self.web_scout_url}/health", timeout=5)
            if response.status_code == 200:
                print(f"Web-Scout MCP server is healthy at {self.web_scout_url}")
                return True
            else:
                print(f"Web-Scout server returned status {response.status_code}")
                return False
        except requests.exceptions.RequestException as e:
            print(f"Cannot connect to Web-Scout MCP server: {e}")
            return False

    async def _send_request(self, request: dict) -> dict:
        """Send MCP request over HTTP."""
        try:
            # Send JSON-RPC over HTTP
            response = self.session.post(
                self.mcp_url,
                json=request,
                timeout=30.0
            )

            if response.status_code == 200:
                return response.json()
            else:
                return {'error': {'code': response.status_code, 'message': f'HTTP {response.status_code}: {response.text}'}}

        except requests.exceptions.RequestException as e:
            return {'error': {'code': -32000, 'message': f'Request failed: {e}'}}
        except Exception as e:
            return {'error': {'code': -32000, 'message': str(e)}}

    async def search_web(self, query: str, mode: str = "summary") -> dict:
        """Perform a web search using the Web-Scout server."""
        try:
            # Use MCP protocol
            # First check if tools are available
            tools_response = await self._send_request({
                'jsonrpc': '2.0',
                'id': self._get_next_id(),
                'method': 'tools/list'
            })

            if tools_response.get('error'):
                return {"error": f"Failed to list tools: {tools_response['error']['message']}"}

            # Call the web search tool
            call_response = await self._send_request({
                'jsonrpc': '2.0',
                'id': self._get_next_id(),
                'method': 'tools/call',
                'params': {
                    'name': 'web_search',
                    'arguments': {'query': query, 'mode': mode}
                }
            })

            if call_response.get('error'):
                return {"error": f"Tool call failed: {call_response['error']['message']}"}

            # Extract the result from the content
            result_content = call_response.get('result', {}).get('content', [])
            if result_content:
                try:
                    result_text = result_content[0].get('text', '{}')
                    return json.loads(result_text)
                except json.JSONDecodeError:
                    return {"error": "Invalid JSON response from MCP server"}

            return {"error": "No content received from MCP server"}

        except Exception as e:
            return {"error": f"Web search error: {str(e)}"}

    async def list_tools(self) -> dict:
        """Fetch available tools from MCP server."""
        try:
            tools_response = await self._send_request({
                'jsonrpc': '2.0',
                'id': self._get_next_id(),
                'method': 'tools/list'
            })

            if tools_response.get('error'):
                return {"error": f"Failed to list tools: {tools_response['error']['message']}"}

            return tools_response.get('result', {})
        except Exception as e:
            return {"error": f"Tool listing error: {str(e)}"}

    async def stop(self):
        """Stop the HTTP connection."""
        if self.session:
            self.session.close()
            print("HTTP MCP client connection closed")
    async def stop(self):
        """Stop the HTTP connection."""
        if self.session:
            self.session.close()
            print("HTTP MCP client connection closed")

class WebSearchTool:
    """Wrapper for web search functionality in Lily-Core."""

    def __init__(self):
        # Use MCP-over-HTTP protocol
        self.client = WebScoutMCPClient()

    async def initialize(self):
        """Initialize the client connection."""
        success = await self.client.start()
        if not success:
            print("Failed to initialize Web-Scout connection")
            return False
        return True

    async def search(self, query: str, mode: str = "summary") -> dict:
        """Perform a web search."""
        return await self.client.search_web(query, mode)

    async def cleanup(self):
        """Clean up resources."""
        await self.client.stop()


# Global instance
_search_tool = None


def get_web_search_tool() -> WebSearchTool:
    """Get the global web search tool instance."""
    global _search_tool
    if _search_tool is None:
        _search_tool = WebSearchTool()
    return _search_tool


async def perform_web_search(query: str, mode: str = "summary") -> dict:
    """Convenience function to perform web search."""
    tool = get_web_search_tool()

    # Initialize if not already done
    if not hasattr(tool, '_initialized'):
        await tool.initialize()
        tool._initialized = True

    return await tool.search(query, mode)


def sync_web_search(query: str, mode: str = "summary") -> dict:
    """Synchronous wrapper for web search."""
    try:
        loop = asyncio.get_event_loop()
    except RuntimeError:
        loop = asyncio.new_event_loop()
        asyncio.set_event_loop(loop)

    try:
        return loop.run_until_complete(perform_web_search(query, mode))
    except Exception as e:
        return {"error": f"Web search failed: {str(e)}"}


if __name__ == "__main__":
    # Test the MCP client
    if len(sys.argv) < 2:
        print("Usage: python mcp_client.py <search_query> [mode]")
        sys.exit(1)

    query = sys.argv[1]
    mode = sys.argv[2] if len(sys.argv) > 2 else "summary"

    # Load environment variables
    load_dotenv()

    result = sync_web_search(query, mode)
    print(json.dumps(result, indent=2))