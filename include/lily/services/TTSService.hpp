#ifndef LILY_SERVICES_TTSSERVICE_HPP
#define LILY_SERVICES_TTSSERVICE_HPP

#include <string>
#include <vector>
#include <cstdint>

namespace lily {
    namespace services {
        class TTSService {
        public:
            TTSService();
            ~TTSService();

            bool connect(const std::string& provider_url);
            std::vector<uint8_t> synthesize_speech(const std::string& text);
            void close();

        private:
            bool _is_connected;
        };
    }
}

#endif // LILY_SERVICES_TTSSERVICE_HPP