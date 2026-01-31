#ifndef LILY_SERVICES_ECHOSERVICE_HPP
#define LILY_SERVICES_ECHOSERVICE_HPP

#include <string>
#include <vector>
#include <cstdint>
#include <memory>
#include <functional>
#include <thread>
#include <atomic>
#include <cpprest/ws_client.h>

namespace lily {
    namespace services {
        
        using TranscriptionHandler = std::function<void(const std::string&)>;

        class EchoService {
        public:
            EchoService();
            ~EchoService();

            bool connect(const std::string& provider_url);
            void send_audio(const std::vector<uint8_t>& audio_data);
            void set_transcription_handler(const TranscriptionHandler& handler);
            void close();
            bool is_connected() const { return _is_connected; }

        private:
            std::atomic<bool> _is_connected;
            std::string _provider_url;
            std::unique_ptr<web::websockets::client::websocket_client> _websocket_client;
            TranscriptionHandler _transcription_handler;
            std::thread _receive_thread;
            
            void on_message(web::websockets::client::websocket_incoming_message msg);
            void receive_loop();
        };
    }
}

#endif // LILY_SERVICES_ECHOSERVICE_HPP