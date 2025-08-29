#!/usr/bin/env python3
"""
Chat Service for Lily-Core

Main use case for chat functionality. Orchestrates conversation management,
tool usage decisions, and response generation using AI models.
"""

import time
from typing import Dict, List, Optional, Any
from datetime import datetime

from core.config import model, chat_settings
from models.message import Conversation, Message
from services.memory_service import get_memory_service
from services.tool_service import get_tool_service


class ChatService:
    """Main service for chat functionality and conversation orchestration."""

    def __init__(self):
        """Initialize chat service."""
        self.memory_service = get_memory_service()
        self.tool_service = get_tool_service()
        self._initialized = False

    async def initialize(self) -> bool:
        """
        Initialize the chat service and its dependencies.

        Returns:
            bool: True if initialization was successful
        """
        try:
            # Initialize tool service
            await self.tool_service.initialize()
            self._initialized = True
            print("✅ Chat service initialized successfully")
            return True
        except Exception as e:
            print(f"❌ Failed to initialize chat service: {e}")
            return False

    async def chat(self, message: str, user_id: str = "default_user") -> Dict[str, Any]:
        """
        Main chat method that processes user input and generates responses.

        Args:
            message: User's message
            user_id: User identifier

        Returns:
            dict: Chat response with metadata
        """
        if not self._initialized:
            await self.initialize()

        start_time = time.time()
        tool_used = False
        tool_name = None

        try:
            # Add user message to conversation
            user_msg = self.memory_service.add_message(user_id, 'user', message)

            # Get conversation context
            conversation = self.memory_service.get_conversation(user_id)
            context_messages = conversation.get_recent_messages(chat_settings.conversation_context_window)
            context_str = self._build_context_string(context_messages)

            # Determine if tool usage is appropriate
            should_use_tool, tool_name, reasoning = self.tool_service.should_use_tool(message, context_str)

            print(f"🤖 Tool decision: {should_use_tool} ({reasoning})")

            if should_use_tool:
                # Use tool and generate response with tool results
                response_text = await self._generate_tool_based_response(message, tool_name)
                tool_used = True
            else:
                # Generate conversational response
                response_text = await self._generate_conversational_response(message, context_str)

            # Add assistant response to conversation
            assistant_msg = self.memory_service.add_message(
                user_id,
                'assistant',
                response_text,
                metadata={
                    'tool_used': tool_used,
                    'tool_name': tool_name if tool_used else None,
                    'response_time': time.time() - start_time
                }
            )

            # Build response
            response = {
                'response': response_text,
                'user_id': user_id,
                'timestamp': assistant_msg.timestamp.isoformat(),
                'used_tool': tool_used,
                'tool_used': tool_name if tool_used else None,
                'conversation_id': f"{user_id}_{conversation.created_at.strftime('%Y%m%d%H%M%S')}"
            }

            return response

        except Exception as e:
            error_msg = f"I'm sorry, I encountered an error: {str(e)}"
            self.memory_service.add_message(user_id, 'assistant', error_msg)

            return {
                'response': error_msg,
                'user_id': user_id,
                'timestamp': datetime.now().isoformat(),
                'used_tool': False,
                'error': str(e)
            }

    def _build_context_string(self, messages: List[Message]) -> str:
        """Build a context string from recent messages."""
        if not messages:
            return ""

        context_lines = []
        for msg in messages:
            role = msg.role.capitalize()
            content = msg.content[:200] + "..." if len(msg.content) > 200 else msg.content
            context_lines.append(f"{role}: {content}")

        return "\n".join(context_lines)

    async def _generate_tool_based_response(self, user_message: str, tool_name: str) -> str:
        """Generate response that incorporates tool results."""
        try:
            # Execute the tool
            tool_result = await self.tool_service.call_tool_by_name(
                tool_name,
                query=user_message
            )

            if 'error' in tool_result:
                return f"I tried to use the {tool_name} tool, but encountered an error: {tool_result['error']}"

            # Extract relevant information from tool result
            if tool_name == 'web_search':
                summary = tool_result.get('summary', 'No information found')
                sources_used = tool_result.get('sources_used', 0)
                search_mode = tool_result.get('search_mode', 'summary')

                # Build response with tool results
                response_parts = []

                response_parts.append(f"Based on my web search, here's what I found:\n\n{summary}")

                if sources_used > 0:
                    source_text = f"{sources_used} source" if sources_used == 1 else f"{sources_used} sources"
                    response_parts.append(f"\n\nThis information was gathered from {source_text}.")

                if search_mode == 'detailed':
                    response_parts.append("\n\nI've provided detailed information for your research.")
                else:
                    response_parts.append("\n\nIf you'd like more detailed information, just let me know!")

                response_parts.append("\n\nIs there anything specific you'd like to explore further?")
            else:
                # Generic tool response
                response_parts = [f"Tool '{tool_name}' executed successfully."]
                if 'summary' in tool_result:
                    response_parts.append(f"Result: {tool_result['summary']}")

            return "".join(response_parts)

        except Exception as e:
            return f"I used the {tool_name} tool but encountered an issue processing the results: {str(e)}"

    async def _generate_conversational_response(self, user_message: str, context: str) -> str:
        """Generate conversational response without tools."""
        try:
            # Build prompt for conversational response
            prompt = f"""You are Lily, a helpful and friendly AI assistant.

Recent conversation:
{context}

User: {user_message}

Please provide a helpful, engaging response. Keep it natural and conversational."""

            # Generate response using the configured model
            response = model.generate_content(prompt)

            return response.text if response else "I'm having trouble generating a response right now."

        except Exception as e:
            return f"I'm sorry, I had trouble generating a conversational response: {str(e)}"

    def get_conversation_history(self, user_id: str, limit: Optional[int] = None) -> List[Dict[str, Any]]:
        """
        Get conversation history for a user.

        Args:
            user_id: User identifier
            limit: Maximum number of messages to return

        Returns:
            list: Conversation messages
        """
        return self.memory_service.get_conversation_history(user_id, limit)

    async def clear_conversation(self, user_id: str) -> bool:
        """
        Clear conversation history for a user.

        Args:
            user_id: User identifier

        Returns:
            bool: True if conversation was cleared
        """
        success = self.memory_service.clear_conversation(user_id)
        return success

    def get_available_tools(self) -> List[Dict[str, Any]]:
        """
        Get information about available tools.

        Returns:
            list: Available tools information
        """
        return self.tool_service.get_available_tools()

    def get_conversation_summary(self, user_id: str) -> Dict[str, Any]:
        """
        Get summary information about a user's conversation.

        Args:
            user_id: User identifier

        Returns:
            dict: Conversation summary
        """
        return self.memory_service.get_conversation_summary(user_id)

    async def cleanup(self) -> None:
        """Clean up service resources."""
        await self.tool_service.cleanup()


# Global chat service instance
_chat_service = None


def get_chat_service() -> ChatService:
    """Get the global chat service instance."""
    global _chat_service
    if _chat_service is None:
        _chat_service = ChatService()
    return _chat_service