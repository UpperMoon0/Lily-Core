#!/usr/bin/env python3
"""
Tool Service for Lily-Core

Use case for managing external tools like web search.
Provides interfaces for tool discovery, execution, and management.
"""

import asyncio
from typing import Dict, List, Optional, Any
from datetime import datetime

from core.config import chat_settings


class ToolService:
    """Service for managing available tools and their execution."""

    def __init__(self):
        """Initialize tool service."""
        self.available_tools: List[Dict[str, Any]] = []
        self.web_search_tool = None
        self._initialized = False

    async def initialize(self) -> bool:
        """
        Initialize the tool service and discover available tools.

        Returns:
            bool: True if initialization was successful
        """
        try:
            # Try to discover web search tool
            await self._discover_tools()
            self._initialized = True
            return True
        except Exception as e:
            print(f"Warning: Tool service initialization failed: {e}")
            return False

    async def _discover_tools(self) -> None:
        """Discover and configure available tools."""
        # Try to import and initialize web search tool
        try:
            from mcp_client import get_web_search_tool, WebSearchTool

            self.web_search_tool = get_web_search_tool()
            await self.web_search_tool.initialize()

            # Add web search tool to available tools
            self.available_tools = [
                {
                    'name': 'web_search',
                    'description': 'Perform web searches and get summaries or detailed information',
                    'when_to_use': 'when the user asks about current events, facts, news, research, or anything that would benefit from up-to-date web information',
                    'parameters': {
                        'query': {'type': 'string', 'description': 'The search query'},
                        'mode': {
                            'type': 'string',
                            'description': 'Response mode: "summary" for concise, "detailed" for in-depth',
                            'enum': ['summary', 'detailed'],
                            'default': 'summary'
                        }
                    }
                }
            ]

            print("✅ Tool service initialized with web search capability")

        except Exception as e:
            print(f"⚠️  Web search tool not available: {e}")
            print("➡️  Continuing without web search capabilities")
            self.available_tools = []
            self.web_search_tool = None

    def should_use_tool(self, user_message: str, conversation_context: Optional[str] = None) -> tuple[bool, str, str]:
        """
        Determine if a tool should be used based on message content and context.

        Args:
            user_message: The user's message
            conversation_context: Recent conversation context

        Returns:
            tuple: (should_use_tool, tool_name, reasoning)
        """
        if not self.available_tools or not self.web_search_tool or not chat_settings.enable_tool_usage:
            return False, "", "No tools available or tools disabled"

        message_lower = user_message.lower()

        # Check for explicit search/web requests
        for keyword in chat_settings.search_keywords:
            if keyword in message_lower:
                return True, "web_search", f"Message contains search-related keyword: '{keyword}'"

        # Check conversation context for follow-up questions
        if conversation_context:
            context_lower = conversation_context.lower()
            if "search result" in context_lower or "web search" in context_lower:
                return True, "web_search", "Recent conversation involved web search"

        # Use LLM-like logic for questions
        if "?" in user_message and len(user_message.split()) > 3:
            return True, "web_search", "Complex question that may benefit from current information"

        return False, "", "Simple conversational message"

    async def execute_tool(self, tool_name: str, parameters: Dict[str, Any]) -> Dict[str, Any]:
        """
        Execute a specific tool with given parameters.

        Args:
            tool_name: Name of the tool to execute
            parameters: Parameters for the tool

        Returns:
            dict: Tool execution result
        """
        if tool_name == 'web_search' and self.web_search_tool:
            try:
                query = parameters.get('query', '')
                mode = parameters.get('mode', 'summary')

                if not query:
                    return {'error': 'Query parameter is required for web search'}

                # Execute the search
                result = await self.web_search_tool.search(query, mode)

                if 'error' in result:
                    return {'error': result['error']}

                # Add metadata
                result['tool_used'] = 'web_search'
                result['search_mode'] = mode
                result['query'] = query

                return result

            except Exception as e:
                return {'error': f'Web search failed: {str(e)}'}
        else:
            return {'error': f'Unknown or unavailable tool: {tool_name}'}

    async def call_tool_by_name(self, tool_name: str, **kwargs) -> Dict[str, Any]:
        """
        Convenience method to call a tool with keyword arguments.

        Args:
            tool_name: Name of the tool to execute
            **kwargs: Tool parameters

        Returns:
            dict: Tool execution result
        """
        return await self.execute_tool(tool_name, kwargs)

    def get_available_tools(self) -> List[Dict[str, Any]]:
        """
        Get list of available tools.

        Returns:
            List[Dict[str, Any]]: Available tools information
        """
        return self.available_tools.copy()

    def is_tool_available(self, tool_name: str) -> bool:
        """
        Check if a specific tool is available.

        Args:
            tool_name: Name of the tool to check

        Returns:
            bool: True if tool is available
        """
        return any(tool['name'] == tool_name for tool in self.available_tools)

    async def cleanup(self) -> None:
        """Clean up tool resources."""
        if self.web_search_tool:
            try:
                await self.web_search_tool.cleanup()
            except Exception as e:
                print(f"Warning: Error cleaning up web search tool: {e}")


# Global tool service instance
_tool_service = None


def get_tool_service() -> ToolService:
    """Get the global tool service instance."""
    global _tool_service
    if _tool_service is None:
        _tool_service = ToolService()
    return _tool_service