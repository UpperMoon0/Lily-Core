#!/usr/bin/env python3
"""
Memory Service for Lily-Core

Use case for managing conversation memory and history.
Provides interfaces for storing, retrieving, and managing conversations.
"""

import os
import json
from typing import Dict, List, Optional, Any
from datetime import datetime
from pathlib import Path

from models.message import Conversation, Message


class MemoryService:
    """Service for managing conversation memory and persistence."""

    def __init__(self, storage_path: Optional[str] = None):
        """
        Initialize memory service.

        Args:
            storage_path: Path to store conversation data (optional)
        """
        self.storage_path = Path(storage_path) if storage_path else Path("conversations")
        self.storage_path.mkdir(exist_ok=True)
        self.conversations: Dict[str, Conversation] = {}

        # Try to load existing conversations
        self._load_conversations()

    def _get_conversation_file(self, user_id: str) -> Path:
        """Get the file path for a user's conversation."""
        return self.storage_path / f"{user_id}.json"

    def _load_conversations(self) -> None:
        """Load all conversations from storage."""
        if not self.storage_path.exists():
            return

        for file_path in self.storage_path.glob("*.json"):
            try:
                user_id = file_path.stem
                with open(file_path, 'r', encoding='utf-8') as f:
                    data = json.load(f)
                conversation = Conversation.from_dict(data)
                self.conversations[user_id] = conversation
            except Exception as e:
                print(f"Warning: Failed to load conversation for user {user_id}: {e}")

    def _save_conversation(self, conversation: Conversation) -> None:
        """Save a conversation to storage."""
        file_path = self._get_conversation_file(conversation.user_id)
        with open(file_path, 'w', encoding='utf-8') as f:
            json.dump(conversation.to_dict(), f, indent=2, ensure_ascii=False)

    def get_conversation(self, user_id: str) -> Conversation:
        """
        Get or create a conversation for a user.

        Args:
            user_id: The user's identifier

        Returns:
            Conversation: The user's conversation object
        """
        if user_id not in self.conversations:
            self.conversations[user_id] = Conversation(
                user_id=user_id,
                created_at=datetime.now(),
                updated_at=datetime.now()
            )

        return self.conversations[user_id]

    def add_message(self, user_id: str, role: str, content: str, metadata: Optional[Dict[str, Any]] = None) -> Message:
        """
        Add a message to a user's conversation.

        Args:
            user_id: The user's identifier
            role: Message role ('user' or 'assistant')
            content: Message content
            metadata: Optional metadata for the message

        Returns:
            Message: The created message object
        """
        conversation = self.get_conversation(user_id)
        message = conversation.add_message(role, content, metadata)

        # Save to storage
        self._save_conversation(conversation)

        return message

    def get_conversation_history(self, user_id: str, limit: Optional[int] = None) -> List[Dict[str, Any]]:
        """
        Get conversation history for a user.

        Args:
            user_id: The user's identifier
            limit: Maximum number of messages to return (optional)

        Returns:
            List[Dict[str, Any]]: List of message dictionaries
        """
        conversation = self.get_conversation(user_id)
        messages = conversation.get_recent_messages(limit) if limit else conversation.messages

        return [msg.to_dict() for msg in messages]

    def clear_conversation(self, user_id: str) -> bool:
        """
        Clear a user's conversation history.

        Args:
            user_id: The user's identifier

        Returns:
            bool: True if conversation was cleared successfully
        """
        if user_id in self.conversations:
            conversation = self.conversations[user_id]
            conversation.clear_messages()

            # Remove from storage
            file_path = self._get_conversation_file(user_id)
            if file_path.exists():
                try:
                    file_path.unlink()
                except Exception as e:
                    print(f"Warning: Failed to delete conversation file: {e}")

            return True

        return False

    def get_conversation_summary(self, user_id: str) -> Dict[str, Any]:
        """
        Get a summary of a user's conversation.

        Args:
            user_id: The user's identifier

        Returns:
            Dict[str, Any]: Conversation summary information
        """
        conversation = self.get_conversation(user_id)

        return {
            'user_id': conversation.user_id,
            'message_count': len(conversation.messages),
            'created_at': conversation.created_at.isoformat(),
            'updated_at': conversation.updated_at.isoformat(),
            'has_active_conversation': len(conversation.messages) > 0
        }

    def list_all_users(self) -> List[str]:
        """
        List all users with conversations.

        Returns:
            List[str]: List of user IDs
        """
        return list(self.conversations.keys())

    def cleanup_old_conversations(self, days: int = 30) -> int:
        """
        Clean up conversations older than specified days.

        Args:
            days: Number of days to keep conversations

        Returns:
            int: Number of conversations cleaned up
        """
        from datetime import timedelta
        cutoff_date = datetime.now() - timedelta(days=days)
        removed_count = 0

        users_to_remove = []
        for user_id, conversation in self.conversations.items():
            if conversation.updated_at < cutoff_date:
                users_to_remove.append(user_id)

        for user_id in users_to_remove:
            self.clear_conversation(user_id)
            removed_count += 1

        return removed_count


# Global memory service instance
_memory_service = None


def get_memory_service() -> MemoryService:
    """Get the global memory service instance."""
    global _memory_service
    if _memory_service is None:
        # Use environment variable for storage path or default
        storage_path = os.getenv('CONVERSATION_STORAGE_PATH')
        _memory_service = MemoryService(storage_path)
    return _memory_service