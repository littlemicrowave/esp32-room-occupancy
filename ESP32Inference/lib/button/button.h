#include <Arduino.h>
#include <esp_sleep.h>

class Button {
    uint32_t wake_up_delay;
    uint32_t timer;
    uint8_t pin;
    public:
        Button(uint8_t pin, uint32_t wake_up_delay);
        void system_start();
        void startTimer();
        uint32_t getTimer();
        bool wake_up(uint32_t time_to_hold);
        void buttonCtrl(uint32_t click_time, bool *flag, uint32_t *calibration_counter);
};