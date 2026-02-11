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
#include "lily/services/GatewayService.hpp"
#include "lily/services/SessionService.hpp"
#include "lily/controller/ChatController.hpp"
#include "lily/controller/SystemController.hpp"
#include "lily/controller/SessionController.hpp"
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

// Define static member for ApplicationContextHolder
std::shared_ptr<lily::core::ApplicationContext> lily::core::ApplicationContextHolder::context_ = nullptr;

void signal_handler(int signal) {
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
    std::shared_ptr<Service> tool_service,
    lily::config::AppConfig& config
) {
    return std::make_shared<AgentLoopService>(*memory_service, *tool_service, config);
}

/**
 * @brief Gateway Service Bean Configuration
 */
std::shared_ptr<GatewayService> createGatewayService() {
    return std::make_shared<GatewayService>();
}

/**
 * @brief Session Service Bean Configuration
 */
std::shared_ptr<SessionService> createSessionService(std::shared_ptr<GatewayService> gateway_service) {
    return std::make_shared<SessionService>(*gateway_service);
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
    std::shared_ptr<GatewayService> gateway_service,
    std::shared_ptr<SessionService> session_service,
    std::shared_ptr<lily::utils::ThreadPool> thread_pool
) {
    return std::make_shared<ChatService>(
        *agent_loop_service,
        *memory_service,
        *tool_service,
        *tts_service,
        *echo_service,
        *gateway_service,
        *session_service,
        *thread_pool
    );
}

// Controllers

std::shared_ptr<lily::controller::SystemController> createSystemController(
    lily::config::AppConfig& config,
    std::shared_ptr<Service> toolService
) {
    return std::make_shared<lily::controller::SystemController>(config, *toolService);
}

std::shared_ptr<lily::controller::SessionController> createSessionController(
    std::shared_ptr<SessionService> sessionService,
    std::shared_ptr<GatewayService> gatewayService
) {
    return std::make_shared<lily::controller::SessionController>(sessionService, gatewayService);
}

std::shared_ptr<lily::controller::ChatController> createChatController(
    std::shared_ptr<ChatService> chatService,
    std::shared_ptr<AgentLoopService> agentLoopService,
    std::shared_ptr<MemoryService> memoryService
) {
    return std::make_shared<lily::controller::ChatController>(chatService, agentLoopService, memoryService);
}

// ... (Service connector omitted for brevity, logic unchanged) ...
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
            if (!tts_available) {
                // Try to get TTS URL (prefer WebSocket if available)
                std::string tts_url = tool_service->getServiceUrl("tts-provider", "ws");
                if (tts_url.empty()) tts_url = tool_service->getServiceUrl("tts-provider", "http");
                
                if (!tts_url.empty()) {
                     if (tts_service->connect(tts_url)) {
                         tts_available = true;
                         std::cout << "[ServiceConnector] Connected to TTS provider at " << tts_url << std::endl;
                     }
                }
            }
            
            if (!echo_available) {
                std::string echo_url = tool_service->getServiceUrl("echo", "http");
                if (!echo_url.empty()) {
                    if (echo_service->connect(echo_url)) {
                        echo_available = true;
                        std::cout << "[ServiceConnector] Connected to Echo provider at " << echo_url << std::endl;
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
    
    // Set config file path and load
    config.setConfigFilePath("/app/data/config.json");
    config.loadFromFile();

    // Register beans
    context->registerBean("memoryService", createMemoryService());
    context->registerBean("toolService", createToolService());
    context->registerBean("threadPool", createThreadPool()); // Register ThreadPool
    
    // Get dependencies
    auto memory_service = context->getBeanByName<MemoryService>("memoryService");
    auto tool_service = context->getBeanByName<Service>("toolService");
    auto thread_pool = context->getBeanByName<lily::utils::ThreadPool>("threadPool");
    
    // Create and register other beans
    auto agent_loop_service = createAgentLoopService(memory_service, tool_service, config);
    context->registerBean("agentLoopService", agent_loop_service);
    
    auto gateway_service = createGatewayService();
    context->registerBean("gatewayService", gateway_service);
    
    auto session_service = createSessionService(gateway_service);
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
        gateway_service,
        session_service,
        thread_pool
    );
    context->registerBean("chatService", chat_service);

    // Create Controllers
    auto system_controller = createSystemController(config, tool_service);
    system_controller->setAgentLoopService(agent_loop_service.get());
    auto session_controller = createSessionController(session_service, gateway_service);
    auto chat_controller = createChatController(chat_service, agent_loop_service, memory_service);

    // Set dependencies for GatewayService
    gateway_service->set_controllers(chat_controller, system_controller, session_controller);
    gateway_service->set_dependencies(
        chat_service,
        session_service,
        config
    );

    // Register with Consul
    std::cout << "[Main] Registering Lily-Core with Consul..." << std::endl;
    // We only register the single HTTP port (which handles both HTTP and WS upgrade)
    tool_service->register_all_services(config.http_port, config.http_port);
    
    // Track service availability
    std::atomic<bool> tts_available{false};
    std::atomic<bool> echo_available{false};
    
    // Start background service connector
    std::thread service_connector(connect_services_async, 
                                   tts_service, echo_service, tool_service,
                                   std::ref(tts_available), std::ref(echo_available));
    service_connector.detach();
    
    // Check for Gemini API
    bool gemini_available = config.getGeminiApiKeyCount() > 0;
    if (!gemini_available) {
        std::cerr << "[Main] Warning: GEMINI_API_KEY not set. AI features will be disabled." << std::endl;
    } else {
        std::cout << "[Main] Gemini API: " << config.getGeminiApiKeyCount() << " API key(s) configured (round-robin enabled)" << std::endl;
    }
    
    // Start Unified Server (HTTP + WebSocket)
    std::cout << "[Main] Starting Unified Server on port " << config.http_port << "..." << std::endl;
    
    gateway_service->set_binary_message_handler([chat_service](const std::vector<uint8_t>& data, const std::string& user_id) {
        chat_service->handle_audio_stream(data, user_id);
    });
    
    gateway_service->set_port(config.http_port);
    gateway_service->set_ping_interval(config.ping_interval);
    gateway_service->set_pong_timeout(config.pong_timeout);
    
    // Set message handler for incoming chat messages (ASYNC)
    gateway_service->set_message_handler([chat_service, gateway_service, session_service](const std::string& message) {
        try {
            nlohmann::json msg = nlohmann::json::parse(message);
            std::string type = msg.value("type", "message");
            std::string user_id = msg.value("user_id", "unknown");
            std::string text = msg.value("text", "");
            
            // Define a callback that runs when the async LLM task is done
            auto response_callback = [gateway_service, user_id, type](std::string response) {
                nlohmann::json response_msg = {
                    {"type", (type == "session_start" || type == "session_end" || type == "session_no_active") ? type : "response"},
                    {"user_id", user_id},
                    {"text", response}
                };
                gateway_service->send_text_to_client_by_id(user_id, response_msg.dump());
            };

            if (type == "session_start") {
                session_service->start_session(user_id);
                // Call Async
                chat_service->handle_chat_message_async(text, user_id, response_callback);

            } else if (type == "session_end") {
                auto end_session_callback = [gateway_service, user_id, type, session_service](std::string response) {
                    nlohmann::json response_msg = {
                        {"type", "session_end"},
                        {"user_id", user_id},
                        {"text", response}
                    };
                    gateway_service->send_text_to_client_by_id(user_id, response_msg.dump());
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
    gateway_service->set_echo_message_handler([&chat_service, &gateway_service](const nlohmann::json& message) {
        try {
            std::string message_type = message["type"];
            std::string text = message["text"];
            
            if (message_type == "interim") {
                std::cout << "Interim transcription: " << text << std::endl;
                nlohmann::json ui_message = {
                    {"type", "interim"},
                    {"text", text}
                };
                gateway_service->broadcast("transcription:" + ui_message.dump());
            } else if (message_type == "final") {
                std::cout << "Final transcription: " << text << std::endl;
                nlohmann::json ui_message = {
                    {"type", "final"},
                    {"text", text}
                };
                gateway_service->broadcast("transcription:" + ui_message.dump());
                
                // ASYNC handling for voice command
                chat_service->handle_chat_message_async(text, "default_user", nullptr); 
            }
        } catch (const std::exception& e) {
            std::cerr << "Error processing Echo message: " << e.what() << std::endl;
        }
    });
    
    std::string echo_websocket_url = tool_service->getServiceUrl("echo", "ws");
    if (!echo_websocket_url.empty()) {
        echo_websocket_url += "/ws/transcribe";
        std::cout << "Found Echo WebSocket endpoint at " << echo_websocket_url << std::endl;
    }
    
    if (!echo_websocket_url.empty()) {
        if (!gateway_service->connect_to_echo(echo_websocket_url)) {
            std::cerr << "Failed to connect to Echo service" << std::endl;
        }
    } else {
        std::cerr << "Echo service not found. Audio transcription will not work." << std::endl;
    }
    
    gateway_service->run();
    
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    std::cout << "[Main] Lily-Core is ready! (Gemini: " << (gemini_available ? "available" : "disabled")
              << ", Echo: connecting asynchronously, TTS: connecting asynchronously)" << std::endl;
    
    // Keep main thread alive
    std::promise<void>().get_future().wait();
    
    return 0;
}
