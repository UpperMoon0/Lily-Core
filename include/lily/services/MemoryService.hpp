#ifndef MEMORY_SERVICE_HPP
#define MEMORY_SERVICE_HPP

#include <string>
#include <vector>
#include <map>
#include <chrono>

namespace lily {
    namespace services {
        struct Message {
            std::string role;
            std::string content;
            std::chrono::system_clock::time_point timestamp;
        };

        class MemoryService {
        public:
            MemoryService();
            ~MemoryService();

            const std::vector<Message>& get_conversation(const std::string& user_id);
            void add_message(const std::string& user_id, const std::string& role, const std::string& content);
            void clear_conversation(const std::string& user_id);
            std::string summarize_conversation(const std::string& user_id);

        private:
            std::map<std::string, std::vector<Message>> conversations;
        };
    }
}

#endif // MEMORY_SERVICE_HPP