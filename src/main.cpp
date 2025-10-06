#include <iostream>
#include <memory>
#include <csignal>

#include "lily/services/AgentLoopService.hpp"
#include "lily/services/ChatService.hpp"
#include "lily/services/MemoryService.hpp"
#include "lily/services/Service.hpp"
#include "lily/services/TTSService.hpp"
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

std::unique_ptr<HTTPServer> http_server_ptr;

void signal_handler(int signal) {
    if (http_server_ptr) {
        http_server_ptr->stop();
    }
    exit(signal);
}

int main() {
    if (getenv("GEMINI_API_KEY") == nullptr) {
        std::cerr << "Error: GEMINI_API_KEY environment variable not set." << std::endl;
        return 1;
    }

    auto tts_service = std::make_shared<TTSService>();
    auto tool_service = std::make_shared<Service>();
    tool_service->start_periodic_discovery();  // Start periodic tool discovery
    
    // Give some time for service discovery to complete
    std::this_thread::sleep_for(std::chrono::seconds(2));
    
    // Try to connect to TTS provider
    auto discovered_servers = tool_service->get_discovered_servers();
    auto tools_per_server = tool_service->get_tools_per_server();
    
    // Look for TTS provider in the discovered services
    for (const auto& service : tool_service->get_services_info()) {
        if (service.id == "tts-provider") {
            std::cout << "Found TTS provider at " << service.http_url << std::endl;
            if (tts_service->connect(service.websocket_url.empty() ? service.http_url : service.websocket_url)) {
                std::cout << "Connected to TTS provider successfully." << std::endl;
            } else {
                std::cerr << "Failed to connect to TTS provider." << std::endl;
            }
            break;
        }
    }
    
    auto memory_service = std::make_shared<MemoryService>();
    auto agent_loop_service = std::make_shared<AgentLoopService>(*memory_service, *tool_service.get());
    auto websocket_manager = std::make_shared<WebSocketManager>();
    auto chat_service = std::make_shared<ChatService>(
        *agent_loop_service,
        *memory_service,
        *tool_service.get(),
        *tts_service,
        *websocket_manager
    );

    http_server_ptr = std::make_unique<HTTPServer>("0.0.0.0", 8000, *chat_service, *memory_service, *tool_service.get(), *websocket_manager, *agent_loop_service);
    http_server_ptr->start();

    websocket_manager->set_port(9002);
    websocket_manager->set_ping_interval(30);  // Ping every 30 seconds
    websocket_manager->set_pong_timeout(60);  // Timeout after 60 seconds
    
    // Set binary message handler for audio data
    websocket_manager->set_binary_message_handler([&chat_service, &websocket_manager, tool_service](const std::vector<uint8_t>& audio_data) {
        // Send audio data to Echo service via WebSocket for streaming transcription
        std::cout << "Received binary audio data of size: " << audio_data.size() << " bytes" << std::endl;

        // Send audio data to Echo service
        websocket_manager->send_audio_to_echo(audio_data);
    });

    // Set Echo message handler to process transcription results
    websocket_manager->set_echo_message_handler([&chat_service, &websocket_manager](const nlohmann::json& message) {
        try {
            std::string message_type = message["type"];
            std::string text = message["text"];

            if (message_type == "interim") {
                std::cout << "Interim transcription: " << text << std::endl;
                // Forward interim transcription to UI for live display
                nlohmann::json ui_message = {
                    {"type", "interim"},
                    {"text", text}
                };
                std::string ui_message_str = ui_message.dump();
                websocket_manager->broadcast("transcription:" + ui_message_str);
            } else if (message_type == "final") {
                std::cout << "Final transcription: " << text << std::endl;
                // Forward final transcription to UI
                nlohmann::json ui_message = {
                    {"type", "final"},
                    {"text", text}
                };
                std::string ui_message_str = ui_message.dump();
                websocket_manager->broadcast("transcription:" + ui_message_str);

                // Send final transcription to chat service (triggers agent loop)
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
            // Use WebSocket URL if available, otherwise construct from HTTP URL
            if (!service.websocket_url.empty()) {
                echo_websocket_url = service.websocket_url + "/ws/transcribe";
            } else {
                // Convert HTTP URL to WebSocket URL
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

    // Keep the main thread alive to allow the server to run
    std::promise<void>().get_future().wait();

    return 0;
}