#ifndef LILY_CONFIG_APP_CONFIG_HPP
#define LILY_CONFIG_APP_CONFIG_HPP

#include <string>
#include <vector>
#include <memory>
#include <mutex>
#include <fstream>
#include <iostream>
#include <sstream>
#include <algorithm>
#include <nlohmann/json.hpp>

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
private:
    // Wrapper to make mutex "copyable" (by creating a new one)
    struct CopyableMutex {
        mutable std::mutex m;
        CopyableMutex() = default;
        CopyableMutex(const CopyableMutex&) {}
        CopyableMutex& operator=(const CopyableMutex&) { return *this; }
    };
    
    mutable CopyableMutex config_mutex;

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
    std::vector<std::string> gemini_api_keys;
    std::string gemini_model = "gemini-2.5-flash";
    std::string gemini_system_prompt = "You are Lily, a helpful AI assistant.";
    
    // Round-robin index for API keys
    size_t _current_key_index = 0;
    
    // Config persistence
    std::string config_file_path;

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
    
    AppConfig& withGeminiApiKeys(const std::vector<std::string>& api_keys) {
        std::lock_guard<std::mutex> lock(config_mutex.m);
        gemini_api_keys.clear();
        for (const auto& key : api_keys) {
            if (!key.empty()) {
                gemini_api_keys.push_back(key);
            }
        }
        gemini_enabled = !gemini_api_keys.empty();
        return *this;
    }

    // Thread-safe getters/setters for dynamic config
    std::vector<std::string> getGeminiApiKeys() const {
        std::lock_guard<std::mutex> lock(config_mutex.m);
        return gemini_api_keys;
    }
    
    std::string getCurrentGeminiApiKey() {
        std::lock_guard<std::mutex> lock(config_mutex.m);
        if (gemini_api_keys.empty()) {
            return "";
        }
        // Get current key and advance round-robin index
        size_t index = _current_key_index % gemini_api_keys.size();
        _current_key_index = (index + 1) % gemini_api_keys.size();
        return gemini_api_keys[index];
    }
    
    std::string peekNextGeminiApiKey() {
        std::lock_guard<std::mutex> lock(config_mutex.m);
        if (gemini_api_keys.empty()) {
            return "";
        }
        size_t index = _current_key_index % gemini_api_keys.size();
        return gemini_api_keys[index];
    }

    void setGeminiApiKeys(const std::vector<std::string>& keys) {
        std::lock_guard<std::mutex> lock(config_mutex.m);
        gemini_api_keys.clear();
        for (const auto& key : keys) {
            if (!key.empty()) {
                gemini_api_keys.push_back(key);
            }
        }
        gemini_enabled = !gemini_api_keys.empty();
        _current_key_index = 0;
    }
    
    void addGeminiApiKey(const std::string& key) {
        std::lock_guard<std::mutex> lock(config_mutex.m);
        if (!key.empty()) {
            gemini_api_keys.push_back(key);
            gemini_enabled = !gemini_api_keys.empty();
        }
    }
    
    void removeGeminiApiKey(const std::string& key) {
        std::lock_guard<std::mutex> lock(config_mutex.m);
        gemini_api_keys.erase(
            std::remove(gemini_api_keys.begin(), gemini_api_keys.end(), key),
            gemini_api_keys.end()
        );
        gemini_enabled = !gemini_api_keys.empty();
        _current_key_index = 0;
    }
    
    size_t getGeminiApiKeyCount() {
        std::lock_guard<std::mutex> lock(config_mutex.m);
        return gemini_api_keys.size();
    }

    std::string getGeminiModel() const {
        std::lock_guard<std::mutex> lock(config_mutex.m);
        return gemini_model;
    }

    void setGeminiModel(const std::string& model) {
        std::lock_guard<std::mutex> lock(config_mutex.m);
        gemini_model = model;
    }

    std::string getGeminiSystemPrompt() const {
        std::lock_guard<std::mutex> lock(config_mutex.m);
        return gemini_system_prompt;
    }

    void setGeminiSystemPrompt(const std::string& prompt) {
        std::lock_guard<std::mutex> lock(config_mutex.m);
        gemini_system_prompt = prompt;
    }
    
    void setConfigFilePath(const std::string& path) {
        config_file_path = path;
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

        if ((env_value = getenv("LILY_WEBSOCKET_PORT")) != nullptr) {
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
        
        // Load GEMINI_API_KEYS from environment variable (comma-separated)
        if ((env_value = getenv("GEMINI_API_KEYS")) != nullptr) {
            std::lock_guard<std::mutex> lock(config_mutex.m);
            gemini_api_keys.clear();
            std::string keys_str(env_value);
            std::stringstream ss(keys_str);
            std::string key;
            while (std::getline(ss, key, ',')) {
                if (!key.empty()) {
                    gemini_api_keys.push_back(key);
                }
            }
            gemini_enabled = !gemini_api_keys.empty();
        }
        
        if ((env_value = getenv("ECHO_WS_URL")) != nullptr) {
            echo_websocket_url = env_value;
        }
        
        if ((env_value = getenv("TTS_PROVIDER_URL")) != nullptr) {
            tts_provider_url = env_value;
        }
    }

    void loadFromFile() {
        if (config_file_path.empty()) return;

        std::ifstream file(config_file_path);
        if (file.is_open()) {
            try {
                nlohmann::json j;
                file >> j;
                
                std::lock_guard<std::mutex> lock(config_mutex.m);
                if (j.contains("gemini_api_keys") && j["gemini_api_keys"].is_array()) {
                    gemini_api_keys.clear();
                    for (const auto& key : j["gemini_api_keys"]) {
                        if (key.is_string()) {
                            gemini_api_keys.push_back(key.get<std::string>());
                        }
                    }
                    gemini_enabled = !gemini_api_keys.empty();
                }
                if (j.contains("gemini_model")) {
                    gemini_model = j["gemini_model"].get<std::string>();
                }
                if (j.contains("gemini_system_prompt")) {
                    gemini_system_prompt = j["gemini_system_prompt"].get<std::string>();
                }
                std::cout << "Loaded configuration from " << config_file_path << std::endl;
            } catch (const std::exception& e) {
                std::cerr << "Error parsing config file: " << e.what() << std::endl;
            }
        }
    }

    void saveToFile() {
        if (config_file_path.empty()) return;

        nlohmann::json j;
        {
            std::lock_guard<std::mutex> lock(config_mutex.m);
            j["gemini_api_keys"] = gemini_api_keys;
            j["gemini_model"] = gemini_model;
            j["gemini_system_prompt"] = gemini_system_prompt;
        }

        std::ofstream file(config_file_path);
        if (file.is_open()) {
            file << j.dump(4);
            std::cout << "Saved configuration to " << config_file_path << std::endl;
        } else {
            std::cerr << "Failed to save configuration to " << config_file_path << std::endl;
        }
    }
};

} // namespace config
} // namespace lily


#endif // LILY_CONFIG_APP_CONFIG_HPP
