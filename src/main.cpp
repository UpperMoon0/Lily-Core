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
    
    // Set binary message handler for audio data
    websocket_manager->set_binary_message_handler([&chat_service, tool_service](const std::vector<uint8_t>& audio_data) {
        // Here we'll handle the audio data: send to Echo service for transcription
        std::cout << "Received binary audio data of size: " << audio_data.size() << " bytes" << std::endl;
        
        // Find Echo service from discovered services
        std::string echo_service_url;
        for (const auto& service : tool_service->get_services_info()) {
            if (service.id == "echo") {
                echo_service_url = service.http_url;
                std::cout << "Found Echo service at " << echo_service_url << std::endl;
                break;
            }
        }
        
        if (echo_service_url.empty()) {
            std::cerr << "Echo service not found. Cannot transcribe audio." << std::endl;
            return;
        }
        
        // Send audio data to Echo service for transcription
        try {
            // Create HTTP client
            web::http::client::http_client client(echo_service_url);
            
            // Prepare request
            web::http::http_request request(web::http::methods::POST);
            request.set_request_uri("/transcribe");
            
            // Set audio data as binary body
            request.set_body(audio_data);
            request.headers().set_content_type("application/octet-stream");
            
            // Send request and get response
            auto response = client.request(request).get();
            
            if (response.status_code() == web::http::status_codes::OK) {
                auto response_json = response.extract_json().get();
                // Convert cpprest json to string and parse with nlohmann for consistency
                std::string response_str = utility::conversions::to_utf8string(response_json.serialize());
                auto nlohmann_json = nlohmann::json::parse(response_str);
                std::string transcribed_text = nlohmann_json["text"].get<std::string>();
                std::cout << "Transcribed text: " << transcribed_text << std::endl;
                
                // Pass transcribed text to chat service
                chat_service->handle_chat_message(transcribed_text, "default_user");
            } else {
                std::cerr << "Echo service returned error: " << response.status_code() << std::endl;
                auto error_body = response.extract_string().get();
                std::cerr << "Error body: " << error_body << std::endl;
            }
        } catch (const std::exception& e) {
            std::cerr << "Error calling Echo service: " << e.what() << std::endl;
        }
    });
    
    websocket_manager->run();

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    // Keep the main thread alive to allow the server to run
    std::promise<void>().get_future().wait();

    return 0;
}