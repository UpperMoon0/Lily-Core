#include "lily/services/MemoryService.hpp"

using namespace lily::services;

MemoryService::MemoryService() {
    // Constructor implementation
}

MemoryService::~MemoryService() {
    // Destructor implementation
}

const std::vector<Message>& MemoryService::get_conversation(const std::string& user_id) {
    if (conversations.find(user_id) == conversations.end()) {
        conversations[user_id] = std::vector<Message>();
    }
    return conversations[user_id];
}

void MemoryService::add_message(const std::string& user_id, const std::string& role, const std::string& content) {
    Message message;
    message.role = role;
    message.content = content;
    message.timestamp = std::chrono::system_clock::now();
    conversations[user_id].push_back(message);
}

void MemoryService::clear_conversation(const std::string& user_id) {
    if (conversations.find(user_id) != conversations.end()) {
        conversations.erase(user_id);
    }
}

std::string MemoryService::summarize_conversation(const std::string& user_id) {
    return "This is a placeholder summary.";
}