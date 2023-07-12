#include "Logic.hpp"
#include <iostream>

#include <atomic>
#include <rbwifi.h>

#include "MQTT.hpp"

static constexpr const char* MQTT_HOST = "mqtt://192.168.42.21";
static constexpr const char* MQTT_TOPIC = "rb23/bludiste";

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

static std::atomic<bool> gConnected = false;

static void showDisconnected() {
    display.clear();
    display.drawCharacter('X', Red, 3, 0);
    display.show(50);
}

static void showConnected() {
    display.clear();
    display.drawLine(2, 6, 7, 6, Green);
    display.show(50);
}

void logicMain() {
    showDisconnected();

    rb::WiFi::connect("RoboTabor", "RobotiNaVylete");
    rb::WiFi::waitForIp();

    uint32_t next_connected_check = millis() + 5000;
    std::atomic<uint32_t> next_display_clear = 0;

    MQTT mqtt(MQTT_HOST, "", "");

    mqtt.on(MQTT_EVENT_CONNECTED, [&](auto event) {
        ESP_LOGI("bludiste", "MQTT connected");
        gConnected = true;
        showConnected();
    });

    mqtt.on(MQTT_EVENT_DISCONNECTED, [&](auto event) {
        gConnected = false;
        showDisconnected();
    });

    mqtt.start();

    buttons.onPress([&]() {
        if (!gConnected) {
            return;
        }
        mqtt.publish(MQTT_TOPIC, "left");

        display.clear();
        display.drawSquareFilled(0, 3, 4, Green);
        display.show(30);
        next_display_clear = millis() + 300;
    },
        Left);

    buttons.onPress([&]() {
        if (!gConnected) {
            return;
        }
        mqtt.publish(MQTT_TOPIC, "right");

        display.clear();
        display.drawSquareFilled(6, 3, 4, Red);
        display.show(30);
        next_display_clear = millis() + 300;
    },
        Right);

    buttons.onPress([&]() {
        if (!gConnected) {
            return;
        }
        mqtt.publish(MQTT_TOPIC, "up");

        display.clear();
        display.drawSquareFilled(3, 0, 4, Blue);
        display.show(30);
        next_display_clear = millis() + 300;
    },
        Up);

    buttons.onPress([&]() {
        if (!gConnected) {
            return;
        }
        mqtt.publish(MQTT_TOPIC, "down");

        display.clear();
        display.drawSquareFilled(3, 6, 4, Purple);
        display.show(30);
        next_display_clear = millis() + 300;
    },
        Down);

    while (true) {
        if (millis() > next_connected_check) {
            if (!mqtt.connected()) {
                mqtt.stop();
                mqtt.start();
            }
            next_connected_check = millis() + 5000;
        }

        if (next_display_clear != 0 && millis() > next_display_clear) {
            if (gConnected) {
                showConnected();
            }
            next_display_clear = 0;
        }

        delay(100);
    }
}
