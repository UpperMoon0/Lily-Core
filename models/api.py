#!/usr/bin/env python3
"""
API Models for Lily-Core

Request and response models for the HTTP API.
"""

from typing import Dict, List, Optional, Any
from datetime import datetime
from pydantic import BaseModel


class ChatRequest(BaseModel):
    """Request model for chat messages."""

    message: str
    user_id: Optional[str] = "default_user"
    metadata: Optional[Dict[str, Any]] = None


class ChatResponse(BaseModel):
    """Response model for chat messages."""

    response: str
    user_id: str
    timestamp: str
    used_tool: bool = False
    tool_used: Optional[str] = None
    metadata: Optional[Dict[str, Any]] = None


class ConversationResponse(BaseModel):
    """Response model for conversation history."""

    conversation: List[Dict[str, Any]]
    user_id: str
    message_count: int
    created_at: Optional[str] = None
    updated_at: Optional[str] = None


class ToolInfo(BaseModel):
    """Model for tool information."""

    name: str
    description: str
    parameters: Optional[Dict[str, Any]] = None
    when_to_use: Optional[str] = None


class ToolsResponse(BaseModel):
    """Response model for available tools."""

    tools: List[ToolInfo]
    count: int


class HealthResponse(BaseModel):
    """Health check response model."""

    status: str
    service: str
    version: str
    chatbot_ready: bool
    timestamp: str
    uptime: Optional[str] = None


class ApiInfoResponse(BaseModel):
    """API information response model."""

    message: str
    version: str
    endpoints: Dict[str, str]
    features: List[str]
    health_endpoint: str = "/health"