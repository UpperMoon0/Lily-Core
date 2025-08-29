#!/usr/bin/env python3
"""
Lily-Core API Routes

HTTP interface adapters for the chat system using FastAPI.
Provides REST endpoints for chat functionality and follows clean architecture.
"""

from fastapi import APIRouter, HTTPException, Query
from typing import Dict, List, Optional, Any

from models.api import (
    ChatRequest, ChatResponse, ConversationResponse,
    ToolsResponse, ToolInfo, HealthResponse, ApiInfoResponse
)
from services.chat_service import get_chat_service
from services.memory_service import get_memory_service
from core.config import config
from datetime import datetime

router = APIRouter()

# Get service instances (initialized in lifespan)
chat_service = get_chat_service()
memory_service = get_memory_service()


@router.get("/health")
async def health_check() -> HealthResponse:
    """
    Health check endpoint.

    Returns:
        HealthResponse: Health status information
    """
    return HealthResponse(
        status="healthy",
        service="Lily-Core ChatBot",
        version="1.0.0",
        chatbot_ready=True,
        timestamp=datetime.now().isoformat()
    )


@router.post("/chat", response_model=ChatResponse)
async def chat(request: ChatRequest) -> ChatResponse:
    """
    Send a message to the chatbot and get a response.

    The chatbot will intelligently decide whether to use web search tools
    based on the context and content of your message.

    Args:
        request: Chat request with message and user ID

    Returns:
        ChatResponse: Chat response with metadata
    """
    try:
        # Process the chat message using chat service
        result = await chat_service.chat(
            message=request.message,
            user_id=request.user_id
        )

        return ChatResponse(
            response=result['response'],
            user_id=result['user_id'],
            timestamp=result['timestamp'],
            used_tool=result.get('used_tool', False),
            tool_used=result.get('tool_used'),
            metadata=request.metadata
        )

    except Exception as e:
        raise HTTPException(status_code=500, detail=f"Chat error: {str(e)}")


@router.get("/conversation/{user_id}")
async def get_conversation_history(
    user_id: str,
    limit: Optional[int] = Query(None, description="Maximum number of messages to return")
) -> ConversationResponse:
    """
    Get conversation history for a user.

    Args:
        user_id: User identifier
        limit: Maximum number of messages to return

    Returns:
        ConversationResponse: Conversation data with messages
    """
    try:
        conversation = memory_service.get_conversation(user_id)
        messages = memory_service.get_conversation_history(user_id, limit)

        return ConversationResponse(
            conversation=messages,
            user_id=user_id,
            message_count=len(messages),
            created_at=conversation.created_at.isoformat(),
            updated_at=conversation.updated_at.isoformat()
        )

    except Exception as e:
        raise HTTPException(status_code=500, detail=f"Failed to get conversation history: {str(e)}")


@router.delete("/conversation/{user_id}")
async def clear_conversation(user_id: str) -> Dict[str, str]:
    """
    Clear conversation history for a user.

    Args:
        user_id: User identifier

    Returns:
        dict: Success confirmation
    """
    try:
        success = await chat_service.clear_conversation(user_id)
        if success:
            return {"message": f"Conversation cleared for user {user_id}"}
        else:
            return {"message": f"No conversation found for user {user_id}"}

    except Exception as e:
        raise HTTPException(status_code=500, detail=f"Failed to clear conversation: {str(e)}")


@router.get("/tools")
async def get_available_tools() -> ToolsResponse:
    """
    Get information about available tools.

    Returns:
        ToolsResponse: List of available tools with descriptions
    """
    try:
        tools_info = chat_service.get_available_tools()

        # Convert to API model format
        tools = []
        for tool in tools_info:
            tools.append(ToolInfo(
                name=tool.get('name', ''),
                description=tool.get('description', ''),
                parameters=tool.get('parameters'),
                when_to_use=tool.get('when_to_use')
            ))

        return ToolsResponse(
            tools=tools,
            count=len(tools)
        )

    except Exception as e:
        raise HTTPException(status_code=500, detail=f"Failed to get tool information: {str(e)}")


@router.get("/conversation-summary/{user_id}")
async def get_conversation_summary(user_id: str) -> Dict[str, Any]:
    """
    Get summary information about a user's conversation.

    Args:
        user_id: User identifier

    Returns:
        dict: Conversation summary
    """
    try:
        summary = chat_service.get_conversation_summary(user_id)
        return summary

    except Exception as e:
        raise HTTPException(status_code=500, detail=f"Failed to get conversation summary: {str(e)}")


@router.get("/")
async def root() -> ApiInfoResponse:
    """
    Root endpoint with API information.

    Returns:
        ApiInfoResponse: API information and available endpoints
    """
    return ApiInfoResponse(
        message="Welcome to Lily-Core ChatBot API 🌸",
        version="1.0.0",
        endpoints={
            "GET /": "API information",
            "GET /health": "Health check",
            "POST /chat": "Send chat message",
            "GET /conversation/{user_id}": "Get conversation history",
            "DELETE /conversation/{user_id}": "Clear conversation history",
            "GET /tools": "Get available tools information",
            "GET /conversation-summary/{user_id}": "Get conversation summary"
        },
        features=[
            "Intelligent tool usage (decides when to search web)",
            "Conversation memory and context",
            "Automatic search mode selection (summary/detailed)",
            "Gemini AI powered responses",
            "Clean architecture design"
        ]
    )