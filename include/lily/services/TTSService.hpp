#ifndef LILY_SERVICES_TTSSERVICE_HPP
#define LILY_SERVICES_TTSSERVICE_HPP

#include <string>
#include <vector>
#include <cstdint>
#include <memory>
#include <cpprest/ws_client.h>

namespace lily {
    namespace services {
        struct TTSParameters {
            int speaker = 0;
            int sample_rate = 24000;
            std::string model = "edge";
            std::string lang = "en-US";
        };

        class TTSService {
        public:
            TTSService();
            ~TTSService();

            bool connect(const std::string& provider_url);
            std::vector<uint8_t> synthesize_speech(const std::string& text, const TTSParameters& params = TTSParameters());
            void close();

        private:
            bool _is_connected;
            std::string _provider_url;
            std::unique_ptr<web::websockets::client::websocket_client> _websocket_client;
            
            bool initialize_websocket();
        };
    }
}

#endif // LILY_SERVICES_TTSSERVICE_HPP