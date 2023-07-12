#include "Logic.hpp"
#include <iostream>

#include <rbwifi.h>

#include "MQTT.hpp"

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

#define MAZE 2

static constexpr const char* MQTT_HOST = "mqtt://192.168.42.21";
static constexpr const char* MQTT_TOPIC = "rb23/bludiste";

// clang-format off
static constexpr const uint8_t WALLS[100] = {
#if MAZE == 1
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 1, 1, 1, 1, 1, 1, 1, 1, 0,
    0, 0, 0, 0, 0, 1, 0, 0, 0, 0,
    1, 1, 1, 1, 0, 1, 0, 1, 1, 1,
    0, 0, 0, 0, 0, 1, 0, 0, 0, 0,
    0, 1, 1, 1, 1, 1, 1, 1, 0, 0,
    0, 0, 0, 0, 0, 1, 1, 1, 1, 1,
    1, 1, 1, 0, 2, 1, 0, 0, 0, 0,
#elif MAZE == 2
    0, 0, 0, 0, 0, 1, 0, 1, 0, 0,
    1, 1, 1, 1, 0, 1, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 1, 0, 1, 0, 0,
    0, 1, 1, 1, 1, 1, 0, 1, 1, 0,
    0, 1, 0, 0, 0, 0, 0, 1, 0, 0,
    0, 1, 1, 1, 0, 0, 1, 1, 0, 1,
    0, 0, 0, 0, 0, 1, 1, 0, 0, 0,
    0, 1, 1, 1, 1, 1, 1, 1, 1, 0,
    0, 0, 0, 0, 1, 0, 0, 0, 1, 0,
    1, 1, 1, 0, 1, 2, 1, 0, 0, 0
#endif
};
// clang-format on

#if MAZE == 1
static constexpr const char* WIN_TEXT = "BBGGRYYGBR";
#elif MAZE == 2
static constexpr const char* WIN_TEXT = "RYYBGRYBYG";
#endif

static int16_t gPlayerX = 0;
static int16_t gPlayerY = 0;

static std::atomic<bool> gConnected = false;

struct Move {
    int16_t dx;
    int16_t dy;
};

static void drawMaze() {
    int idx = 0;
    for (uint16_t y = 0; y < 10; ++y) {
        for (uint16_t x = 0; x < 10; ++x) {
            const auto val = WALLS[idx++];
            if (val == 1) {
                display.at(x, y) = Rgb(100, 100, 100);
            } else if (val == 2) {
                display.at(x, y) = Yellow;
            }
        }
    }
}

static bool movePlayer(int16_t dx, int16_t dy) {
    int16_t nx = gPlayerX + dx;
    int16_t ny = gPlayerY + dy;

    if (nx < 0 || nx >= 10 || ny < 0 || ny >= 10) {
        return false;
    }
    switch (WALLS[ny * 10 + nx]) {
    case 0:
        gPlayerX = nx;
        gPlayerY = ny;
        return false;
    case 1:
        return false;
    case 2:
        return true;
    }
    return false;
}

static void draw() {
    display.clear();
    if (gConnected) {
        display.at(gPlayerX, gPlayerY) = Green;
    } else {
        display.at(gPlayerX, gPlayerY) = Red;
    }
    drawMaze();
    display.show(50);
}

void logicMain() {
    draw();

    rb::WiFi::connect("RoboTabor", "RobotiNaVylete");
    rb::WiFi::waitForIp();

    auto moveQueue = xQueueCreate(16, sizeof(Move));

    MQTT mqtt(MQTT_HOST, "", "");

    mqtt.on(MQTT_EVENT_CONNECTED, [&](auto event) {
        ESP_LOGI("bludiste", "MQTT connected");
        gConnected = true;

        Move emptyMove = {};
        xQueueSend(moveQueue, &emptyMove, pdMS_TO_TICKS(500));

        mqtt.subscribe(MQTT_TOPIC, [&](esp_mqtt_event_handle_t event) {
            std::string cmd(event->data, event->data_len);

            Move move = {};
            if (cmd == "left") {
                move.dx = -1;
            } else if (cmd == "right") {
                move.dx = 1;
            } else if (cmd == "down") {
                move.dy = 1;
            } else if (cmd == "up") {
                move.dy = -1;
            } else {
                return;
            }

            xQueueSend(moveQueue, &move, pdMS_TO_TICKS(500));
        });
    });

    mqtt.on(MQTT_EVENT_DISCONNECTED, [&](auto event) {
        gConnected = false;
        Move emptyMove = {};
        xQueueSend(moveQueue, &emptyMove, pdMS_TO_TICKS(500));
    });

    mqtt.start();

    Move recvMove;
    while (true) {
        if (!xQueueReceive(moveQueue, &recvMove, pdMS_TO_TICKS(3000))) {
            draw();
            if (!mqtt.connected()) {
                mqtt.stop();
                mqtt.start();
            }
            continue;
        }

        if (movePlayer(recvMove.dx, recvMove.dy)) {
            break;
        }
        draw();
    }

    mqtt.stop();

    // Win condition
    {
        const int str_len = display.drawString(WIN_TEXT, Rgb(0, 0, 0), INT_MAX);

        while (true) {
            float offX = 10.f;
            float delta = -1;
            while (true) {
                display.clear();
                display.drawString(WIN_TEXT, Red, offX);
                display.show(30);

                offX += delta;
                if (offX <= -6 * str_len) {
                    offX = 8;
                }
                delay(200);
            }
        }
    }
}
