#!/usr/bin/env python3
"""
Lily-Core HTTP Server

Main entry point for the Lily chatbot HTTP server.
Provides AI-powered chat with intelligent web search integration.
"""

import os
import sys
import uvicorn
from dotenv import load_dotenv


def main():
    """Main entry point for Lily-Core HTTP Server."""
    # Load environment variables
    load_dotenv()

    print("🌸 Lily-Core - AI-Powered ChatBot 🌸")
    print("=" * 50)

    # Check if API key is configured
    api_key = os.getenv('GEMINI_API_KEY')
    if not api_key or api_key == 'your_gemini_api_key_here':
        print("❌ Error: GEMINI_API_KEY not configured!")
        print("Please set your Gemini API key in the .env file.")
        print("You can get your API key from: https://makersuite.google.com/app/apikey")
        return 1

    print("✅ Gemini API key configured")

    # Check Web-Scout configuration
    web_scout_url = os.getenv('WEB_SCOUT_URL')
    if not web_scout_url:
        print("⚠️  Warning: WEB_SCOUT_URL not configured")
        print("   Using default: http://web-scout:8000")
    else:
        print(f"✅ Web-Scout URL: {web_scout_url}")

    # Get server configuration
    host = os.getenv('HOST', '0.0.0.0')
    port = int(os.getenv('PORT', '8000'))

    print(f"🚀 Starting HTTP server on {host}:{port}")
    print("=" * 50)
    print("📚 Available endpoints:")
    print("   GET  /         - API information")
    print("   GET  /health   - Health check")
    print("   POST /chat     - Send chat message (JSON)")
    print("   GET  /conversation/{user_id} - Get conversation history")
    print("   DELETE /conversation/{user_id} - Clear conversation")
    print("   GET  /tools    - Get available tools")
    print("=" * 50)
    print("💡 The chatbot will automatically decide when to use web search")
    print("🌐 Send POST requests to /chat with: {'message': 'your message'}")

    try:
        # Start the HTTP server
        uvicorn.run(
            "http_server:app",
            host=host,
            port=port,
            reload=False,
            log_level="info"
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