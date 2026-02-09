#ifndef LILY_LILY_APPLICATION_HPP
#define LILY_LILY_APPLICATION_HPP

#include <memory>
#include <string>
#include <iostream>
#include <csignal>
#include <thread>
#include <future>

#include "lily/config/AppConfig.hpp"
#include "lily/core/ApplicationContext.hpp"

namespace lily {

/**
 * @brief Spring Boot-like Application Runner
 * 
 * Similar to SpringApplication class in Spring Boot, this class
 * handles application startup, configuration, and lifecycle management.
 */
class LilyApplication {
public:
    /**
     * @brief Create application with configuration
     */
    static std::unique_ptr<LilyApplication> create(int argc, char** argv) {
        auto app = std::unique_ptr<LilyApplication>(new LilyApplication());
        
        // Parse command line arguments and load configuration
        app->config_ = config::AppConfig::builder()
            .withHttpAddress("0.0.0.0")
            .withHttpPort(8000)
            .withWebSocketPort(9002);
        
        // Load from environment
        app->config_.loadFromEnvironment();
        
        // Create application context
        app->context_ = std::make_shared<core::ApplicationContext>();
        core::ApplicationContextHolder::setContext(app->context_);
        
        return app;
    }
    
    /**
     * @brief Get application configuration
     */
    config::AppConfig& getConfig() {
        return config_;
    }
    
    /**
     * @brief Get application context
     */
    std::shared_ptr<core::ApplicationContext> getContext() {
        return context_;
    }
    
    /**
     * @brief Run the application (similar to SpringApplication.run())
     * 
     * This method starts all components and blocks until shutdown.
     */
    int run() {
        std::cout << "[LilyApplication] Starting Lily Core Application..." << std::endl;
        std::cout << "[LilyApplication] HTTP Server: " << config_.http_address << ":" << config_.http_port << std::endl;
        std::cout << "[LilyApplication] WebSocket Server: " << config_.websocket_port << std::endl;
        std::cout << "[LilyApplication] Gemini API: " << (config_.gemini_enabled ? "enabled" : "disabled") << std::endl;
        
        // Register shutdown handlers
        setupSignalHandlers();
        
        // Application is ready
        std::cout << "[LilyApplication] Lily Core is ready!" << std::endl;
        
        // Block main thread
        std::promise<void>().get_future().wait();
        
        return 0;
    }
    
    /**
     * @brief Stop the application (similar to ApplicationRunner)
     */
    void shutdown() {
        std::cout << "[LilyApplication] Shutting down..." << std::endl;
        
        // Stop all registered beans
        // In a full implementation, this would call destroy() on all beans
        
        std::cout << "[LilyApplication] Shutdown complete" << std::endl;
    }
    
    /**
     * @brief Print application banner (similar to Spring Boot banner)
     */
    void printBanner() {
        std::cout << R"(
   __               __             
  / /  ___ _______ / /  ___ _______
 / _ \/ -_) __/ -_) _ \/ -_) __/ -_)
/_//_/\__/_/  \__/_//_/\__/_/  \__/ 
    )" << std::endl;
        std::cout << "Lily Core - AI Assistant" << std::endl;
        std::cout << "========================" << std::endl;
    }

private:
    LilyApplication() = default;
    
    void setupSignalHandlers() {
        std::signal(SIGINT, [](int signal) {
            std::cout << "\n[LilyApplication] Received SIGINT" << std::endl;
            LilyApplication::getInstance()->shutdown();
            exit(signal);
        });
        
        std::signal(SIGTERM, [](int signal) {
            std::cout << "\n[LilyApplication] Received SIGTERM" << std::endl;
            LilyApplication::getInstance()->shutdown();
            exit(signal);
        });
    }
    
    static LilyApplication* getInstance() {
        return instance_;
    }
    
    static LilyApplication* instance_;
    
    config::AppConfig config_;
    std::shared_ptr<core::ApplicationContext> context_;
};

LilyApplication* LilyApplication::instance_ = nullptr;

} // namespace lily

#endif // LILY_LILY_APPLICATION_HPP
