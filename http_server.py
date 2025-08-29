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

from chatbot import get_chatbot, ChatBot


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


# Global chatbot instance
chatbot_instance: Optional[ChatBot] = None


@asynccontextmanager
async def lifespan(app: FastAPI):
    """Application lifespan manager."""
    global chatbot_instance

    # Startup
    print("🌸 Starting Lily-Core HTTP Server")
    try:
        chatbot_instance = get_chatbot()
        success = await chatbot_instance.initialize()
        if not success:
            raise Exception("Failed to initialize chatbot")
        print("✅ ChatBot initialized successfully")
    except Exception as e:
        print(f"❌ Failed to start server: {e}")
        raise

    yield

    # Shutdown
    if chatbot_instance:
        await chatbot_instance.cleanup()
        print("✅ ChatBot cleaned up")


# Create FastAPI app
app = FastAPI(
    title="Lily-Core ChatBot API",
    description="AI-powered chatbot with web search capabilities",
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
        "service": "Lily-Core ChatBot",
        "chatbot_ready": chatbot_instance is not None
    }


@app.post("/chat", response_model=ChatResponse)
async def chat(message: ChatMessage):
    """
    Send a message to the chatbot and get a response.

    The chatbot will intelligently decide whether to use web search tools
    based on the context and content of your message.
    """
    if not chatbot_instance:
        raise HTTPException(status_code=503, detail="ChatBot not initialized")

    try:
        # Process the chat message
        response_text = await chatbot_instance.chat(message.message)

        # Create response
        response = ChatResponse(
            response=response_text,
            user_id=message.user_id,
            timestamp=datetime.now(timezone.utc).isoformat()
        )

        return response

    except Exception as e:
        raise HTTPException(status_code=500, detail=f"Chat error: {str(e)}")


@app.get("/conversation/{user_id}")
async def get_conversation_history(user_id: str):
    """Get conversation history for a user."""
    if not chatbot_instance:
        raise HTTPException(status_code=503, detail="ChatBot not initialized")

    try:
        # For now, return the global conversation history
        # In a multi-user setup, you'd store separate histories per user
        history = chatbot_instance.get_conversation_history()

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
        raise HTTPException(status_code=503, detail="ChatBot not initialized")

    try:
        await chatbot_instance.clear_conversation()
        return {"message": f"Conversation cleared for user {user_id}"}

    except Exception as e:
        raise HTTPException(status_code=500, detail=f"Failed to clear conversation: {str(e)}")


@app.get("/tools")
async def get_available_tools():
    """Get information about available tools."""
    if not chatbot_instance:
        raise HTTPException(status_code=503, detail="ChatBot not initialized")

    try:
        return {
            "tools": chatbot_instance.available_tools,
            "description": "Tools automatically used by the chatbot when appropriate"
        }

    except Exception as e:
        raise HTTPException(status_code=500, detail=f"Failed to get tool information: {str(e)}")


@app.get("/")
async def root():
    """Root endpoint with API information."""
    return {
        "message": "Welcome to Lily-Core ChatBot API 🌸",
        "version": "1.0.0",
        "endpoints": {
            "GET /health": "Health check",
            "POST /chat": "Send chat message",
            "GET /conversation/{user_id}": "Get conversation history",
            "DELETE /conversation/{user_id}": "Clear conversation history",
            "GET /tools": "Get available tools information"
        },
        "features": [
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