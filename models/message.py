#!/usr/bin/env python3
"""
Message Models for Lily-Core

Domain models for chat messages and conversation data.
"""

from typing import Dict, Any, Optional
from datetime import datetime
from pydantic import BaseModel


class Message(BaseModel):
    """Domain model for a single chat message."""

    role: str  # 'user' or 'assistant'
    content: str
    timestamp: datetime
    metadata: Optional[Dict[str, Any]] = None

    class Config:
        """Pydantic configuration."""
        from_attributes = True

    def to_dict(self) -> Dict[str, Any]:
        """Convert message to dictionary for storage/serialization."""
        return {
            'role': self.role,
            'content': self.content,
            'timestamp': self.timestamp.isoformat(),
            'metadata': self.metadata or {}
        }

    @classmethod
    def from_dict(cls, data: Dict[str, Any]) -> 'Message':
        """Create message from dictionary."""
        return cls(
            role=data['role'],
            content=data['content'],
            timestamp=datetime.fromisoformat(data['timestamp']),
            metadata=data.get('metadata', {})
        )


class Conversation(BaseModel):
    """Domain model for a conversation."""

    user_id: str
    messages: list[Message] = []
    created_at: datetime
    updated_at: datetime
    metadata: Optional[Dict[str, Any]] = None

    class Config:
        """Pydantic configuration."""
        from_attributes = True

    def add_message(self, role: str, content: str, metadata: Optional[Dict[str, Any]] = None) -> Message:
        """Add a new message to the conversation."""
        message = Message(
            role=role,
            content=content,
            timestamp=datetime.now(),
            metadata=metadata
        )
        self.messages.append(message)
        self.updated_at = datetime.now()
        return message

    def get_recent_messages(self, limit: int = 10) -> list[Message]:
        """Get most recent messages from the conversation."""
        return self.messages[-limit:] if limit > 0 else self.messages

    def clear_messages(self) -> None:
        """Clear all messages from the conversation."""
        self.messages = []
        self.updated_at = datetime.now()

    def to_dict(self) -> Dict[str, Any]:
        """Convert conversation to dictionary for storage/serialization."""
        return {
            'user_id': self.user_id,
            'messages': [msg.to_dict() for msg in self.messages],
            'created_at': self.created_at.isoformat(),
            'updated_at': self.updated_at.isoformat(),
            'metadata': self.metadata or {}
        }

    @classmethod
    def from_dict(cls, data: Dict[str, Any]) -> 'Conversation':
        """Create conversation from dictionary."""
        messages = [Message.from_dict(msg_data) for msg_data in data.get('messages', [])]
        return cls(
            user_id=data['user_id'],
            messages=messages,
            created_at=datetime.fromisoformat(data['created_at']),
            updated_at=datetime.fromisoformat(data['updated_at']),
            metadata=data.get('metadata', {})
        )


class ToolResult(BaseModel):
    """Domain model for tool execution results."""

    tool_name: str
    success: bool
    result: Optional[Dict[str, Any]] = None
    error_message: Optional[str] = None
    execution_time: Optional[float] = None
    timestamp: datetime = datetime.now()

    class Config:
        """Pydantic configuration."""
        from_attributes = True

    def to_dict(self) -> Dict[str, Any]:
        """Convert tool result to dictionary."""
        return {
            'tool_name': self.tool_name,
            'success': self.success,
            'result': self.result,
            'error_message': self.error_message,
            'execution_time': self.execution_time,
            'timestamp': self.timestamp.isoformat()
        }