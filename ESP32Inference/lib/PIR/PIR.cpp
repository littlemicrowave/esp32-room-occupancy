#include "PIR.h"

void PIR::update()
{
    static uint32_t last_call = millis();

    uint32_t now = millis();
    uint32_t elapsed_time = now - last_call;
    last_call = now;
    
    if (digitalRead(pin) == HIGH)
        accumulated_uptime += elapsed_time;
}

u_int32_t PIR::read()
{
    u_int32_t uptime = accumulated_uptime;
    accumulated_uptime = 0;
    return uptime;
}


