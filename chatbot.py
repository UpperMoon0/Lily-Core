#!/usr/bin/env python3
"""
Lily-Core ChatBot - AI-Powered Chat with Tool Integration

A chatbot that intelligently decides when to use web search tools
based on conversation context and user needs.
"""

import asyncio
import json
import os
from typing import Dict, List, Optional, Any
from datetime import datetime
from mcp_client import get_web_search_tool, WebSearchTool
import google.generativeai as genai
from dotenv import load_dotenv


class ConversationMemory:
    """Simple conversation memory management."""

    def __init__(self, max_messages: int = 20):
        self.messages = []
        self.max_messages = max_messages

    def add_message(self, role: str, content: str):
        """Add a message to the conversation."""
        message = {
            'role': role,
            'content': content,
            'timestamp': datetime.now().isoformat()
        }
        self.messages.append(message)

        # Keep only recent messages
        if len(self.messages) > self.max_messages:
            self.messages = self.messages[-self.max_messages:]

    def get_recent_messages(self, limit: int = 10) -> List[Dict]:
        """Get recent messages from the conversation."""
        return self.messages[-limit:]

    def get_context(self) -> str:
        """Get formatted conversation context."""
        context_lines = []
        for msg in self.get_recent_messages():
            role = msg['role'].capitalize()
            content = msg['content'][:200] + "..." if len(msg['content']) > 200 else msg['content']
            context_lines.append(f"{role}: {content}")
        return "\n".join(context_lines)


class ChatBot:
    """Intelligent chatbot with optional tool integration."""

    def __init__(self):
        # Load environment variables
        load_dotenv()

        # Initialize Gemini AI
        api_key = os.getenv('GEMINI_API_KEY')
        if not api_key:
            raise ValueError("GEMINI_API_KEY not configured")

        genai.configure(api_key=api_key)

        # Initialize conversation memory
        self.memory = ConversationMemory()

        # Initialize web search tool (optional)
        self.web_search_tool: Optional[WebSearchTool] = None
        self.available_tools = []

        # Create Gemini model
        self.model = genai.GenerativeModel(
            model_name='gemini-2.5-flash',
            generation_config={
                'temperature': 0.7,
                'top_p': 0.8,
                'max_output_tokens': 1000,
            }
        )

    async def initialize(self):
        """Initialize the chatbot and optionally connect to web-scout."""
        try:
            # Initialize web search tool (optional - don't fail if not available)
            try:
                self.web_search_tool = get_web_search_tool()
                await self.web_search_tool.initialize()
                print("✅ Web search tool initialized successfully")
            except Exception as e:
                print(f"⚠️  Web search tool not available: {e}")
                print("➡️  Continuing without web search capabilities")
                self.web_search_tool = None

            # Get available tools from the server (if available)
            await self._discover_tools()

            print("✅ ChatBot initialized successfully")
            return True

        except Exception as e:
            print(f"❌ Failed to initialize ChatBot: {e}")
            return False

    async def _discover_tools(self):
        """Discover available tools from the MCP server (only if web-scout is available)."""
        try:
            # Only add web search tool if it's actually available
            if self.web_search_tool is not None:
                self.available_tools = [
                    {
                        'name': 'web_search',
                        'description': 'Perform web searches and get summaries or detailed information',
                        'when_to_use': 'when the user asks about current events, facts, news, research, or anything that would benefit from up-to-date web information'
                    }
                ]
            else:
                self.available_tools = []
                print("ℹ️  No tools available (web-scout not connected)")
        except Exception as e:
            print(f"Warning: Could not discover tools: {e}")
            self.available_tools = []

    def _should_use_tool(self, user_message: str) -> tuple[bool, str]:
        """
        Determine if a tool should be used based on the message and whether tools are available.

        Returns:
            tuple: (should_use_tool, reasoning)
        """
        # If no tools are available, never use tools
        if not self.available_tools or self.web_search_tool is None:
            return False, "No tools available"

        message_lower = user_message.lower()
        context = self.memory.get_context()

        # Check for explicit search/web requests
        search_keywords = [
            'search', 'find', 'lookup', 'research', 'what is', 'how to',
            'tell me about', 'what are', 'where can', 'latest', 'news',
            'current', 'price of', 'weather', 'recipe for'
        ]

        if any(keyword in message_lower for keyword in search_keywords):
            return True, "Message contains search-related keywords"

        # Check conversation context for follow-up questions
        recent_messages = self.memory.get_recent_messages(3)
        for msg in recent_messages:
            if msg['role'] == 'assistant' and any(word in msg['content'].lower() for word in ['web search', 'search result', 'found online']):
                return True, "Recent conversation involved web search"

        # Use LLM to analyze if tool use is appropriate
        if "?" in user_message and len(user_message.split()) > 3:
            return True, "Complex question that may require current information"

        return False, "Simple conversational message"

    async def _call_tool(self, tool_name: str, **kwargs) -> Dict[str, Any]:
        """Call a specific tool with given parameters."""
        try:
            if tool_name == 'web_search' and self.web_search_tool:
                # Let the LLM decide on the search mode based on context
                query = kwargs.get('query', '')
                search_mode = self._determine_search_mode(query)

                result = await self.web_search_tool.search(query, search_mode)

                if 'error' in result:
                    return {'error': result['error']}

                # Add search metadata
                if 'sources_used' in result:
                    result['search_sources'] = result['sources_used']

                return result

            else:
                return {'error': f'Unknown tool: {tool_name}'}

        except Exception as e:
            return {'error': f'Tool call failed: {str(e)}'}

    def _determine_search_mode(self, query: str) -> str:
        """
        Use conversation context to determine appropriate search mode.
        """
        query_lower = query.lower()
        context = self.memory.get_context()

        # Detailed mode for research-oriented queries
        detailed_keywords = [
            'research', 'paper', 'study', 'analysis', 'comparison',
            'tutorial', 'guide', 'documentation', 'specific'
        ]

        if any(keyword in query_lower or keyword in context.lower() for keyword in detailed_keywords):
            return 'detailed'

        # Summary mode for general questions
        return 'summary'

    async def chat(self, user_message: str) -> str:
        """
        Main chat method that processes user input and generates responses.
        Intelligently decides whether to use tools or respond conversationally.
        """
        try:
            # Add user message to conversation
            self.memory.add_message('user', user_message)

            # Determine if tool use is appropriate
            should_use_tool, reasoning = self._should_use_tool(user_message)

            if should_use_tool:
                # Use tool and incorporate results into response
                response = await self._generate_tool_based_response(user_message)
            else:
                # Generate conversational response
                response = await self._generate_conversational_response(user_message)

            # Add assistant response to conversation
            self.memory.add_message('assistant', response)

            return response

        except Exception as e:
            error_msg = f"I'm sorry, I encountered an error: {str(e)}"
            self.memory.add_message('assistant', error_msg)
            return error_msg

    async def _generate_tool_based_response(self, user_message: str) -> str:
        """Generate response that includes tool results."""
        try:
            # Call web search tool
            search_result = await self._call_tool('web_search', query=user_message)

            if 'error' in search_result:
                return f"I tried to search for information, but encountered an error: {search_result['error']}"

            # Generate enhanced response incorporating search results
            sources = search_result.get('sources_used', 0)
            summary = search_result.get('summary', 'No search results found')
            search_mode = search_result.get('mode', 'summary')

            response = f"Based on my web search, here's what I found:\n\n{summary}"

            if sources > 0:
                response += f"\n\nThis information was gathered from {sources} sources."
            if search_mode == 'detailed':
                response += "\n\nI've provided detailed information for your research."
            else:
                response += "\n\nIf you'd like more detailed information, just let me know!"

            # Add helpful context
            response += "\n\nIs there anything specific you'd like to explore further about this topic?"

            return response

        except Exception as e:
            return f"I searched the web but encountered an issue processing the results: {str(e)}"

    async def _generate_conversational_response(self, user_message: str) -> str:
        """Generate conversational response without tools."""
        try:
            # Build context from conversation
            context = self.memory.get_context()

            # Create prompt for conversational response
            prompt = f"""You are Lily, a helpful and friendly AI assistant.

Recent conversation:
{context}

User: {user_message}

Please provide a helpful, engaging response. Keep it natural and conversational."""

            # Generate response
            response = self.model.generate_content(prompt)

            return response.text if response else "I'm having trouble generating a response right now."

        except Exception as e:
            return f"I'm sorry, I had trouble generating a conversational response: {str(e)}"

    def get_conversation_history(self) -> List[Dict]:
        """Get the full conversation history."""
        return self.messages.copy()

    async def clear_conversation(self):
        """Clear the conversation memory."""
        self.messages = []

    async def cleanup(self):
        """Clean up resources."""
        if self.web_search_tool:
            await self.web_search_tool.cleanup()


# Global chatbot instance
_chatbot_instance = None


def get_chatbot() -> ChatBot:
    """Get the global chatbot instance."""
    global _chatbot_instance
    if _chatbot_instance is None:
        _chatbot_instance = ChatBot()
    return _chatbot_instance


async def create_chatbot() -> ChatBot:
    """Create and initialize a new chatbot instance."""
    bot = ChatBot()
    await bot.initialize()
    return bot