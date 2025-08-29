#!/usr/bin/env python3
"""
Lily-Core Configuration Module

Centralized configuration for the Lily chatbot system.
Contains business configuration, AI model setup, and core settings.
"""

import os
from dotenv import load_dotenv
import google.generativeai as genai

# Load environment variables
load_dotenv()

# Core Configuration
class Config:
    """Application configuration settings."""

    # Environment
    DEBUG: bool = os.getenv('DEBUG', 'false').lower() == 'true'
    HOST: str = os.getenv('HOST', '0.0.0.0')
    PORT: int = int(os.getenv('PORT', '8000'))

    # AI Model Configuration
    GEMINI_API_KEY: str = os.getenv('GEMINI_API_KEY')
    GEMINI_MODEL_NAME: str = os.getenv('GEMINI_MODEL_NAME', 'gemini-2.5-flash')
    TEMPERATURE: float = float(os.getenv('TEMPERATURE', '0.7'))
    TOP_P: float = float(os.getenv('TOP_P', '0.8'))
    MAX_OUTPUT_TOKENS: int = int(os.getenv('MAX_OUTPUT_TOKENS', '1000'))

        # Web-Scout Integration
    WEB_SCOUT_URL: str = os.getenv('WEB_SCOUT_URL', 'http://web-scout:8000')

    def __init__(self):
        """Validate configuration on initialization."""
        if not self.GEMINI_API_KEY:
            raise ValueError("GEMINI_API_KEY not found in environment variables")


# Global configuration instance
config = Config()

# Configure Gemini AI
genai.configure(api_key=config.GEMINI_API_KEY)

# Create Gemini model instance
model = genai.GenerativeModel(
    model_name=config.GEMINI_MODEL_NAME,
    generation_config={
        'temperature': config.TEMPERATURE,
        'top_p': config.TOP_P,
        'max_output_tokens': config.MAX_OUTPUT_TOKENS,
    }
)

# Business Entity: Chat Configuration
class ChatSettings:
    """Business entity for chat behavior settings."""

    def __init__(self):
        self.max_conversation_length: int = 50  # Maximum messages to keep in memory
        self.enable_tool_usage: bool = True  # Whether to use tools like web search
        self.auto_tool_detection: bool = True  # Whether to automatically detect when to use tools
        self.conversation_context_window: int = 10  # Number of recent messages for context
        self.search_keywords = [
            'search', 'find', 'lookup', 'research', 'what is', 'how to',
            'tell me about', 'what are', 'where can', 'latest', 'news',
            'current', 'price of', 'weather', 'recipe for'
        ]


# Global chat settings instance
chat_settings = ChatSettings()