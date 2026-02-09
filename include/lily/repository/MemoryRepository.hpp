#ifndef LILY_REPOSITORY_MEMORY_REPOSITORY_HPP
#define LILY_REPOSITORY_MEMORY_REPOSITORY_HPP

#include <string>
#include <vector>
#include <mutex>
#include <map>
#include <nlohmann/json.hpp>

namespace lily {
namespace repository {

/**
 * @brief Conversation memory entity
 * 
 * Similar to JPA Entity in Spring Boot, this represents
 * a stored conversation in memory.
 */
struct ConversationMemory {
    std::string conversation_id;
    std::string user_id;
    std::vector<nlohmann::json> messages;
    uint64_t created_at;
    uint64_t last_updated_at;
    
    ConversationMemory() : created_at(0), last_updated_at(0) {}
    
    ConversationMemory(const std::string& conv_id, const std::string& user)
        : conversation_id(conv_id), user_id(user), created_at(0), last_updated_at(0) {}
};

/**
 * @brief Repository interface for conversation memory
 * 
 * Similar to Spring Data JPA Repository, this defines
 * the contract for data access operations.
 */
class IMemoryRepository {
public:
    virtual ~IMemoryRepository() = default;
    
    virtual void save(const ConversationMemory& memory) = 0;
    virtual std::optional<ConversationMemory> findById(const std::string& conversation_id) = 0;
    virtual std::vector<ConversationMemory> findByUserId(const std::string& user_id) = 0;
    virtual void deleteById(const std::string& conversation_id) = 0;
    virtual void deleteAll() = 0;
    virtual size_t count() = 0;
};

/**
 * @brief In-memory implementation of MemoryRepository
 * 
 * Similar to Spring's @Repository with @InMemory database.
 * This is a simple in-memory implementation for demonstration.
 */
class MemoryRepository : public IMemoryRepository {
public:
    MemoryRepository() = default;
    
    void save(const ConversationMemory& memory) override {
        std::lock_guard<std::mutex> lock(mutex_);
        conversations_[memory.conversation_id] = memory;
    }
    
    std::optional<ConversationMemory> findById(const std::string& conversation_id) override {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = conversations_.find(conversation_id);
        if (it != conversations_.end()) {
            return it->second;
        }
        return std::nullopt;
    }
    
    std::vector<ConversationMemory> findByUserId(const std::string& user_id) override {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<ConversationMemory> result;
        for (const auto& pair : conversations_) {
            if (pair.second.user_id == user_id) {
                result.push_back(pair.second);
            }
        }
        return result;
    }
    
    void deleteById(const std::string& conversation_id) override {
        std::lock_guard<std::mutex> lock(mutex_);
        conversations_.erase(conversation_id);
    }
    
    void deleteAll() override {
        std::lock_guard<std::mutex> lock(mutex_);
        conversations_.clear();
    }
    
    size_t count() override {
        std::lock_guard<std::mutex> lock(mutex_);
        return conversations_.size();
    }

private:
    std::mutex mutex_;
    std::map<std::string, ConversationMemory> conversations_;
};

/**
 * @brief Chat message DTO (Data Transfer Object)
 * 
 * Similar to Spring's DTO pattern, this is used for
 * transferring chat messages between layers.
 */
struct ChatMessageDto {
    std::string role;  // "user", "assistant", "system"
    std::string content;
    uint64_t timestamp;
    
    nlohmann::json toJson() const {
        return {
            {"role", role},
            {"content", content},
            {"timestamp", timestamp}
        };
    }
    
    static ChatMessageDto fromJson(const nlohmann::json& json) {
        ChatMessageDto dto;
        dto.role = json.value("role", "");
        dto.content = json.value("content", "");
        dto.timestamp = json.value("timestamp", 0);
        return dto;
    }
};

} // namespace repository
} // namespace lily

#endif // LILY_REPOSITORY_MEMORY_REPOSITORY_HPP
