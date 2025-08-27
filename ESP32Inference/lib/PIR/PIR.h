#pragma once
#include <Arduino.h>

class PIR{
    u_int32_t accumulated_uptime;
    uint8_t pin;
    public:
        PIR(uint8_t pin): pin(pin), accumulated_uptime(0) {
             pinMode(pin, INPUT);
            }
        u_int32_t read();
        void update();
};

