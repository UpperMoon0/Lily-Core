#!/usr/bin/env python3
"""
WebSocket Manager for Lily-Core TTS connections

Manages WebSocket connections, client sessions, and message routing
for Text-to-Speech functionality with persistent settings per client.
"""

import asyncio
import json
import logging
from typing import Dict, List, Optional, Any, Set
from datetime import datetime

from fastapi import WebSocket, WebSocketDisconnect
from models.api import (
    TTSSettings, TTSSettingsUpdate, TTSRequest,
    TTSResponse, WebSocketError, AudioStreamEnd
)
from services.tts_service import get_tts_service


class WebSocketManager:
    """Manager for WebSocket connections and TTS client sessions."""

    def __init__(self):
        """Initialize the WebSocket manager."""
        self.active_connections: Dict[str, WebSocket] = {}
        self.client_settings: Dict[str, TTSSettings] = {}
        self.connection_times: Dict[str, datetime] = {}
        self.logger = logging.getLogger("WebSocketManager")
        self.tts_service = get_tts_service()
        self._shutdown_event = asyncio.Event()

    async def initialize(self) -> bool:
        """
        Initialize the WebSocket manager and TTS service.

        Returns:
            bool: True if initialization was successful
        """
        try:
            # Initialize TTS service
            tts_initialized = await self.tts_service.initialize()
            if not tts_initialized:
                self.logger.warning("TTS service initialization failed - WebSocket server will work in degraded mode")

            self.logger.info("✅ WebSocket manager initialized successfully")
            return True

        except Exception as e:
            self.logger.error(f"Failed to initialize WebSocket manager: {str(e)}")
            return False

    def is_client_connected(self, client_id: str) -> bool:
        """
        Check if a client is currently connected.

        Args:
            client_id: Client identifier

        Returns:
            bool: True if client is connected
        """
        return client_id in self.active_connections

    def get_connected_clients(self) -> List[str]:
        """
        Get list of all connected client IDs.

        Returns:
            list: List of connected client identifiers
        """
        return list(self.active_connections.keys())

    def get_connection_stats(self) -> Dict[str, Any]:
        """
        Get connection statistics.

        Returns:
            dict: Connection statistics
        """
        return {
            "active_connections": len(self.active_connections),
            "clients_with_settings": len(self.client_settings),
            "uptime": str(datetime.now().isoformat())
        }

    async def connect_client(self, websocket: WebSocket, client_id: str) -> bool:
        """
        Connect a new WebSocket client.

        Args:
            websocket: WebSocket connection
            client_id: Unique client identifier

        Returns:
            bool: True if connection was successful
        """
        try:
            await websocket.accept()

            # Store connection
            self.active_connections[client_id] = websocket
            self.connection_times[client_id] = datetime.now()

            self.logger.info(f"Client {client_id} connected. Active connections: {len(self.active_connections)}")
            return True

        except Exception as e:
            self.logger.error(f"Failed to connect client {client_id}: {str(e)}")
            return False

    async def disconnect_client(self, client_id: str) -> None:
        """
        Disconnect a WebSocket client and clean up resources.

        Args:
            client_id: Client identifier to disconnect
        """
        try:
            # Close WebSocket connection if still active
            if client_id in self.active_connections:
                websocket = self.active_connections[client_id]
                try:
                    await websocket.close()
                except Exception as e:
                    self.logger.debug(f"Error closing WebSocket for client {client_id}: {str(e)}")

                del self.active_connections[client_id]

            # Remove client settings
            if client_id in self.client_settings:
                del self.client_settings[client_id]

            # Remove connection time
            if client_id in self.connection_times:
                del self.connection_times[client_id]

            # Clean up TTS service settings
            self.tts_service.remove_client_settings(client_id)

            self.logger.info(f"Client {client_id} disconnected. Active connections: {len(self.active_connections)}")

        except Exception as e:
            self.logger.error(f"Error disconnecting client {client_id}: {str(e)}")

    async def update_client_settings(self, client_id: str, settings_data: Dict[str, Any]) -> bool:
        """
        Update TTS settings for a client.

        Args:
            client_id: Client identifier
            settings_data: Settings payload

        Returns:
            bool: True if settings were updated successfully
        """
        try:
            # Create TTS settings from payload
            settings = TTSSettings(
                speaker=settings_data.get('speaker', 1),
                sample_rate=settings_data.get('sample_rate', 24000),
                model=settings_data.get('model', 'edge'),
                lang=settings_data.get('lang', 'ja-JP')
            )

            # Store in memory
            self.client_settings[client_id] = settings

            # Also store in TTS service
            self.tts_service.store_client_settings(client_id, settings)

            self.logger.debug(f"Updated settings for client {client_id}: {settings}")
            return True

        except Exception as e:
            self.logger.error(f"Error updating settings for client {client_id}: {str(e)}")
            return False

    async def handle_client_message(self, client_id: str, message_data: Dict[str, Any]) -> None:
        """
        Handle incoming message from a client.

        Args:
            client_id: Client identifier
            message_data: Parsed message data
        """
        try:
            message_type = message_data.get('type', '')

            if message_type == 'settings.update':
                await self._handle_settings_update(client_id, message_data)

            elif message_type == 'tts.request':
                await self._handle_tts_request(client_id, message_data)

            else:
                await self._send_error_message(client_id, 400, f"Unknown message type: {message_type}")

        except Exception as e:
            self.logger.error(f"Error handling message from client {client_id}: {str(e)}")
            await self._send_error_message(client_id, 500, "Internal server error")

    async def _handle_settings_update(self, client_id: str, message_data: Dict[str, Any]) -> None:
        """Handle TTS settings update message."""
        try:
            payload = message_data.get('payload', {})

            # Update client settings
            success = await self.update_client_settings(client_id, payload)

            if success:
                self.logger.info(f"Settings updated successfully for client {client_id}")
                # Optional: Send confirmation back to client
            else:
                await self._send_error_message(client_id, 500, "Failed to update settings")

        except Exception as e:
            self.logger.error(f"Error handling settings update for client {client_id}: {str(e)}")
            await self._send_error_message(client_id, 500, "Error processing settings update")

    async def _handle_tts_request(self, client_id: str, message_data: Dict[str, Any]) -> None:
        """Handle TTS request message."""
        try:
            # Get WebSocket connection
            if client_id not in self.active_connections:
                await self._send_error_message(client_id, 404, "Client connection not found")
                return

            websocket = self.active_connections[client_id]

            # Generate speech using TTS service
            tts_response = await self.tts_service.generate_speech_from_request(message_data, client_id)

            # Stream audio to WebSocket
            await self.tts_service.stream_audio_to_websocket(websocket, tts_response)

        except Exception as e:
            self.logger.error(f"Error handling TTS request for client {client_id}: {str(e)}")
            await self._send_error_message(client_id, 500, "Error processing TTS request")

    async def _send_error_message(self, client_id: str, code: int, message: str) -> None:
        """
        Send an error message to a client.

        Args:
            client_id: Client identifier
            code: Error code
            message: Error message
        """
        try:
            if client_id not in self.active_connections:
                return

            websocket = self.active_connections[client_id]
            error_msg = {
                "type": "error",
                "payload": {
                    "code": code,
                    "message": message
                }
            }

            await websocket.send_text(json.dumps(error_msg))

        except Exception as e:
            self.logger.debug(f"Failed to send error message to client {client_id}: {str(e)}")

    async def handle_client_connection(self, websocket: WebSocket, client_id: str) -> None:
        """
        Handle a complete WebSocket client connection lifecycle.

        Args:
            websocket: WebSocket connection
            client_id: Unique client identifier
        """
        try:
            # Connect the client
            connected = await self.connect_client(websocket, client_id)
            if not connected:
                self.logger.error(f"Failed to connect client {client_id}")
                return

            self.logger.info(f"🎯 WebSocket connection established for client {client_id}")

            # Connection loop - handle incoming messages
            while not self._shutdown_event.is_set():
                try:
                    # Receive message from client
                    message_text = await websocket.receive_text()
                    message_data = json.loads(message_text)

                    # Handle the message
                    await self.handle_client_message(client_id, message_data)

                except WebSocketDisconnect:
                    self.logger.info(f"Client {client_id} disconnected normally")
                    break

                except json.JSONDecodeError:
                    await self._send_error_message(client_id, 400, "Invalid JSON message")
                    continue

                except Exception as e:
                    self.logger.error(f"Error in client connection loop for {client_id}: {str(e)}")
                    await self._send_error_message(client_id, 500, "Internal connection error")
                    break

        except Exception as e:
            self.logger.error(f"Error in WebSocket connection handler for {client_id}: {str(e)}")

        finally:
            # Always disconnect the client on exit
            await self.disconnect_client(client_id)

    async def shutdown(self) -> None:
        """Shutdown the WebSocket manager and close all connections."""
        self.logger.info("Shutting down WebSocket manager...")

        # Set shutdown event
        self._shutdown_event.set()

        # Disconnect all clients
        client_ids = list(self.active_connections.keys())
        disconnect_tasks = []

        for client_id in client_ids:
            disconnect_tasks.append(self.disconnect_client(client_id))

        if disconnect_tasks:
            await asyncio.gather(*disconnect_tasks, return_exceptions=True)

        # Clean up TTS service
        await self.tts_service.cleanup()

        self.logger.info("✅ WebSocket manager shutdown complete")

    async def broadcast_message(self, message: Dict[str, Any], exclude_client: Optional[str] = None) -> None:
        """
        Broadcast a message to all connected clients.

        Args:
            message: Message to broadcast
            exclude_client: Client ID to exclude from broadcast (optional)
        """
        message_text = json.dumps(message)

        tasks = []
        for client_id, websocket in self.active_connections.items():
            if exclude_client and client_id == exclude_client:
                continue

            tasks.append(self._send_text_safe(client_id, message_text))

        if tasks:
            await asyncio.gather(*tasks, return_exceptions=True)

    async def _send_text_safe(self, client_id: str, message_text: str) -> None:
        """
        Send text message safely, handling disconnection errors.

        Args:
            client_id: Client identifier
            message_text: Text message to send
        """
        try:
            if client_id in self.active_connections:
                websocket = self.active_connections[client_id]
                await websocket.send_text(message_text)
        except Exception as e:
            self.logger.debug(f"Failed to send message to client {client_id}: {str(e)}")
            # Client may have disconnected, but we'll let the connection loop handle cleanup


# Global WebSocket manager instance
_websocket_manager = None


def get_websocket_manager() -> WebSocketManager:
    """Get the global WebSocket manager instance."""
    global _websocket_manager
    if _websocket_manager is None:
        _websocket_manager = WebSocketManager()
    return _websocket_manager