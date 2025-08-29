# Lily-Core Package
#
# Clean Architecture Implementation for Lily ChatBot
#
# Directory Structure:
# - core/          : Business entities and core configuration
# - models/        : Data models and domain entities
# - services/      : Use cases and business logic
# - api/           : Interface adapters (HTTP REST API)
# - utils/         : Shared utilities and helpers
# - tests/         : Unit and integration tests

__version__ = "1.0.0"

# Core components
from .core.config import config, chat_settings

# Services
from .services.chat_service import get_chat_service
from .services.memory_service import get_memory_service
from .services.tool_service import get_tool_service

# Models
from .models.message import Message, Conversation
from .models.api import ChatRequest, ChatResponse

# Utils
from .utils.text_utils import clean_text, truncate_text

__all__ = [
    "config",
    "chat_settings",
    "get_chat_service",
    "get_memory_service",
    "get_tool_service",
    "Message",
    "Conversation",
    "ChatRequest",
    "ChatResponse",
    "clean_text",
    "truncate_text"
]