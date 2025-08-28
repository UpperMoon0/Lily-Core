#!/usr/bin/env python3
"""
Test script to verify Lily-Core can initialize without Web-Scout
"""

import asyncio
import os
import sys
from dotenv import load_dotenv

# Load environment variables
load_dotenv()

# Set fake Gemini API key for testing (real key would be required for actual chat)
os.environ.setdefault('GEMINI_API_KEY', 'test_key')

# Import chatbot after setting environment
from chatbot import get_chatbot, ChatBot

async def test_initialization():
    """Test that Lily-Core can initialize without Web-Scout."""
    print("🧪 Testing Lily-Core initialization without Web-Scout...")

    try:
        # Get chatbot instance
        chatbot = get_chatbot()

        # Try to initialize
        print("📦 Initializing chatbot...")
        success = await chatbot.initialize()

        if success:
            print("✅ Success! Lily-Core initialized without Web-Scout")
            print(f"   Available tools: {len(chatbot.available_tools)}")
            print(f"   Web search tool available: {chatbot.web_search_tool is not None}")

            # Test basic chat functionality
            print("   Testing basic chat...")
            response = await chatbot.chat("Hello! How are you?")
            print(f"   Chat response: {response[:100]}...")

            return True
        else:
            print("❌ Failed to initialize chatbot")
            return False

    except Exception as e:
        print(f"❌ Error during initialization test: {e}")
        return False

if __name__ == "__main__":
    result = asyncio.run(test_initialization())
    sys.exit(0 if result else 1)