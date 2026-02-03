#ifndef LILY_CONFIG_APP_CONFIG_HPP
#define LILY_CONFIG_APP_CONFIG_HPP

#include <string>
#include <memory>

namespace lily {
namespace config {

/**
 * @brief Application configuration class
 * 
 * Similar to Spring Boot's @ConfigurationProperties, this class
 * holds all application configuration loaded from environment variables
 * or configuration files.
 */
class AppConfig {
public:
    // Server configuration
    std::string http_address = "0.0.0.0";
    uint16_t http_port = 8000;
    uint16_t websocket_port = 9002;
    
    // Consul configuration
    std::string consul_host = "localhost";
    uint16_t consul_port = 8500;
    std::string service_name = "lily-core";
    
    // Feature flags
    bool gemini_enabled = false;
    std::string gemini_api_key;
    
    // WebSocket configuration
    uint32_t ping_interval = 30;
    uint32_t pong_timeout = 60;
    
    // Queue configuration
    size_t max_queue_size = 1000;
    size_t max_concurrent_tasks = 10;
    
    // Echo service configuration
    std::string echo_websocket_url;
    bool auto_connect_echo = true;
    
    // TTS service configuration
    std::string tts_provider_url;
    bool auto_connect_tts = true;
    
    // Builder pattern for easier configuration
    static AppConfig builder() {
        return AppConfig();
    }
    
    AppConfig& withHttpAddress(const std::string& address) {
        http_address = address;
        return *this;
    }
    
    AppConfig& withHttpPort(uint16_t port) {
        http_port = port;
        return *this;
    }
    
    AppConfig& withWebSocketPort(uint16_t port) {
        websocket_port = port;
        return *this;
    }
    
    AppConfig& withConsulHost(const std::string& host) {
        consul_host = host;
        return *this;
    }
    
    AppConfig& withConsulPort(uint16_t port) {
        consul_port = port;
        return *this;
    }
    
    AppConfig& withServiceName(const std::string& name) {
        service_name = name;
        return *this;
    }
    
    AppConfig& withGeminiApiKey(const std::string& api_key) {
        if (!api_key.empty()) {
            gemini_api_key = api_key;
            gemini_enabled = true;
        }
        return *this;
    }
    
    AppConfig& withPingInterval(uint32_t seconds) {
        ping_interval = seconds;
        return *this;
    }
    
    AppConfig& withPongTimeout(uint32_t seconds) {
        pong_timeout = seconds;
        return *this;
    }
    
    AppConfig& withMaxQueueSize(size_t size) {
        max_queue_size = size;
        return *this;
    }
    
    AppConfig& withMaxConcurrentTasks(size_t count) {
        max_concurrent_tasks = count;
        return *this;
    }
    
    AppConfig& withEchoWebSocketUrl(const std::string& url) {
        echo_websocket_url = url;
        return *this;
    }
    
    AppConfig& withTtsProviderUrl(const std::string& url) {
        tts_provider_url = url;
        return *this;
    }
    
    /**
     * @brief Load configuration from environment variables
     * 
     * Similar to Spring Boot's property binding, this method
     * loads configuration from environment variables.
     */
    void loadFromEnvironment() {
        // Load from environment variables
        const char* env_value;
        
        if ((env_value = getenv("LILY_HTTP_ADDRESS")) != nullptr) {
            http_address = env_value;
        }
        
        if ((env_value = getenv("LILY_HTTP_PORT")) != nullptr) {
            http_port = static_cast<uint16_t>(std::stoi(env_value));
        }
        
        if ((env_value = getenv("LILY_WS_PORT")) != nullptr) {
            websocket_port = static_cast<uint16_t>(std::stoi(env_value));
        }
        
        if ((env_value = getenv("CONSUL_HOST")) != nullptr) {
            consul_host = env_value;
        }
        
        if ((env_value = getenv("CONSUL_PORT")) != nullptr) {
            consul_port = static_cast<uint16_t>(std::stoi(env_value));
        }
        
        if ((env_value = getenv("LILY_SERVICE_NAME")) != nullptr) {
            service_name = env_value;
        }
        
        if ((env_value = getenv("GEMINI_API_KEY")) != nullptr) {
            gemini_api_key = env_value;
            gemini_enabled = true;
        }
        
        if ((env_value = getenv("ECHO_WS_URL")) != nullptr) {
            echo_websocket_url = env_value;
        }
        
        if ((env_value = getenv("TTS_PROVIDER_URL")) != nullptr) {
            tts_provider_url = env_value;
        }
    }
};

} // namespace config
} // namespace lily

#endif // LILY_CONFIG_APP_CONFIG_HPP
