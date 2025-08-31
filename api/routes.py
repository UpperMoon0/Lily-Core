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
    ToolsResponse, ToolInfo, HealthResponse, ApiInfoResponse,
    MonitoringResponse, ServiceStatus, SystemMetrics
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
        service="Lily-Core Chat Service",
        version="1.0.0",
        chatbot_ready=True,
        timestamp=datetime.now().isoformat()
    )


@router.post("/chat", response_model=ChatResponse)
async def chat(request: ChatRequest) -> ChatResponse:
    """
    Send a message to the chatbot and get a response.

    The chatbot uses the advanced agent loop system for complex reasoning.
    Agent loop provides multi-step planning, iterative tool usage, and sophisticated problem solving.

    Args:
        request: Chat request with message and user ID

    Returns:
        ChatResponse: Chat response with metadata
    """
    try:
        # Process the chat message using chat service with agent loop (always enabled)
        result = await chat_service.chat_with_agent_loop(
            message=request.message,
            user_id=request.user_id
        )

        # Build response with agent loop metadata
        response_data = {
            'response': result['response'],
            'user_id': result['user_id'],
            'timestamp': result['timestamp'],
            'tool_used': result.get('tool_used'),
            'metadata': request.metadata or {}
        }

        # Add agent loop metadata
        if 'agent_loop' in result:
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


@router.get("/monitoring")
async def get_monitoring_status() -> MonitoringResponse:
    """
    Get comprehensive monitoring status of the Lily Core service and all connected services.
    
    Returns:
        MonitoringResponse: Detailed system status and metrics for all services
    """
    try:
        import psutil
        import time
        import requests
        from datetime import datetime, timedelta
        from core.config import config
        
        # Get system metrics
        cpu_usage = psutil.cpu_percent(interval=1)
        memory_info = psutil.virtual_memory()
        disk_info = psutil.disk_usage('/')
        
        # Calculate uptime
        boot_time = psutil.boot_time()
        uptime = str(timedelta(seconds=time.time() - boot_time))
        
        metrics = SystemMetrics(
            cpu_usage=cpu_usage,
            memory_usage=memory_info.percent,
            disk_usage=(disk_info.used / disk_info.total) * 100,
            uptime=uptime
        )
        
        # Get service status (chatbot)
        chatbot_status = ServiceStatus(
            name="Chat Service",
            status="healthy" if chat_service else "down",
            details={"ready": chat_service is not None},
            last_updated=datetime.now().isoformat()
        )
        
        # Get service status (memory)
        memory_status = ServiceStatus(
            name="Memory Service",
            status="healthy" if memory_service else "down",
            details={"ready": memory_service is not None},
            last_updated=datetime.now().isoformat()
        )
        
        # Get status from other services
        other_services = []
        
        # Web-Scout status
        try:
            web_scout_response = requests.get(f"{config.WEB_SCOUT_MONITORING_URL}/monitoring", timeout=5)
            if web_scout_response.status_code == 200:
                web_scout_data = web_scout_response.json()
                other_services.append(ServiceStatus(
                    name="Web-Scout",
                    status=web_scout_data.get("status", "unknown"),
                    details=web_scout_data.get("details", {}),
                    last_updated=web_scout_data.get("timestamp", datetime.now().isoformat())
                ))
            else:
                other_services.append(ServiceStatus(
                    name="Web-Scout",
                    status="down",
                    details={"error": f"HTTP {web_scout_response.status_code}"},
                    last_updated=datetime.now().isoformat()
                ))
        except Exception as e:
            other_services.append(ServiceStatus(
                name="Web-Scout",
                status="down",
                details={"error": str(e)},
                last_updated=datetime.now().isoformat()
            ))
        
        # TTS-Provider status
        try:
            tts_provider_response = requests.get(f"{config.TTS_PROVIDER_MONITORING_URL}/monitoring", timeout=5)
            if tts_provider_response.status_code == 200:
                tts_provider_data = tts_provider_response.json()
                other_services.append(ServiceStatus(
                    name="TTS-Provider",
                    status=tts_provider_data.get("status", "unknown"),
                    details=tts_provider_data.get("details", {}),
                    last_updated=tts_provider_data.get("timestamp", datetime.now().isoformat())
                ))
            else:
                other_services.append(ServiceStatus(
                    name="TTS-Provider",
                    status="down",
                    details={"error": f"HTTP {tts_provider_response.status_code}"},
                    last_updated=datetime.now().isoformat()
                ))
        except Exception as e:
            other_services.append(ServiceStatus(
                name="TTS-Provider",
                status="down",
                details={"error": str(e)},
                last_updated=datetime.now().isoformat()
            ))
        
        # Combine all services
        all_services = [chatbot_status, memory_status] + other_services
        
        # Overall status - healthy if core services are healthy and at least one external service is healthy
        core_healthy = chat_service and memory_service
        external_healthy = any(service.status == "healthy" for service in other_services)
        overall_status = "healthy" if core_healthy and (len(other_services) == 0 or external_healthy) else "degraded"
        
        return MonitoringResponse(
            status=overall_status,
            service_name="Lily-Core",
            version="1.0.0",
            timestamp=datetime.now().isoformat(),
            metrics=metrics,
            services=all_services
        )
        
    except ImportError:
        # If psutil is not available, return basic status
        return MonitoringResponse(
            status="healthy",
            service_name="Lily-Core",
            version="1.0.0",
            timestamp=datetime.now().isoformat(),
            details={"message": "Detailed metrics not available (psutil not installed)"}
        )
    except Exception as e:
        raise HTTPException(status_code=500, detail=f"Failed to get monitoring status: {str(e)}")


@router.get("/")
async def root() -> ApiInfoResponse:
    """
    Root endpoint with API information.

    Returns:
        ApiInfoResponse: API information and available endpoints
    """
    return ApiInfoResponse(
        message="Welcome to Lily-Core Chat Service API 🌸",
        version="1.0.0",
        endpoints={
            "GET /": "API information",
            "GET /health": "Health check",
            "GET /monitoring": "Comprehensive system monitoring",
            "POST /chat": "Send chat message (uses agent loop by default)",
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