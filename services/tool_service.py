#!/usr/bin/env python3
"""
Tool Service for Lily-Core

Use case for managing external tools like web search.
Provides interfaces for tool discovery, execution, and management.
"""

import asyncio
import json
from typing import Dict, List, Optional, Any
from datetime import datetime

from core.config import chat_settings, model


# Data structure for tool decision results
class ToolDecision:
    """Represents a tool usage decision with confidence scoring."""
    def __init__(self, should_use: bool, tool_name: str, confidence: float, reasoning: str):
        self.should_use = should_use
        self.tool_name = tool_name
        self.confidence = confidence
        self.reasoning = reasoning


class LLMToolDecider:
    """LLM-powered tool decision making using Gemini."""

    def __init__(self):
        """Initialize the LLM tool decider."""
        self.decision_cache = {}  # Simple cache for repeated decisions

    async def analyze_tool_need(
        self,
        user_message: str,
        available_tools: List[Dict[str, Any]],
        conversation_context: Optional[str] = None
    ) -> ToolDecision:
        """
        Use LLM to analyze if a tool should be used based on user message and available tools.

        Args:
            user_message: The user's current message
            available_tools: List of available tools with their metadata
            conversation_context: Recent conversation history

        Returns:
            ToolDecision: Structured decision result
        """
        try:
            # Create the analysis prompt
            prompt = self._build_analysis_prompt(user_message, available_tools, conversation_context)

            # Get LLM response
            response = model.generate_content(prompt)

            if not response or not response.text:
                return ToolDecision(False, "", 0.0, "Failed to get LLM response")

            # Parse the LLM response
            return self._parse_llm_response(response.text.strip())

        except Exception as e:
            return ToolDecision(False, "", 0.0, f"Tool analysis failed: {str(e)}")

    def _build_analysis_prompt(
        self,
        user_message: str,
        available_tools: List[Dict[str, Any]],
        conversation_context: Optional[str]
    ) -> str:
        """Build the prompt for LLM tool analysis."""

        # Format available tools
        tools_text = ""
        for tool in available_tools:
            tools_text += f"""
TOOL: {tool['name']}
DESCRIPTION: {tool.get('description', 'No description available')}
PARAMETERS: {json.dumps(tool.get('parameters', {}), indent=2)}
"""

        prompt = f"""You are an expert AI assistant that helps decide when to use external tools vs providing conversational responses.

AVAILABLE TOOLS:
{tools_text}

USER MESSAGE: "{user_message}"

{f"RECENT CONVERSATION CONTEXT: {conversation_context}" if conversation_context else ""}

ANALYSIS TASK:
1. Analyze the user's intent and information needs
2. Evaluate if any available tools are needed to fulfill the request
3. Consider if the information is time-sensitive, requires external knowledge, or needs current data

RESPONSE FORMAT:
DECISION: [YES/NO]  (Should a tool be used?)
TOOL: [tool_name or NONE]
CONFIDENCE: [0.0-1.0] (How confident are you in this decision?)
REASONING: [Brief explanation of your decision]

Examples:
DECISION: YES
TOOL: web_search
CONFIDENCE: 0.8
REASONING: User is asking about current events that require up-to-date information

DECISION: NO
TOOL: NONE
CONFIDENCE: 0.9
REASONING: This is a casual conversation that doesn't require external tools

Make your decision:"""

        return prompt

    def _parse_llm_response(self, response_text: str) -> ToolDecision:
        """Parse the LLM response into a structured ToolDecision."""
        try:
            lines = response_text.split('\n')

            should_use = False
            tool_name = ""
            confidence = 0.5
            reasoning = response_text  # Default to full response as reasoning

            for line in lines:
                if line.startswith('DECISION:'):
                    decision_text = line.replace('DECISION:', '').strip().upper()
                    should_use = 'YES' in decision_text

                elif line.startswith('TOOL:'):
                    tool_name = line.replace('TOOL:', '').strip()
                    if tool_name.upper() == 'NONE':
                        tool_name = ""
                        should_use = False

                elif line.startswith('CONFIDENCE:'):
                    confidence_text = line.replace('CONFIDENCE:', '').strip()
                    try:
                        confidence = float(confidence_text)
                    except ValueError:
                        confidence = 0.5

                elif line.startswith('REASONING:'):
                    reasoning = line.replace('REASONING:', '').strip()

            return ToolDecision(should_use, tool_name, confidence, reasoning)

        except Exception as e:
            return ToolDecision(False, "", 0.5, f"Failed to parse LLM response: {str(e)}")

    async def get_available_tools_from_mcp(self) -> List[Dict[str, Any]]:
        """Fetch tools dynamically from MCP server."""
        try:
            from mcp_client import WebScoutMCPClient

            client = WebScoutMCPClient()
            await client.start()

            tools_result = await client.list_tools()
            await client.stop()

            if 'error' in tools_result:
                print(f"Warning: Failed to fetch MCP tools: {tools_result['error']}")
                return []

            # Extract tools from MCP response format
            mcp_tools = tools_result.get('tools', [])

            # Convert MCP tool format to our format
            available_tools = []
            for tool in mcp_tools:
                available_tools.append({
                    'name': tool.get('name', ''),
                    'description': tool.get('description', ''),
                    'parameters': tool.get('inputSchema', {}).get('properties', {})
                })

            return available_tools

        except Exception as e:
            print(f"Warning: Could not fetch MCP tools: {e}")
            return []


class ToolService:
    """Service for managing available tools and their execution."""

    def __init__(self):
        """Initialize tool service."""
        self.available_tools: List[Dict[str, Any]] = []
        self.web_search_tool = None
        self.llm_decider = LLMToolDecider()
        self._initialized = False

    async def initialize(self) -> bool:
        """
        Initialize the tool service and discover available tools.
        Always succeeds but may have reduced functionality if MCP servers are down.

        Returns:
            bool: True if initialization was successful (always succeeds)
        """
        try:
            # Try to discover web search tool
            tool_discovery_success = await self._discover_tools()
            self._initialized = True
            if not tool_discovery_success:
                print("⚠️  Tool discovery failed - system will operate in conversational-only mode")
            return True
        except Exception as e:
            # Even if there's an exception, continue with empty tools
            print(f"Warning: Tool service initialization encountered errors: {e}")
            print("⚠️  Continuing with conversational-only mode")
            self.available_tools = []  # Reset to empty
            self.web_search_tool = None
            self._initialized = True
            return True

    async def _discover_tools(self) -> bool:
        """Discover and configure available tools.

        Returns:
            bool: True if tools were successfully discovered, False if MCP server is down
        """
        # Try to import and initialize web search tool
        try:
            from mcp_client import get_web_search_tool, WebSearchTool

            self.web_search_tool = get_web_search_tool()
            init_success = await self.web_search_tool.initialize()

            if not init_success:
                print("⚠️  MCP client initialization failed - MCP server may be down")
                self.available_tools = []
                self.web_search_tool = None
                return False

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
            return True

        except Exception as e:
            print(f"⚠️  Web search tool not available: {e}")
            print("➡️  Continuing without web search capabilities")
            print("💡 System will operate in conversational-only mode")
            self.available_tools = []
            self.web_search_tool = None
            return False

    async def should_use_tool(self, user_message: str, conversation_context: Optional[str] = None) -> tuple[bool, str, str]:
        """
        Determine if a tool should be used based on LLM-powered analysis of message content and context.

        Args:
            user_message: The user's message
            conversation_context: Recent conversation context

        Returns:
            tuple: (should_use_tool, tool_name, reasoning)
        """
        # Fallback to keyword-based logic if tools disabled or not available
        if not chat_settings.enable_tool_usage:
            return False, "", "Tool usage is disabled in settings"

        if not self.available_tools or not self.web_search_tool:
            return False, "", "No tools available or not initialized"

        try:
            # Get fresh tool metadata from MCP server
            fresh_tools = await self.llm_decider.get_available_tools_from_mcp()

            # If we can't get fresh tools, use cached ones
            tools_to_analyze = fresh_tools if fresh_tools else self.available_tools

            # Use LLM to analyze if tool usage is appropriate
            decision = await self.llm_decider.analyze_tool_need(
                user_message,
                tools_to_analyze,
                conversation_context
            )

            print(f"🤖 LLM Tool Decision: {decision.should_use} (confidence: {decision.confidence:.2f}) - {decision.reasoning}")

            # Only use tool if confidence is above threshold
            confidence_threshold = 0.6  # Adjust based on testing
            if decision.should_use and decision.confidence >= confidence_threshold:
                return True, decision.tool_name, f"LLM analysis: {decision.reasoning} (confidence: {decision.confidence:.2f})"
            else:
                return False, "", f"LLM analysis: {decision.reasoning} (confidence: {decision.confidence:.2f})"

        except Exception as e:
            print(f"⚠️  LLM tool analysis failed, falling back to keyword-based logic: {e}")
            # Fallback to the original keyword-based logic
            return self._fallback_keyword_decision(user_message, conversation_context)

    def _fallback_keyword_decision(self, user_message: str, conversation_context: Optional[str] = None) -> tuple[bool, str, str]:
        """
        Fallback to the original keyword-based decision logic.

        Args:
            user_message: The user's message
            conversation_context: Recent conversation context

        Returns:
            tuple: (should_use_tool, tool_name, reasoning)
        """
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

        # Use basic logic for questions
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