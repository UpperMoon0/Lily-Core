#!/usr/bin/env python3
"""
Lily-Core Main Application.

A simple application that demonstrates web search functionality through MCP.
"""

import asyncio
import sys
from dotenv import load_dotenv
import os
from mcp_client import sync_web_search


def main():
    """Main entry point for Lily-Core."""
    # Load environment variables
    load_dotenv()

    print("🌸 Lily-Core - AI-Powered Web Search 🌸")
    print("=" * 50)

    # Check if API key is configured
    api_key = os.getenv('GEMINI_API_KEY')
    if not api_key or api_key == 'your_gemini_api_key_here':
        print("❌ Error: GEMINI_API_KEY not configured!")
        print("Please set your Gemini API key in the .env file.")
        print("You can get your API key from: https://makersuite.google.com/app/apikey")
        return 1

    print("✅ Gemini API key configured")

    # Interactive search mode
    if len(sys.argv) == 1:
        print("\n🔍 Interactive Web Search Mode")
        print("Enter search queries (or 'quit' to exit):")

        while True:
            try:
                query = input("\nSearch: ").strip()
                if query.lower() in ['quit', 'exit', 'q']:
                    print("Goodbye! 🌸")
                    break

                if not query:
                    continue

                mode = input("Mode (summary/detailed) [summary]: ").strip().lower()
                if not mode:
                    mode = "summary"

                if mode not in ["summary", "detailed"]:
                    print("❌ Invalid mode. Use 'summary' or 'detailed'")
                    continue

                print(f"\n🔍 Searching for: '{query}'...")
                print(f"📝 Mode: {mode}")
                print("-" * 50)

                result = sync_web_search(query, mode)

                if 'error' in result:
                    print(f"❌ Search failed: {result['error']}")
                else:
                    print("✅ Search Results:")
                    print(f"Query: {result.get('query', 'N/A')}")
                    print(f"Mode: {result.get('mode', 'N/A')}")
                    print(f"Sources: {result.get('sources_used', 0)}")
                    print("\n" + "=" * 50)
                    print("📄 Summary:")
                    print(result.get('summary', 'No summary available'))
                    print("=" * 50)

            except KeyboardInterrupt:
                print("\n\nGoodbye! 🌸")
                break
            except Exception as e:
                print(f"❌ Unexpected error: {e}")

    # Command line search mode
    elif len(sys.argv) >= 2:
        query = sys.argv[1]
        mode = sys.argv[2] if len(sys.argv) > 2 else "summary"

        if mode not in ["summary", "detailed"]:
            mode = "summary"

        print(f"🔍 Searching for: '{query}' (mode: {mode})...")
        print("-" * 50)

        result = sync_web_search(query, mode)

        if 'error' in result:
            print(f"❌ Search failed: {result['error']}")
            return 1
        else:
            print("✅ Search Results:")
            print(f"Query: {result.get('query', 'N/A')}")
            print(f"Mode: {result.get('mode', 'N/A')}")
            print(f"Sources: {result.get('sources_used', 0)}")
            print("\n" + "=" * 50)
            print("📄 Summary:")
            print(result.get('summary', 'No summary available'))
            print("=" * 50)

    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except Exception as e:
        print(f"❌ Fatal error: {e}")
        sys.exit(1)