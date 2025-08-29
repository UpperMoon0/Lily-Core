#!/usr/bin/env python3
"""
MCP Client for Lily-Core.

This client connects to the Web-Scout MCP server to perform web searches.
Always uses JSON-RPC MCP protocol via subprocess communication.
"""

import asyncio
import json
import subprocess
import sys
import os
from typing import Dict, Any, Optional
import threading
import time
import signal
from dotenv import load_dotenv

from .core.config import config


class WebScoutMCPClient:
    """Client for Web-Scout MCP server using JSON-RPC protocol."""

    def __init__(self):
        # Always use MCP protocol via subprocess
        self.web_scout_path = config.WEB_SCOUT_MCP_PATH
        self.gemini_api_key = config.GEMINI_API_KEY
        self.process = None
        self.request_id = 0
        self.response_queue = asyncio.Queue()

    def _get_next_id(self) -> str:
        """Get next request ID."""
        self.request_id += 1
        return str(self.request_id)

    async def start(self):
        """Start the MCP connection."""
        # Start local MCP process
        return await self._start_local_process()

    async def _start_local_process(self):
        """Start the local MCP server process."""
        try:
            if not self.gemini_api_key:
                raise ValueError("GEMINI_API_KEY not provided for local MCP mode")

            env = os.environ.copy()
            env['GEMINI_API_KEY'] = self.gemini_api_key

            # Start the MCP server as a subprocess
            self.process = subprocess.Popen(
                [sys.executable, self.web_scout_path, "--mcp"],
                stdin=subprocess.PIPE,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                text=True,
                env=env
            )

            # Start a thread to read responses
            response_thread = threading.Thread(target=self._read_responses)
            response_thread.daemon = True
            response_thread.start()

            # Wait a moment for the server to start
            await asyncio.sleep(2)

            # Initialize the MCP connection
            init_response = await self._send_request({
                'jsonrpc': '2.0',
                'id': self._get_next_id(),
                'method': 'initialize',
                'params': {}
            })

            if init_response.get('error'):
                raise Exception(f"MCP initialization failed: {init_response['error']['message']}")

            print("Local MCP client initialized successfully")
            return True

        except Exception as e:
            print(f"Failed to start local MCP client: {e}")
            return False

    def _read_responses(self):
        """Read responses from the local MCP server."""
        try:
            while self.process and not self.process.poll():
                line = self.process.stdout.readline()
                if not line:
                    break

                line = line.strip()
                if line:
                    try:
                        response = json.loads(line)
                        asyncio.run_coroutine_threadsafe(
                            self.response_queue.put(response),
                            asyncio.get_event_loop()
                        )
                    except json.JSONDecodeError as e:
                        print(f"Invalid JSON response from MCP server: {e}")

        except Exception as e:
            print(f"Error reading MCP responses: {e}")

    async def _send_request(self, request: dict) -> dict:
        """Send a request to the MCP server."""
        try:
            # MCP mode - send request via subprocess
            request_json = json.dumps(request) + '\n'
            self.process.stdin.write(request_json)
            self.process.stdin.flush()

            response = await asyncio.wait_for(
                self.response_queue.get(),
                timeout=30.0
            )
            return response

        except asyncio.TimeoutError:
            return {'error': {'code': -32000, 'message': 'Request timeout'}}
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

    async def stop(self):
        """Stop the connection."""
        if self.process:
            try:
                self.process.terminate()
                await asyncio.wait_for(self._wait_pid(), timeout=5.0)
            except asyncio.TimeoutError:
                self.process.kill()
            except Exception as e:
                print(f"Error stopping MCP process: {e}")

    async def _wait_pid(self):
        """Wait for the process to finish."""
        while self.process.poll() is None:
            await asyncio.sleep(0.1)


class WebSearchTool:
    """Wrapper for web search functionality in Lily-Core."""

    def __init__(self):
        # Always use MCP protocol
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