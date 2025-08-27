#include "button.h"

volatile bool button_update;

void IRAM_ATTR ButtonISR()
{
    if(!button_update) button_update = true;
}

void Button::startTimer() { timer = millis(); }

uint32_t Button::getTimer()
{
    uint32_t now = millis();
    if (timer == 0)
        return 0;
    return now - timer;
}


bool Button::wake_up(uint32_t time_to_hold)
{
    startTimer();
    while(getTimer() < time_to_hold)
    {
        if(digitalRead(pin) == 0)
            return false;
        delay(50);
    }
    timer = 0;
    return true;
}

Button::Button(uint8_t pin, uint32_t wake_up_delay) : pin(pin), timer(0), wake_up_delay(wake_up_delay)
{ 
    pinMode(pin, INPUT);
} 

void Button::system_start()
{
    esp_sleep_enable_ext0_wakeup((gpio_num_t)pin, 1);
    esp_sleep_wakeup_cause_t wake_up_reason = esp_sleep_get_wakeup_cause();
    if(wake_up_reason == ESP_SLEEP_WAKEUP_EXT0)
    {  
        if (!wake_up(wake_up_delay))
            esp_deep_sleep_start();
    }
    attachInterrupt(digitalPinToInterrupt(pin), &ButtonISR, CHANGE);
}

void Button::buttonCtrl(uint32_t click_time, bool *flag, uint32_t *calibration_counter)
{
    static uint8_t click_count = 0;
    if(button_update)
    {
        bool current_state = digitalRead(pin);
        if(current_state)
        {
            startTimer();
            static uint32_t last_click = timer;
            if(timer - last_click <= click_time)
                click_count++;
            else 
                click_count = 1;
            last_click = timer;
        }
        else
        {
            uint32_t timer_val = getTimer();
            if(timer_val >= 1000)
            {
                digitalWrite(1, HIGH);
                Serial.println("Goining to sleep....");
                esp_deep_sleep_start();
            }
            if(click_count == 2)
            {
                if(*flag == false)
                {
                    Serial.println("Inference: on");
                    *flag = true;
                    *calibration_counter = 0;
                }
                else 
                {
                    Serial.println("Inference: off, Collecting the data...");
                    *flag = false;
                }
                click_count = 0;
            }
            timer = 0;
        }
        button_update = false;
    }
}