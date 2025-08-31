#ifndef LILY_SERVICES_WEBSOCKETMANAGER_HPP
#define LILY_SERVICES_WEBSOCKETMANAGER_HPP

#include <vector>
#include <string>
#include <memory>

// Forward declaration for a WebSocket connection type.
// This will be replaced with the actual type from the WebSocket library.
namespace websocketpp {
    namespace connection {
        template <typename T>
        class weak_ptr;
    }
}

namespace lily {
    namespace services {
        // Using a placeholder for the connection handle for now.
        using ConnectionHandle = websocketpp::connection::weak_ptr<void>;

        class WebSocketManager {
        public:
            WebSocketManager();
            ~WebSocketManager();

            void connect(const ConnectionHandle& conn);
            void disconnect(const ConnectionHandle& conn);
            void broadcast(const std::string& message);

        private:
            std::vector<ConnectionHandle> _connections;
        };
    }
}

#endif // LILY_SERVICES_WEBSOCKETMANAGER_HPP