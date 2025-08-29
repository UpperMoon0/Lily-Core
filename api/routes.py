#!/usr/bin/env python3
"""
Lily-Core API Routes

HTTP interface adapters for the chat system using FastAPI.
Provides REST endpoints for chat functionality and follows clean architecture.
Enhanced with Agent Loop Architecture for advanced reasoning.
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
async def chat(
    request: ChatRequest,
    use_agent_loop: Optional[bool] = Query(False, description="Use advanced agent loop system for multi-step reasoning")
) -> ChatResponse:
    """
    Send a message to the chatbot and get a response.

    The chatbot can use simple tool logic or advanced agent loop for complex reasoning.
    Agent loop provides multi-step planning, iterative tool usage, and sophisticated problem solving.

    Args:
        request: Chat request with message and user ID
        use_agent_loop: Whether to use advanced agent loop (default: False)

    Returns:
        ChatResponse: Chat response with metadata
    """
    try:
        # Process the chat message using chat service with agent loop option
        result = await chat_service.chat_with_agent_loop(
            message=request.message,
            user_id=request.user_id,
            use_agent_loop=use_agent_loop
        )

        # Build response with enhanced metadata if agent loop was used
        response_data = {
            'response': result['response'],
            'user_id': result['user_id'],
            'timestamp': result['timestamp'],
            'tool_used': result.get('tool_used'),
            'metadata': request.metadata or {}
        }

        # Add agent loop metadata if used
        if use_agent_loop and 'agent_loop' in result:
            response_data['metadata']['agent_loop'] = {
                'used': True,
                'steps': result['agent_loop']['total_steps'],
                'execution_time': result['agent_loop'].get('execution_time', 0)
            }

        return ChatResponse(**response_data)

    except Exception as e:
        raise HTTPException(status_code=500, detail=f"Chat error: {str(e)}")


@router.get("/agent-loop/status")
async def get_agent_loop_status() -> Dict[str, Any]:
    """
    Get the status of the agent loop system.

    Returns:
        dict: Status information about agent loop capabilities
    """
    return {
        "agent_loop_available": True,
        "features": [
            "Multi-step reasoning and planning",
            "Iterative tool execution",
            "Dynamic context updates",
            "Sophisticated problem solving",
            "Fallback to simple chat mode"
        ],
        "max_steps": 5,
        "supported_actions": [
            "EXECUTE_TOOL",
            "GENERATE_RESPONSE",
            "ASK_CLARIFICATION",
            "TERMINATE"
        ],
        "timestamp": datetime.now().isoformat()
    }


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
            "POST /chat": "Send chat message (supports agent loop with ?use_agent_loop=true)",
            "GET /agent-loop/status": "Get agent loop system status",
            "GET /conversation/{user_id}": "Get conversation history",
            "DELETE /conversation/{user_id}": "Clear conversation history",
            "GET /tools": "Get available tools information",
            "GET /conversation-summary/{user_id}": "Get conversation summary"
        },
        features=[
            "Intelligent tool usage (decides when to search web)",
            "Conversation memory and context",
            "Advanced Agent Loop Architecture for multi-step reasoning",
            "Automatic search mode selection (summary/detailed)",
            "Gemini AI powered responses with iterative planning",
            "Reason-Act-Observe pattern for complex problem solving",
            "Clean architecture design"
        ],
        health_endpoint="/health"
    )