#include <Arduino.h>
#include <driver/rmt.h>



class DHT {
    uint8_t pin;
    float temperature;
    float humidity;
    int last_read = -1;
    rmt_channel_t rx_channel;
    RingbufHandle_t rxBuffer;
    RingbufHandle_t init_rx_channel(rmt_channel_t channel);
    bool recieve_and_decode();
    void start();
    public:
        DHT(uint8_t dht_pin, rmt_channel_t channel = RMT_CHANNEL_0);
        ~DHT() { if(rxBuffer) rmt_driver_uninstall(rx_channel); }
        bool read();
        float getTemperature() {return temperature;}
        float getHumidity() {return humidity;}
};