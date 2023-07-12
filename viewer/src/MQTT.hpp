#pragma once

#include "eventpp/eventdispatcher.h"

#include "esp_event.h"
#include "esp_log.h"
#include "esp_tls.h"
#include "mqtt_client.h"

#include <atomic>
#include <string>
#include <string_view>
#include <utility>

class MQTT {
public:
    using EventID = esp_mqtt_event_id_t;
    using Dispatcher = eventpp::EventDispatcher<EventID, void(esp_mqtt_event_handle_t)>;
    using Callback = Dispatcher::Callback;

private:
    static constexpr const char* s_tag = "MQTT";

    esp_mqtt_client_config_t m_config;
    esp_mqtt_client_handle_t m_client;

    Dispatcher m_dispatcher;

    std::atomic_bool m_connected = false;

    static void trampoline(void* handler_args, esp_event_base_t base, std::int32_t event_id, void* event_data);

public:
    MQTT(const std::string& host, const std::string& user, const std::string& password);
    MQTT(const std::string& host, const std::string& user, const std::string& password, const std::string& certificate);
    ~MQTT();

    void start();
    void stop();
    void publish(std::string_view topic, std::string_view message);
    void subscribe(std::string_view topic);
    void subscribe(std::string_view topic, Callback callback);
    void unsubscribe(std::string_view topic);

    void on(EventID event, Callback callback);

    bool connected() const;
};
