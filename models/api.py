#!/usr/bin/env python3
"""
API Models for Lily-Core

Request and response models for the HTTP API.
"""

from typing import Dict, List, Optional, Any, Literal
from datetime import datetime
from pydantic import BaseModel, Field


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
# WebSocket Message Models for TTS functionality

class WebSocketMessage(BaseModel):
    """Base WebSocket message model with discriminated union by type."""

    type: str


class TTSSettingsUpdatePayload(BaseModel):
    """Payload for TTS settings update message."""

    speaker: int = Field(default=1, gt=0, description="TTS speaker/voice ID")
    sample_rate: int = Field(default=24000, gt=0, description="Audio sample rate in Hz")
    model: str = Field(default="edge", min_length=1, description="TTS model to use")
    lang: str = Field(default="ja-JP", min_length=2, description="Language and locale identifier")


class TTSRequestPayload(BaseModel):
    """Payload for TTS request message."""

    text: str = Field(..., min_length=1, max_length=10000, description="Text to synthesize to speech")


class TTSResponsePayload(BaseModel):
    """Payload for TTS response message."""

    status: Literal["success", "queued", "error"] = Field(..., description="Response status")
    message: Optional[str] = Field(None, description="Optional status message")
    sample_rate: Optional[int] = Field(None, gt=0, description="Audio sample rate if applicable")
    channels: Optional[int] = Field(1, gt=0, le=2, description="Number of audio channels")
    bit_depth: Optional[int] = Field(16, gt=0, description="Audio bit depth")


class WebSocketErrorPayload(BaseModel):
    """Payload for WebSocket error message."""

    code: int = Field(..., gt=0, le=599, description="HTTP-style error code")
    message: str = Field(..., min_length=1, description="Error message description")


class TTSSettingsUpdate(WebSocketMessage):
    """TTS settings update message."""

    type: Literal["settings.update"] = "settings.update"
    payload: TTSSettingsUpdatePayload


class TTSRequest(WebSocketMessage):
    """TTS request message from client."""

    type: Literal["tts.request"] = "tts.request"
    payload: TTSRequestPayload


class TTSResponse(WebSocketMessage):
    """TTS response message to client."""

    type: Literal["tts.response"] = "tts.response"
    payload: TTSResponsePayload


class AudioChunk(WebSocketMessage):
    """Audio chunk message with binary data.

    Note: Actual audio data is sent as binary WebSocket message, not JSON.
    """

    type: Literal["audio.chunk"] = "audio.chunk"


class AudioStreamEnd(WebSocketMessage):
    """Audio stream termination message."""

    type: Literal["audio.stream.end"] = "audio.stream.end"


class WebSocketError(WebSocketMessage):
    """Error message model."""

    type: Literal["error"] = "error"
    payload: WebSocketErrorPayload


class TTSSettings(BaseModel):
    """TTS settings model for per-client storage."""

    speaker: int = Field(default=1, gt=0, description="TTS speaker/voice ID")
    sample_rate: int = Field(default=24000, gt=0, description="Audio sample rate in Hz")
    model: str = Field(default="edge", min_length=1, description="TTS model to use")
    lang: str = Field(default="ja-JP", min_length=2, description="Language and locale identifier")


class AudioStreamMetadata(BaseModel):
    """Metadata for audio streaming with enhanced validation."""

    status: str = Field(..., min_length=1, description="Stream status")
    message: Optional[str] = Field(None, description="Optional status message")
    sample_rate: Optional[int] = Field(None, gt=0, description="Audio sample rate in Hz")
    channels: int = Field(default=1, gt=0, le=2, description="Number of audio channels")
    bit_depth: int = Field(default=16, gt=0, description="Audio bit depth")
    length_bytes: Optional[int] = Field(None, ge=0, description="Total length in bytes")
    format: Optional[str] = Field(None, description="Audio format description")


class TTSServiceRequest(BaseModel):
    """Request model for TTS service calls with validation."""

    text: str = Field(..., min_length=1, max_length=10000, description="Text to synthesize")
    speaker: int = Field(default=1, gt=0, description="TTS speaker/voice ID")
    sample_rate: int = Field(default=24000, gt=0, description="Audio sample rate in Hz")
    model: str = Field(default="edge", min_length=1, description="TTS model to use")
    lang: str = Field(default="ja-JP", min_length=2, description="Language and locale identifier")


class TTSServiceResponse(BaseModel):
    """Response model from TTS service with enhanced validation."""

    status: str = Field(..., min_length=1, description="Response status")
    message: Optional[str] = Field(None, description="Optional status message")
    audio_data: Optional[bytes] = Field(None, description="Raw audio data bytes")
    metadata: Optional[Dict[str, Any]] = Field(None, description="Additional metadata")


class ServiceStatus(BaseModel):
    """Model for individual service status."""
    
    name: str
    status: str  # "healthy", "degraded", "down"
    details: Optional[Dict[str, Any]] = None
    last_updated: str


class SystemMetrics(BaseModel):
    """Model for system metrics."""
    
    cpu_usage: Optional[float] = None
    memory_usage: Optional[float] = None
    disk_usage: Optional[float] = None
    uptime: Optional[str] = None


class MonitoringResponse(BaseModel):
    """Response model for system monitoring."""
    
    status: str  # "healthy", "degraded", "down"
    service_name: str
    version: str
    timestamp: str
    metrics: Optional[SystemMetrics] = None
    services: Optional[List[ServiceStatus]] = None
    details: Optional[Dict[str, Any]] = None