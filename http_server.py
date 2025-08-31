#!/usr/bin/env python3
"""
Lily-Core HTTP Server - REST API for Chat Functionality

Provides HTTP endpoints to interact with the Lily chatbot via HTTP requests.
"""

import asyncio
import os
from typing import Dict, List, Optional, Any
from contextlib import asynccontextmanager
from fastapi import FastAPI, HTTPException, Query
from fastapi.middleware.cors import CORSMiddleware
from pydantic import BaseModel
from dotenv import load_dotenv
from datetime import datetime, timezone

from services.chat_service import get_chat_service, ChatService


class ChatMessage(BaseModel):
    """Chat message model."""
    message: str
    user_id: Optional[str] = "default_user"


class ChatResponse(BaseModel):
    """Chat response model."""
    response: str
    user_id: str
    timestamp: str


class ConversationHistory(BaseModel):
    """Conversation history model."""
    history: List[Dict[str, Any]]
    user_id: str


# Global chat service instance
chatbot_instance: Optional[ChatService] = None


@asynccontextmanager
async def lifespan(app: FastAPI):
    """Application lifespan manager."""
    global chatbot_instance

    # Startup
    print("🌸 Starting Lily-Core HTTP Server")
    try:
        chatbot_instance = get_chat_service()
        success = await chatbot_instance.initialize()
        if not success:
            raise Exception("Failed to initialize chat service")
        print("✅ Chat service initialized successfully")
    except Exception as e:
        print(f"❌ Failed to start server: {e}")
        raise

    yield

    # Shutdown
    if chatbot_instance:
        await chatbot_instance.cleanup()
        print("✅ Chat service cleaned up")


# Create FastAPI app
app = FastAPI(
    title="Lily-Core Chat Service API",
    description="AI-powered chat service with web search capabilities and Agent Loop Architecture",
    version="1.0.0",
    lifespan=lifespan
)

# Add CORS middleware
app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],  # In production, specify allowed origins
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)


@app.get("/health")
async def health_check():
    """Health check endpoint."""
    return {
        "status": "healthy",
        "service": "Lily-Core Chat Service",
        "chatbot_ready": chatbot_instance is not None
    }


@app.post("/chat", response_model=ChatResponse)
async def chat(message: ChatMessage):
    """
    Send a message to the chat service and get a response.

    The chat service uses the advanced agent loop system for complex reasoning.
    """
    if not chatbot_instance:
        raise HTTPException(status_code=503, detail="Chat service not initialized")

    try:
        # Process the chat message using agent loop
        result = await chatbot_instance.chat_with_agent_loop(
            message=message.message,
            user_id=message.user_id
        )

        # Create response
        response = ChatResponse(
            response=result['response'],
            user_id=result['user_id'],
            timestamp=result['timestamp']
        )

        return response

    except Exception as e:
        raise HTTPException(status_code=500, detail=f"Chat error: {str(e)}")


@app.get("/conversation/{user_id}")
async def get_conversation_history(user_id: str):
    """Get conversation history for a user."""
    if not chatbot_instance:
        raise HTTPException(status_code=503, detail="Chat service not initialized")

    try:
        history = chatbot_instance.get_conversation_history(user_id)

        return ConversationHistory(
            history=history,
            user_id=user_id
        )

    except Exception as e:
        raise HTTPException(status_code=500, detail=f"Failed to get conversation history: {str(e)}")


@app.delete("/conversation/{user_id}")
async def clear_conversation(user_id: str):
    """Clear conversation history for a user."""
    if not chatbot_instance:
        raise HTTPException(status_code=503, detail="Chat service not initialized")

    try:
        success = await chatbot_instance.clear_conversation(user_id)
        if success:
            return {"message": f"Conversation cleared for user {user_id}"}
        else:
            return {"message": f"No conversation found for user {user_id}"}

    except Exception as e:
        raise HTTPException(status_code=500, detail=f"Failed to clear conversation: {str(e)}")


@app.get("/tools")
async def get_available_tools():
    """Get information about available tools."""
    if not chatbot_instance:
        raise HTTPException(status_code=503, detail="Chat service not initialized")

    try:
        tools = chatbot_instance.get_available_tools()
        return {
            "tools": tools,
            "count": len(tools),
            "description": "Tools automatically used by the chat service when appropriate"
        }

    except Exception as e:
        raise HTTPException(status_code=500, detail=f"Failed to get tool information: {str(e)}")


@app.get("/")
async def root():
    """Root endpoint with API information."""
    return {
        "message": "Welcome to Lily-Core Chat Service API 🌸",
        "version": "1.0.0",
        "endpoints": {
            "GET /health": "Health check",
            "POST /chat": "Send chat message",
            "GET /conversation/{user_id}": "Get conversation history",
            "DELETE /conversation/{user_id}": "Clear conversation history",
            "GET /tools": "Get available tools information"
        },
        "features": [
            "Advanced Agent Loop Architecture for multi-step reasoning",
            "Intelligent tool usage (decides when to search web)",
            "Conversation memory",
            "Automatic search mode selection (summary/detailed)",
            "Gemini AI powered responses"
        ]
    }


if __name__ == "__main__":
    import uvicorn

    # Load environment variables
    load_dotenv()

    # Get server configuration
    host = os.getenv("HOST", "0.0.0.0")
    port = int(os.getenv("PORT", "8000"))

    print(f"🌸 Starting Lily-Core HTTP Server on {host}:{port}")

    uvicorn.run(
        "http_server:app",
        host=host,
        port=port,
        reload=False
    )