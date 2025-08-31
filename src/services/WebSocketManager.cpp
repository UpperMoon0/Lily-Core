#include "lily/services/WebSocketManager.hpp"
#include <iostream>
#include <algorithm>

namespace lily {
    namespace services {

        WebSocketManager::WebSocketManager() {
            std::cout << "WebSocketManager initialized." << std::endl;
        }

        WebSocketManager::~WebSocketManager() {
            std::cout << "WebSocketManager shutting down." << std::endl;
        }

        void WebSocketManager::connect(const ConnectionHandle& conn) {
            // For now, we just log the connection.
            // In a real implementation, we would store the connection pointer.
            std::cout << "Client connected." << std::endl;
            _connections.push_back(conn);
        }

        void WebSocketManager::disconnect(const ConnectionHandle& conn) {
            // For now, we just log the disconnection.
            // In a real implementation, we would find and remove the connection pointer.
            std::cout << "Client disconnected." << std::endl;
            // The following line requires a proper comparison for weak_ptr, which is complex.
            // For this placeholder, we'll just clear the list if it gets too large.
            if (_connections.size() > 100) {
                _connections.clear();
            }
        }

        void WebSocketManager::broadcast(const std::string& message) {
            std::cout << "Broadcasting message: " << message << std::endl;
            // In a real implementation, we would iterate over _connections
            // and send the message to each client.
        }

    }
}