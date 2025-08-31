#!/usr/bin/env python3
"""
Lily-Core HTTP Server - Clean Architecture Entry Point

Main entry point for the Lily chatbot HTTP server.
Implements clean architecture with separation of concerns.
"""

import asyncio
import os
import sys
import uvicorn
from contextlib import asynccontextmanager
from fastapi import FastAPI, WebSocket, HTTPException
from fastapi.middleware.cors import CORSMiddleware
from dotenv import load_dotenv

from core.config import config
from api.routes import router
from services.chat_service import get_chat_service
from services.websocket_manager import get_websocket_manager

# Global service instances
chat_service = get_chat_service()
websocket_manager = get_websocket_manager()


@asynccontextmanager
async def lifespan(app: FastAPI):
    """Application lifespan manager for startup and shutdown."""
    print("🌸 Starting Lily-Core HTTP Server - Clean Architecture Edition")

    try:
        # Startup: Initialize core services
        success = await chat_service.initialize()
        if not success:
            print("⚠️  Chat service initialization failed - continuing with reduced functionality")
            print("⚠️  Web search and tool capabilities may not be available")
            # Don't raise exception - continue with degraded service
        else:
            print("✅ Chat service initialized successfully")

        # Initialize WebSocket manager
        await websocket_manager.initialize()
        print("✅ WebSocket manager initialized successfully")

        print(f"🚀 Starting HTTP server on {config.HOST}:{config.PORT}")
        print("💡 System will continue operating even if MCP servers are unavailable")

        yield

    except Exception as e:
        print(f"❌ Failed to start server: {e}")
        raise

    finally:
        # Shutdown: Clean up resources
        print("🧹 Cleaning up resources...")
        await chat_service.cleanup()
        await websocket_manager.cleanup()
        print("✅ Server shut down cleanly")


def create_app() -> FastAPI:
    """Create and configure the FastAPI application."""
    app = FastAPI(
        title="Lily-Core Chat Service API",
        description="AI-powered chat service with web search capabilities using Clean Architecture and Agent Loop",
        version="1.0.0",
        lifespan=lifespan
    )

    # Add CORS middleware for web clients
    app.add_middleware(
        CORSMiddleware,
        allow_origins=["*"],  # In production, specify allowed origins
        allow_credentials=True,
        allow_methods=["*"],
        allow_headers=["*"],
    )

    # Include API routes
    app.include_router(router, prefix="", tags=["chat"])

    return app


# Create the FastAPI application
app = create_app()


@app.websocket("/ws/tts/{client_id}")
async def tts_websocket_endpoint(websocket: WebSocket, client_id: str):
    """WebSocket endpoint for TTS connections."""
    await websocket_manager.handle_client_connection(websocket, client_id)


def main():
    """Main entry point for Lily-Core HTTP Server."""
    # Load environment variables
    load_dotenv()

    print("🌸 Lily-Core - AI-Powered Chat Service 🌸")
    print("=" * 50)

    # Check if API key is configured
    if not config.GEMINI_API_KEY:
        print("❌ Error: GEMINI_API_KEY not configured!")
        print("Please set your Gemini API key in the .env file.")
        print("You can get your API key from: https://makersuite.google.com/app/apikey")
        return 1

    print("✅ Gemini API key configured")

    # Show web-scout configuration status
    if config.WEB_SCOUT_URL:
        print(f"✅ Web-Scout URL: {config.WEB_SCOUT_URL}")
        print("🔄 MCP Protocol: ENABLED (Web-Scout as remote MCP server)")
    else:
        print("⚠️  Warning: WEB_SCOUT_URL not configured")
        print("   Using default: http://web-scout:8001")
        print("   Web search functionality may not be available")

    # Show debug status
    if config.DEBUG:
        print("🐛 Debug mode: ENABLED")
    else:
        print("🔒 Debug mode: DISABLED")

    print("=" * 50)
    print("📚 Available endpoints:")
    print("   GET  /         - API information")
    print("   GET  /health   - Health check")
    print("   POST /chat     - Send chat message (JSON)")
    print("   GET  /conversation/{user_id} - Get conversation history")
    print("   DELETE /conversation/{user_id} - Clear conversation")
    print("   GET  /tools    - Get available tools")
    print("   GET  /conversation-summary/{user_id} - Get conversation summary")
    print("   WS   /ws/tts/{client_id} - TTS WebSocket endpoint")
    print("   - Supports settings.update and tts.request messages")
    print("=" * 50)
    print("💡 The chatbot will automatically decide when to use web search")
    print("🌐 Send POST requests to /chat with: {'message': 'your message'}")

    try:
        # Start the HTTP server
        uvicorn.run(
            "main:app",
            host=config.HOST,
            port=config.PORT,
            reload=config.DEBUG,
            log_level="info" if not config.DEBUG else "debug"
        )
    except KeyboardInterrupt:
        print("\n\n🌸 Server stopped by user")
    except Exception as e:
        print(f"❌ Fatal server error: {e}")
        return 1

    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except Exception as e:
        print(f"❌ Fatal error: {e}")
        sys.exit(1)