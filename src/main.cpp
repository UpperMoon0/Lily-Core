#include <iostream>
#include <memory>
#include <csignal>

#include "lily/LilyApplication.hpp"
#include "lily/services/AgentLoopService.hpp"
#include "lily/services/ChatService.hpp"
#include "lily/services/MemoryService.hpp"
#include "lily/services/Service.hpp"
#include "lily/services/TTSService.hpp"
#include "lily/services/EchoService.hpp"
#include "lily/services/WebSocketManager.hpp"
#include "lily/services/HTTPServer.hpp"
#include <thread>
#include <chrono>
#include <future>
#include <nlohmann/json.hpp>
#include <cpprest/http_client.h>
#include <cpprest/filestream.h>

using namespace lily::services;
using namespace web;
using namespace web::http;
using namespace web::http::client;

// Global server pointer for signal handling
std::unique_ptr<HTTPServer> http_server_ptr;

void signal_handler(int signal) {
    if (http_server_ptr) {
        http_server_ptr->stop();
    }
    exit(signal);
}

// ============================================================================
// Spring Boot-style Bean Configuration
// ============================================================================

/**
 * @brief Memory Service Bean Configuration
 * 
 * Similar to @Configuration class in Spring Boot.
 */
std::shared_ptr<MemoryService> createMemoryService() {
    return std::make_shared<MemoryService>();
}

/**
 * @brief Service (Tool) Bean Configuration
 */
std::shared_ptr<Service> createToolService() {
    auto service = std::make_shared<Service>();
    service->start_periodic_discovery();
    return service;
}

/**
 * @brief Agent Loop Service Bean Configuration
 */
std::shared_ptr<AgentLoopService> createAgentLoopService(
    std::shared_ptr<MemoryService> memory_service,
    std::shared_ptr<Service> tool_service
) {
    return std::make_shared<AgentLoopService>(*memory_service, *tool_service);
}

/**
 * @brief WebSocket Manager Bean Configuration
 */
std::shared_ptr<WebSocketManager> createWebSocketManager() {
    return std::make_shared<WebSocketManager>();
}

/**
 * @brief TTS Service Bean Configuration
 */
std::shared_ptr<TTSService> createTTSService() {
    return std::make_shared<TTSService>();
}

/**
 * @brief Echo Service Bean Configuration
 */
std::shared_ptr<EchoService> createEchoService() {
    return std::make_shared<EchoService>();
}

/**
 * @brief Chat Service Bean Configuration
 */
std::shared_ptr<ChatService> createChatService(
    std::shared_ptr<AgentLoopService> agent_loop_service,
    std::shared_ptr<MemoryService> memory_service,
    std::shared_ptr<Service> tool_service,
    std::shared_ptr<TTSService> tts_service,
    std::shared_ptr<EchoService> echo_service,
    std::shared_ptr<WebSocketManager> websocket_manager
) {
    return std::make_shared<ChatService>(
        *agent_loop_service,
        *memory_service,
        *tool_service,
        *tts_service,
        *echo_service,
        *websocket_manager
    );
}

// ============================================================================
// Background Service Connector
// ============================================================================

void connect_services_async(
    std::shared_ptr<TTSService> tts_service,
    std::shared_ptr<EchoService> echo_service,
    std::shared_ptr<Service> tool_service,
    std::atomic<bool>& tts_available,
    std::atomic<bool>& echo_available
) {
    std::cout << "[ServiceConnector] Starting background service discovery..." << std::endl;
    
    int retry_count = 0;
    while (true) {
        if (!tts_available || !echo_available) {
            auto services = tool_service->get_services_info();
            
            if (!tts_available) {
                for (const auto& service : services) {
                    if (service.id == "tts-provider") {
                        if (tts_service->connect(service.websocket_url.empty() ? service.http_url : service.websocket_url)) {
                            tts_available = true;
                            std::cout << "[ServiceConnector] Connected to TTS provider at " << service.http_url << std::endl;
                        }
                    }
                }
            }
            
            if (!echo_available) {
                for (const auto& service : services) {
                    if (service.id == "echo") {
                        if (echo_service->connect(service.http_url)) {
                            echo_available = true;
                            std::cout << "[ServiceConnector] Connected to Echo provider at " << service.http_url << std::endl;
                        }
                    }
                }
            }
            
            if (retry_count % 15 == 0) {
                std::cout << "[ServiceConnector] Status - Echo: " << (echo_available ? "connected" : "waiting")
                          << ", TTS: " << (tts_available ? "connected" : "waiting") << std::endl;
            }
        }
        
        if (!tts_available || !echo_available) {
            std::this_thread::sleep_for(std::chrono::seconds(2));
        } else {
            std::this_thread::sleep_for(std::chrono::seconds(10));
        }
        retry_count++;
    }
}

// ============================================================================
// Main Application Entry Point
// ============================================================================

int main(int argc, char** argv) {
    // Print banner
    std::cout << R"(
  _      _ _       
 | |    (_) |      
 | |     _| |_   _ 
 | |    | | | | | |
 | |____| | | |_| |
 |______|_|_|\__, |
              __/ |
             |___/ 
    )" << std::endl;
    std::cout << "Lily Core - AI Assistant" << std::endl;
    std::cout << "========================" << std::endl;
    
    // Create application using Spring Boot-like pattern
    auto app = lily::LilyApplication::create(argc, argv);
    auto& config = app->getConfig();
    auto context = app->getContext();
    
    // Register beans in application context
    context->registerBean("memoryService", createMemoryService());
    context->registerBean("toolService", createToolService());
    
    // Get dependencies
    auto memory_service = context->getBeanByName<MemoryService>("memoryService");
    auto tool_service = context->getBeanByName<Service>("toolService");
    
    // Register with Consul
    std::cout << "[Main] Registering Lily-Core with Consul..." << std::endl;
    tool_service->register_all_services(config.http_port, config.websocket_port);
    
    // Create and register other beans
    auto agent_loop_service = createAgentLoopService(memory_service, tool_service);
    context->registerBean("agentLoopService", agent_loop_service);
    
    auto websocket_manager = createWebSocketManager();
    context->registerBean("websocketManager", websocket_manager);
    
    auto tts_service = createTTSService();
    context->registerBean("ttsService", tts_service);
    
    auto echo_service = createEchoService();
    context->registerBean("echoService", echo_service);
    
    auto chat_service = createChatService(
        agent_loop_service,
        memory_service,
        tool_service,
        tts_service,
        echo_service,
        websocket_manager
    );
    context->registerBean("chatService", chat_service);
    
    // Track service availability
    std::atomic<bool> tts_available{false};
    std::atomic<bool> echo_available{false};
    
    // Start background service connector
    std::thread service_connector(connect_services_async, 
                                   tts_service, echo_service, tool_service,
                                   std::ref(tts_available), std::ref(echo_available));
    service_connector.detach();
    
    // Check for Gemini API
    bool gemini_available = (getenv("GEMINI_API_KEY") != nullptr);
    if (!gemini_available) {
        std::cerr << "[Main] Warning: GEMINI_API_KEY not set. AI features will be disabled." << std::endl;
    }
    
    // Start HTTP server
    std::cout << "[Main] Starting HTTP server on port " << config.http_port << "..." << std::endl;
    http_server_ptr = std::make_unique<HTTPServer>(
        config.http_address, 
        config.http_port, 
        *chat_service, 
        *memory_service, 
        *tool_service.get(), 
        *websocket_manager, 
        *agent_loop_service
    );
    http_server_ptr->start();
    
    // Start WebSocket server
    std::cout << "[Main] Starting WebSocket server on port " << config.websocket_port << "..." << std::endl;
    websocket_manager->set_binary_message_handler([chat_service](const std::vector<uint8_t>& data, const std::string& user_id) {
        chat_service->handle_audio_stream(data, user_id);
    });
    
    websocket_manager->set_port(config.websocket_port);
    websocket_manager->set_ping_interval(config.ping_interval);
    websocket_manager->set_pong_timeout(config.pong_timeout);
    
    // Set message handler for incoming chat messages
    // NOTE: Lily-Core only handles simple chat messages.
    // Discord-specific session management (wake-up, goodbye) is handled in Discord Adapter.
    websocket_manager->set_message_handler([chat_service, websocket_manager](const std::string& message) {
        try {
            nlohmann::json msg = nlohmann::json::parse(message);
            std::string user_id = msg.value("user_id", "unknown");
            std::string text = msg.value("text", "");
            
            // Lily-Core's responsibility: Process the message through the LLM agent loop
            std::string response = chat_service->handle_chat_message(text, user_id);
            
            // Send response back to the client
            nlohmann::json response_msg = {
                {"type", "response"},
                {"user_id", user_id},
                {"text", response}
            };
            websocket_manager->send_text_to_client_by_id(user_id, response_msg.dump());
            
        } catch (const std::exception& e) {
            std::cerr << "Error processing WebSocket message: " << e.what() << std::endl;
        }
    });
    
    // Set Echo message handler
    websocket_manager->set_echo_message_handler([&chat_service, &websocket_manager](const nlohmann::json& message) {
        try {
            std::string message_type = message["type"];
            std::string text = message["text"];
            
            if (message_type == "interim") {
                std::cout << "Interim transcription: " << text << std::endl;
                nlohmann::json ui_message = {
                    {"type", "interim"},
                    {"text", text}
                };
                websocket_manager->broadcast("transcription:" + ui_message.dump());
            } else if (message_type == "final") {
                std::cout << "Final transcription: " << text << std::endl;
                nlohmann::json ui_message = {
                    {"type", "final"},
                    {"text", text}
                };
                websocket_manager->broadcast("transcription:" + ui_message.dump());
                chat_service->handle_chat_message(text, "default_user");
            }
        } catch (const std::exception& e) {
            std::cerr << "Error processing Echo message: " << e.what() << std::endl;
        }
    });
    
    // Connect to Echo service
    std::string echo_websocket_url;
    for (const auto& service : tool_service->get_services_info()) {
        if (service.id == "echo") {
            if (!service.websocket_url.empty()) {
                echo_websocket_url = service.websocket_url + "/ws/transcribe";
            } else {
                std::string http_url = service.http_url;
                if (http_url.find("http://") == 0) {
                    echo_websocket_url = "ws://" + http_url.substr(7) + "/ws/transcribe";
                } else if (http_url.find("https://") == 0) {
                    echo_websocket_url = "wss://" + http_url.substr(8) + "/ws/transcribe";
                }
            }
            std::cout << "Found Echo WebSocket endpoint at " << echo_websocket_url << std::endl;
            break;
        }
    }
    
    if (!echo_websocket_url.empty()) {
        if (!websocket_manager->connect_to_echo(echo_websocket_url)) {
            std::cerr << "Failed to connect to Echo service" << std::endl;
        }
    } else {
        std::cerr << "Echo service not found. Audio transcription will not work." << std::endl;
    }
    
    websocket_manager->run();
    
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    std::cout << "[Main] Lily-Core is ready! (Gemini: " << (gemini_available ? "available" : "disabled")
              << ", Echo: connecting asynchronously, TTS: connecting asynchronously)" << std::endl;
    
    // Keep main thread alive
    std::promise<void>().get_future().wait();
    
    return 0;
}
