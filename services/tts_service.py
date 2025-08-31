#!/usr/bin/env python3
"""
TTS Service for Lily-Core

Service for managing Text-to-Speech functionality via MCP-over-HTTP protocol.
Connects to TTS-Provider service and handles audio generation with WebSocket streaming.
"""

import asyncio
import json
import logging
import time
from typing import Dict, List, Optional, Any
import requests
from requests.adapters import HTTPAdapter
from urllib3.util import Retry

from models.api import (
    TTSServiceRequest, TTSServiceResponse, TTSSettings,
    AudioStreamMetadata, TTSRequest, TTSResponse, WebSocketError
)


class TTSProviderClient:
    """MCP-over-HTTP client for TTS-Provider service."""

    def __init__(self, tts_provider_url: str = "http://tts-provider:9000"):
        """Initialize TTS provider client with MCP-over-HTTP support."""
        self.tts_provider_url = tts_provider_url
        self.mcp_url = f"{self.tts_provider_url}/mcp"
        self.request_id = 0

        # Configure session with retry logic
        self.session = requests.Session()

        # Configure retry strategy
        retry_strategy = Retry(
            total=3,
            backoff_factor=1,
            status_forcelist=[429, 500, 502, 503, 504],
            method_whitelist=["HEAD", "GET", "OPTIONS", "POST"]
        )

        adapter = HTTPAdapter(max_retries=retry_strategy)
        self.session.mount("http://", adapter)
        self.session.mount("https://", adapter)

        self.logger = logging.getLogger("TTSService.MCPClient")

    def _get_next_id(self) -> str:
        """Get next request ID for JSON-RPC."""
        self.request_id += 1
        return str(self.request_id)

    async def _send_mcp_request(self, method: str, params: Optional[Dict[str, Any]] = None) -> Dict[str, Any]:
        """Send MCP JSON-RPC request over HTTP."""
        try:
            request_payload = {
                'jsonrpc': '2.0',
                'id': self._get_next_id(),
                'method': method
            }

            if params:
                request_payload['params'] = params

            self.logger.debug(f"Sending MCP request to {self.mcp_url}: {request_payload}")

            response = self.session.post(
                self.mcp_url,
                json=request_payload,
                timeout=30.0
            )

            if response.status_code == 200:
                return response.json()
            else:
                return {
                    'error': {
                        'code': response.status_code,
                        'message': f'HTTP {response.status_code}: {response.text}'
                    }
                }

        except requests.exceptions.Timeout:
            return {
                'error': {
                    'code': -32000,
                    'message': 'Request timeout - TTS provider is not responding'
                }
            }
        except requests.exceptions.ConnectionError:
            return {
                'error': {
                    'code': -32001,
                    'message': 'Connection failed - TTS provider is not accessible'
                }
            }
        except Exception as e:
            return {
                'error': {
                    'code': -32000,
                    'message': f'MCP request failed: {str(e)}'
                }
            }

    async def generate_speech_with_stream(self, request: TTSServiceRequest) -> Dict[str, Any]:
        """Generate speech using TTS provider with MCP protocol and streaming support."""
        try:
            # List available tools first
            tools_response = await self._send_mcp_request('tools/list')

            if tools_response.get('error'):
                return {
                    "status": "error",
                    "message": f"Failed to list TTS provider tools: {tools_response['error']['message']}"
                }

            # Check if TTS tool is available
            tools = tools_response.get('result', {}).get('tools', [])
            tts_tool = None
            for tool in tools:
                if tool.get('name') in ['tts_generate', 'generate_speech', 'tts']:
                    tts_tool = tool
                    break

            if not tts_tool:
                return {
                    "status": "error",
                    "message": "TTS generation tool not available on provider"
                }

            # Prepare TTS parameters
            tts_params = {
                'text': request.text,
                'speaker': request.speaker,
                'sample_rate': request.sample_rate,
                'model': request.model,
                'lang': request.lang,
                'stream': True  # Enable streaming mode
            }

            # Call the TTS tool
            call_response = await self._send_mcp_request('tools/call', {
                'name': 'tts_generate',
                'arguments': tts_params
            })

            if call_response.get('error'):
                return {
                    "status": "error",
                    "message": f"TTS generation failed: {call_response['error']['message']}"
                }

            # Process the result
            result = call_response.get('result', {})
            if result.get('content'):
                # Extract audio data if available
                content = result['content'][0] if isinstance(result['content'], list) else result['content']
                if content.get('type') == 'audio':
                    binary_data = content.get('data', b'')
                else:
                    # Assume text data contains base64 encoded audio
                    import base64
                    if content.get('text'):
                        try:
                            binary_data = base64.b64decode(content['text'])
                        except:
                            binary_data = b''
                    else:
                        binary_data = b''
            else:
                binary_data = b''

            return {
                "status": "success",
                "message": "Audio generated successfully",
                "audio_data": binary_data,
                "metadata": {
                    "sample_rate": request.sample_rate,
                    "channels": 1,
                    "bit_depth": 16,
                    "length_bytes": len(binary_data),
                    "format": "wav"
                }
            }

        except Exception as e:
            self.logger.error(f"Error in TTS generation: {str(e)}")
            return {
                "status": "error",
                "message": f"Unexpected error: {str(e)}"
            }

    async def get_provider_info(self) -> Dict[str, Any]:
        """Get information about the TTS provider service using MCP."""
        try:
            # Try MCP info endpoint first
            info_response = await self._send_mcp_request('info')

            if not info_response.get('error'):
                return info_response.get('result', {})

            # Fallback to direct HTTP health check
            response = self.session.get(f"{self.tts_provider_url}/health", timeout=5.0)
            if response.status_code == 200:
                return {"status": "healthy", "protocol": "MCP-over-HTTP"}
            else:
                return {"error": f"HTTP {response.status_code}"}

        except Exception as e:
            return {"error": f"Provider info request failed: {str(e)}"}

    async def close(self):
        """Close the HTTP session."""
        if self.session:
            self.session.close()
            self.logger.info("TTS MCP client connection closed")


class TTSService:
    """Service for Text-to-Speech functionality management with WebSocket streaming."""

    def __init__(self):
        """Initialize TTS service."""
        self.provider_client: Optional[TTSProviderClient] = None
        self._initialized = False
        self.logger = logging.getLogger("TTSService")

        # In-memory storage for TTS settings per client connection
        self.client_settings: Dict[str, TTSSettings] = {}
        self.active_streams: Dict[str, Any] = {}  # Track active audio streams

    async def initialize(self) -> bool:
        """
        Initialize the TTS service and connect to TTS provider.

        Returns:
            bool: True if initialization was successful
        """
        try:
            # Initialize TTS provider client
            self.provider_client = TTSProviderClient()

            # Test connection to TTS provider
            provider_info = await self.provider_client.get_provider_info()

            if "error" not in provider_info:
                self.logger.info("Connected to TTS provider successfully")
                self.logger.info(f"Provider info: {provider_info}")

                self._initialized = True
                print("✅ TTS service initialized successfully")
                return True
            else:
                self.logger.warning(f"TTS provider connection test failed: {provider_info['error']}")
                # Continue with degraded functionality
                self._initialized = True
                print("⚠️  TTS provider not available - service will operate in degraded mode")
                return True

        except Exception as e:
            self.logger.error(f"Failed to initialize TTS service: {str(e)}")
            self._initialized = True  # Allow degraded operation
            print("⚠️  TTS service initialization failed - continuing with reduced functionality")
            return True

    def store_client_settings(self, client_id: str, settings: TTSSettings) -> None:
        """
        Store TTS settings for a specific client connection.

        Args:
            client_id: Unique client identifier
            settings: TTS settings to store
        """
        self.client_settings[client_id] = settings
        self.logger.debug(f"Stored TTS settings for client {client_id}: {settings}")

    def get_client_settings(self, client_id: str) -> TTSSettings:
        """
        Get TTS settings for a specific client connection.

        Args:
            client_id: Unique client identifier

        Returns:
            TTSSettings: Client's TTS settings or default settings
        """
        return self.client_settings.get(client_id, TTSSettings())

    def remove_client_settings(self, client_id: str) -> None:
        """
        Remove TTS settings for a client connection.

        Args:
            client_id: Unique client identifier
        """
        if client_id in self.client_settings:
            del self.client_settings[client_id]
            self.logger.debug(f"Removed TTS settings for client {client_id}")

    async def generate_speech_from_request(self, request_data: Dict[str, Any], client_id: str) -> TTSServiceResponse:
        """
        Generate speech from a TTS request, using stored client settings.

        Args:
            request_data: TTS service request data
            client_id: Client identifier to retrieve stored settings

        Returns:
            TTSServiceResponse: Response containing audio data or error
        """
        try:
            # Get stored settings for this client
            stored_settings = self.get_client_settings(client_id)

            # Extract text from request data
            if isinstance(request_data, dict):
                request_text = request_data.get('text', '')
            else:
                request_text = getattr(request_data, 'text', '')

            if not request_text:
                return TTSServiceResponse(
                    status="error",
                    message="No text provided for speech generation"
                )

            # Merge request with stored settings (request takes precedence)
            final_request = TTSServiceRequest(
                text=request_text,
                speaker=request_data.get('speaker', stored_settings.speaker),
                sample_rate=request_data.get('sample_rate', stored_settings.sample_rate),
                model=request_data.get('model', stored_settings.model),
                lang=request_data.get('lang', stored_settings.lang)
            )

            # Generate speech using TTS provider
            if self.provider_client and self._initialized:
                result = await self.provider_client.generate_speech_with_stream(final_request)

                if result['status'] == 'success':
                    return TTSServiceResponse(
                        status="success",
                        message="Audio generated successfully",
                        audio_data=result['audio_data'],
                        metadata=result['metadata']
                    )
                else:
                    return TTSServiceResponse(
                        status="error",
                        message=result['message']
                    )
            else:
                return TTSServiceResponse(
                    status="error",
                    message="TTS provider not available - service is in degraded mode"
                )

        except Exception as e:
            self.logger.error(f"Error in generate_speech_from_request: {str(e)}")
            return TTSServiceResponse(
                status="error",
                message=f"Failed to generate speech: {str(e)}"
            )

    async def update_client_settings(self, client_id: str, settings_update: Dict[str, Any]) -> bool:
        """
        Update TTS settings for a client.

        Args:
            client_id: Client identifier
            settings_update: Settings update data

        Returns:
            bool: True if update was successful
        """
        try:
            current_settings = self.get_client_settings(client_id)

            # Update settings
            for key, value in settings_update.items():
                if hasattr(current_settings, key):
                    setattr(current_settings, key, value)

            self.store_client_settings(client_id, current_settings)
            self.logger.info(f"Updated settings for client {client_id}")
            return True

        except Exception as e:
            self.logger.error(f"Failed to update client settings: {str(e)}")
            return False

    async def stream_audio_to_websocket(self, websocket, audio_response: TTSServiceResponse, client_id: str):
        """
        Stream audio data through WebSocket connection.

        Args:
            websocket: WebSocket connection to send audio chunks
            audio_response: TTS response containing audio data
            client_id: Client identifier for tracking
        """
        stream_id = f"{client_id}_{int(time.time())}"
        self.active_streams[stream_id] = True

        try:
            if audio_response.status != "success" or not audio_response.audio_data:
                # Send error response
                error_msg = TTSResponse(
                    type="tts.response",
                    payload=TTSResponse.__annotations__['payload'](
                        status="error",
                        message=audio_response.message or "No audio data available",
                        sample_rate=audio_response.metadata.get("sample_rate", 24000) if audio_response.metadata else None,
                        channels=audio_response.metadata.get("channels", 1) if audio_response.metadata else None,
                        bit_depth=audio_response.metadata.get("bit_depth", 16) if audio_response.metadata else None
                    )
                )
                await websocket.send(json.dumps(error_msg.dict()))
                return

            # Send success response with metadata
            success_msg = TTSResponse(
                type="tts.response",
                payload=TTSResponse.__annotations__['payload'](
                    status="success",
                    message="Audio generation started",
                    sample_rate=audio_response.metadata.get("sample_rate", 24000),
                    channels=audio_response.metadata.get("channels", 1),
                    bit_depth=audio_response.metadata.get("bit_depth", 16)
                )
            )
            await websocket.send(json.dumps(success_msg.dict()))

            # Small delay before streaming audio
            await asyncio.sleep(0.1)

            # Stream audio data in chunks
            audio_data = audio_response.audio_data
            chunk_size = 4096  # 4KB chunks

            for i in range(0, len(audio_data), chunk_size):
                if not self.active_streams.get(stream_id, False):
                    break  # Stream was cancelled

                chunk = audio_data[i:i + chunk_size]

                # Send audio chunk as binary data
                await websocket.send(chunk)

                # Small delay between chunks to prevent flooding
                await asyncio.sleep(0.01)

            # Send stream end message
            from models.api import AudioStreamEnd
            end_msg = AudioStreamEnd(type="audio.stream.end")
            await websocket.send(json.dumps(end_msg.dict()))

        except Exception as e:
            self.logger.error(f"Error streaming audio to WebSocket: {str(e)}")
            try:
                # Try to send error message
                error_msg = WebSocketError(
                    type="error",
                    payload=WebSocketError.__annotations__['payload'](
                        code=500,
                        message=f"Streaming error: {str(e)}"
                    )
                )
                await websocket.send(json.dumps(error_msg.dict()))
            except:
                # WebSocket might be closed
                pass

        finally:
            # Clean up active stream tracking
            if stream_id in self.active_streams:
                del self.active_streams[stream_id]

    async def cancel_stream(self, client_id: str, timestamp: Optional[int] = None) -> bool:
        """
        Cancel an active audio stream for a client.

        Args:
            client_id: Client identifier
            timestamp: Optional timestamp to identify specific stream

        Returns:
            bool: True if stream was cancelled
        """
        try:
            if timestamp:
                stream_id = f"{client_id}_{timestamp}"
                if stream_id in self.active_streams:
                    self.active_streams[stream_id] = False
                    self.logger.info(f"Cancelled stream {stream_id}")
                    return True
            else:
                # Cancel all streams for this client
                cancelled_count = 0
                streams_to_remove = []
                for stream_id in self.active_streams:
                    if stream_id.startswith(f"{client_id}_"):
                        self.active_streams[stream_id] = False
                        streams_to_remove.append(stream_id)
                        cancelled_count += 1

                for stream_id in streams_to_remove:
                    del self.active_streams[stream_id]

                if cancelled_count > 0:
                    self.logger.info(f"Cancelled {cancelled_count} streams for client {client_id}")
                    return True

            return False

        except Exception as e:
            self.logger.error(f"Error cancelling stream: {str(e)}")
            return False

    def get_service_status(self) -> Dict[str, Any]:
        """
        Get current service status information.

        Returns:
            dict: Status information
        """
        return {
            "initialized": self._initialized,
            "provider_connected": self.provider_client is not None,
            "active_clients": len(self.client_settings),
            "active_streams": len(self.active_streams),
            "provider_url": self.provider_client.tts_provider_url if self.provider_client else None
        }

    async def cleanup(self) -> None:
        """Clean up service resources."""
        # Cancel all active streams
        for stream_id in list(self.active_streams.keys()):
            self.active_streams[stream_id] = False

        # Close provider client connection
        if self.provider_client:
            await self.provider_client.close()

        # Clear all client settings
        self.client_settings.clear()
        self.active_streams.clear()

        self.logger.info("TTS service cleaned up")


# Global TTS service instance
_tts_service = None


def get_tts_service() -> TTSService:
    """Get the global TTS service instance."""
    global _tts_service
    if _tts_service is None:
        _tts_service = TTSService()
    return _tts_service