#include "MQTT.hpp"

#include "eventpp/eventdispatcher.h"

#include "esp_crt_bundle.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_tls.h"
#include "mqtt_client.h"

#include <exception>
#include <string>
#include <string_view>
#include <utility>

void MQTT::trampoline(void* handler_args, esp_event_base_t base, int32_t event_id, void* event_data) {
    ESP_LOGI(s_tag, "Received event: %s:%d", base, (int)event_id);
    auto mqtt = static_cast<MQTT*>(handler_args);
    mqtt->m_dispatcher.dispatch(static_cast<esp_mqtt_event_id_t>(event_id), static_cast<esp_mqtt_event_handle_t>(event_data));
}

MQTT::MQTT(const std::string& host, const std::string& user, const std::string& password)
    : MQTT(host, user, password, "") {}

MQTT::MQTT(const std::string& host, const std::string& user, const std::string& password, const std::string& certificate)
    : m_config(esp_mqtt_client_config_t {}) {
    m_config.broker.address.uri = host.c_str();

    if (!user.empty() && !password.empty()) {
        m_config.credentials.username = user.c_str();
        m_config.credentials.authentication.password = password.c_str();
    } else if (user.empty() != password.empty())
        throw std::invalid_argument("User and password must be both empty or both non-empty");

    if (!certificate.empty()) {
        m_config.broker.verification.certificate = certificate.c_str();
    } else {
        // m_config.broker.verification.crt_bundle_attach = esp_crt_bundle_attach;
    }

    m_client = esp_mqtt_client_init(&m_config);
    if (m_client == nullptr)
        throw std::runtime_error("Failed to initialize MQTT client");

    m_dispatcher.appendListener(MQTT_EVENT_CONNECTED, [this](auto event) {
        ESP_LOGI(s_tag, "MQTT connected");
        m_connected = 1;
    });

    m_dispatcher.appendListener(MQTT_EVENT_DISCONNECTED, [this](auto event) {
        m_connected = 0;
        ESP_LOGI(s_tag, "MQTT disconnected");
    });

    m_dispatcher.appendListener(MQTT_EVENT_SUBSCRIBED, [this](auto event) {
        ESP_LOGI(s_tag, "MQTT subscribed");
    });

    m_dispatcher.appendListener(MQTT_EVENT_UNSUBSCRIBED, [this](auto event) {
        ESP_LOGI(s_tag, "MQTT unsubscribed");
    });

    m_dispatcher.appendListener(MQTT_EVENT_PUBLISHED, [this](auto event) {
        ESP_LOGI(s_tag, "MQTT published");
    });

    m_dispatcher.appendListener(MQTT_EVENT_DATA, [this](auto event) {
        ESP_LOGI(s_tag,
            "MQTT data:\r\n"
            "\tTOPIC = %.*s\r\n"
            "\tDATA = %.*s\r\n",
            event->topic_len,
            event->topic,
            event->data_len,
            event->data);
    });

    m_dispatcher.appendListener(MQTT_EVENT_ERROR, [this](auto event) {
        ESP_LOGI(s_tag, "MQTT error");
    });
}

MQTT::~MQTT() {
    esp_mqtt_client_destroy(m_client);
}

void MQTT::subscribe(std::string_view topic) {
    int ret = esp_mqtt_client_subscribe(m_client, topic.data(), 2);
    if (ret < 0)
        ESP_LOGE(s_tag, "Error subscribing to topic: %s", topic.data());
}

void MQTT::subscribe(std::string_view topic, MQTT::Callback callback) {
    int ret = esp_mqtt_client_subscribe(m_client, topic.data(), 2);
    if (ret < 0)
        ESP_LOGE(s_tag, "Error subscribing to topic: %s", topic.data());

    m_dispatcher.appendListener(MQTT_EVENT_DATA, [=](esp_mqtt_event_handle_t event) {
        std::string_view received_topic(event->topic, event->topic_len);

        if (received_topic == topic) {
            callback(event);
        }
    });
}

void MQTT::unsubscribe(std::string_view topic) {
    int ret = esp_mqtt_client_unsubscribe(m_client, topic.data());
    if (ret < 0)
        ESP_LOGE(s_tag, "Error unsubscribing from topic: %s", topic.data());
}

void MQTT::publish(std::string_view topic, std::string_view message) {
    int ret = esp_mqtt_client_publish(m_client, topic.data(), message.data(), message.size(), 0, 0);
    if (ret < 0)
        ESP_LOGE(s_tag, "Error publishing message \"%s\" to topic: %s", message.data(), topic.data());
}

void MQTT::stop() {
    ESP_ERROR_CHECK(esp_mqtt_client_stop(m_client));
}

void MQTT::start() {
    ESP_LOGI(s_tag, "Starting MQTT client");
    ESP_ERROR_CHECK(esp_mqtt_client_start(m_client));
    ESP_ERROR_CHECK(esp_mqtt_client_register_event(m_client, static_cast<esp_mqtt_event_id_t>(ESP_EVENT_ANY_ID), trampoline, this));
}

void MQTT::on(EventID event, Callback callback) {
    m_dispatcher.appendListener(event, callback);
}

bool MQTT::connected() const {
    return m_connected;
}
