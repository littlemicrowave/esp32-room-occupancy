#include "DHT.h"
#define WAKE_UP_DELAY 20//in microseconds (20ms)
#define MICROSECONDS_TO_ABP_TICKS(ms) ms*80
#define SENSOR_TIMEOUT_MS 2000


void DHT::start()
{
    pinMode(pin, OUTPUT);
    digitalWrite(pin, LOW);
    delay(WAKE_UP_DELAY);
    pinMode(pin, INPUT);
    delayMicroseconds(30);
}

DHT::DHT(uint8_t dht_pin, rmt_channel_t channel): pin(dht_pin), rx_channel(channel) { rxBuffer = init_rx_channel(channel); }

RingbufHandle_t DHT::init_rx_channel(rmt_channel_t channel)
{
    rmt_config_t config = RMT_DEFAULT_CONFIG_RX((gpio_num_t)pin, channel);
    config.clk_div = MICROSECONDS_TO_ABP_TICKS(1); // 80 ticks 
    config.mem_block_num = 1; //64 int sized structs
    config.rx_config.idle_threshold = 1000; //1 millisec to become idle
    config.rx_config.filter_ticks_thresh = 10; //less than 10 micros ignored

    if(!(rmt_config(&config) == ESP_OK))
    {
        Serial.println("Error setting RMT config.\n");
        return nullptr;
    }

    if(!(rmt_driver_install(channel, 350, 0) == ESP_OK)) //Channel 0, buffer size 350 bytes, default interrupts.
    {
        Serial.println("Error in driver installation.\n");
        return nullptr;
    }

    RingbufHandle_t rb = NULL;
    if(rmt_get_ringbuf_handle(channel, &rb) == ESP_OK)
        return rb;
    return nullptr;
}

bool DHT::recieve_and_decode()
{
    if (rxBuffer == nullptr)
        return false;
    
    rmt_rx_memory_reset(rx_channel);
    start();
    rmt_rx_start(rx_channel,true);
    byte data[5] = {0};
    size_t buf_size = 0;
    rmt_item32_s *items = (rmt_item32_s*)xRingbufferReceive(rxBuffer, &buf_size, (TickType_t)pdMS_TO_TICKS(15)); //wait at most for 15 ms
    rmt_rx_stop(rx_channel);

    buf_size = buf_size / 4;
    if(buf_size != 41 || items == nullptr) {
        Serial.printf("Incorrect buffersize: %d\n", buf_size);
        if(buf_size != 0)
            vRingbufferReturnItem(rxBuffer, items);
        return false;
    }

    for (int i=1; i < 41; i++) // ignore the first one (sensor's response low+high for start signal)
    { 
        data[(i-1) / 8] <<= 1;
        data[(i-1) / 8] |= (items[i].duration0) < 33 ? 0 : 1; //might need to be adjusted in case of bugs
    }

    vRingbufferReturnItem(rxBuffer, items);

    if (!(data[4] == ((data[0] + data[1] + data[2] + data[3]) & 0xFF))) {
        Serial.printf("Checksum mismatch\n", buf_size);
        return false;
    }

    humidity = data[0] + data[1] * 0.1;
    temperature = data[2];
    if (data[3] & 0x80) {
        temperature = -1 - temperature;
      }
    temperature += (data[3] & 0x0f) * 0.1;
    return true;
}

bool DHT::read()
{
    int current_time = millis();
    if(current_time - last_read > SENSOR_TIMEOUT_MS)
       {
        last_read = current_time;
        if(recieve_and_decode())
            return true;
       }
    return false;
}
