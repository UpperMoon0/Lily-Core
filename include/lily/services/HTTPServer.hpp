#ifndef LILY_SERVICES_HTTPSERVER_HPP
#define LILY_SERVICES_HTTPSERVER_HPP

#include <cpprest/http_listener.h>
#include <cpprest/json.h>
#include <functional>
#include <memory>

namespace lily {
namespace services {
    class MemoryService;
}
namespace services {

class ChatService;

class HTTPServer {
public:
    HTTPServer(const std::string& address, uint16_t port, ChatService& chat_service, MemoryService& memory_service);
    ~HTTPServer();

    void start();
    void stop();

private:
    void handle_post(web::http::http_request request);
    void handle_get(web::http::http_request request);
    void handle_delete(web::http::http_request request);
    void handle_options(web::http::http_request request);
    void handle_monitoring(web::http::http_request request);

    web::http::experimental::listener::http_listener _listener;
    ChatService& _chat_service;
    MemoryService& _memory_service;
};

} // namespace services
} // namespace lily

#endif // LILY_SERVICES_HTTPSERVER_HPP