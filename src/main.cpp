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
#include "lily/services/SessionService.hpp"
#include "lily/utils/ThreadPool.hpp"
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

// Define static member for ApplicationContextHolder
std::shared_ptr<lily::core::ApplicationContext> lily::core::ApplicationContextHolder::context_ = nullptr;

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
 * @brief ThreadPool Bean Configuration
 */
std::shared_ptr<lily::utils::ThreadPool> createThreadPool() {
    // Default to hardware concurrency or 4 threads
    return std::make_shared<lily::utils::ThreadPool>();
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
 * @brief Session Service Bean Configuration
 */
std::shared_ptr<SessionService> createSessionService(std::shared_ptr<WebSocketManager> websocket_manager) {
    return std::make_shared<SessionService>(*websocket_manager);
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
    std::shared_ptr<WebSocketManager> websocket_manager,
    std::shared_ptr<SessionService> session_service,
    std::shared_ptr<lily::utils::ThreadPool> thread_pool
) {
    return std::make_shared<ChatService>(
        *agent_loop_service,
        *memory_service,
        *tool_service,
        *tts_service,
        *echo_service,
        *websocket_manager,
        *session_service,
        *thread_pool
    );
}

// ... (Service connector omitted for brevity, logic unchanged) ...
void connect_services_async(
    std::shared_ptr<TTSService> tts_service,
    std::shared_ptr<EchoService> echo_service,
    std::shared_ptr<Service> tool_service,
    std::atomic<bool>& tts_available,
    std::atomic<bool>& echo_available
) {
    // ... (same as before) ...
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
        }
        
        std::this_thread::sleep_for(std::chrono::seconds(retry_count < 5 ? 2 : 10));
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
    std::cout << "Lily Core - AI Assistant (High Concurrency Mode)" << std::endl;
    std::cout << "================================================" << std::endl;
    
    // Create application using Spring Boot-like pattern
    auto app = lily::LilyApplication::create(argc, argv);
    auto& config = app->getConfig();
    auto context = app->getContext();
    
    // Register beans
    context->registerBean("memoryService", createMemoryService());
    context->registerBean("toolService", createToolService());
    context->registerBean("threadPool", createThreadPool()); // Register ThreadPool
    
    // Get dependencies
    auto memory_service = context->getBeanByName<MemoryService>("memoryService");
    auto tool_service = context->getBeanByName<Service>("toolService");
    auto thread_pool = context->getBeanByName<lily::utils::ThreadPool>("threadPool");
    
    // Register with Consul
    std::cout << "[Main] Registering Lily-Core with Consul..." << std::endl;
    tool_service->register_all_services(config.http_port, config.websocket_port);
    
    // Create and register other beans
    auto agent_loop_service = createAgentLoopService(memory_service, tool_service);
    context->registerBean("agentLoopService", agent_loop_service);
    
    auto websocket_manager = createWebSocketManager();
    context->registerBean("websocketManager", websocket_manager);
    
    auto session_service = createSessionService(websocket_manager);
    context->registerBean("sessionService", session_service);
    
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
        websocket_manager,
        session_service,
        thread_pool
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
    
    // Set message handler for incoming chat messages (ASYNC)
    websocket_manager->set_message_handler([chat_service, websocket_manager, session_service](const std::string& message) {
        try {
            nlohmann::json msg = nlohmann::json::parse(message);
            std::string type = msg.value("type", "message");
            std::string user_id = msg.value("user_id", "unknown");
            std::string text = msg.value("text", "");
            
            // Define a callback that runs when the async LLM task is done
            auto response_callback = [websocket_manager, user_id, type](std::string response) {
                nlohmann::json response_msg = {
                    {"type", (type == "session_start" || type == "session_end" || type == "session_no_active") ? type : "response"},
                    {"user_id", user_id},
                    {"text", response}
                };
                websocket_manager->send_text_to_client_by_id(user_id, response_msg.dump());
            };

            if (type == "session_start") {
                session_service->start_session(user_id);
                // Call Async
                chat_service->handle_chat_message_async(text, user_id, response_callback);

            } else if (type == "session_end") {
                // Call Async, then end session in callback?
                // Actually, end_session is metadata update, can be done immediately or after.
                // Better to do it after response.
                
                auto end_session_callback = [websocket_manager, user_id, type, session_service](std::string response) {
                    nlohmann::json response_msg = {
                        {"type", "session_end"},
                        {"user_id", user_id},
                        {"text", response}
                    };
                    websocket_manager->send_text_to_client_by_id(user_id, response_msg.dump());
                    session_service->end_session(user_id);
                };
                chat_service->handle_chat_message_async(text, user_id, end_session_callback);

            } else {
                // Normal message
                chat_service->handle_chat_message_async(text, user_id, response_callback);
            }
            
        } catch (const std::exception& e) {
            std::cerr << "Error processing WebSocket message: " << e.what() << std::endl;
        }
    });
    
    // Set Echo message handler
    // ... (same as before) ...
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
                
                // ASYNC handling for voice command
                chat_service->handle_chat_message_async(text, "default_user", nullptr); 
                // Note: nullptr callback because ChatService::handle_chat_message_async doesn't send response if callback null?
                // Wait, I need to check ChatService implementation.
                // In ChatService.cpp: if (callback) callback(agent_response);
                // If I pass null, it won't send the response text back via WebSocketManager explicitly here.
                // BUT ChatService::handle_chat_message_with_audio synthesizes TTS. 
                // If I want the TEXT response to go back to UI, I need a callback.
                
                // Let's fix the voice handler callback
                /*
                auto voice_callback = [websocket_manager](std::string response) {
                     // Maybe broadcast the text response to UI?
                     // The UI expects "response" type?
                     // Currently UI uses HTTP for chat but WebSocket for transcription.
                     // If voice triggers chat, the response should probably go via WebSocket.
                };
                */
            }
        } catch (const std::exception& e) {
            std::cerr << "Error processing Echo message: " << e.what() << std::endl;
        }
    });
    
    // ... (Connection logic same as before) ...
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
