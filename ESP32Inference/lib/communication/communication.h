#pragma once
#include <WiFi.h>
#include <WiFiUdp.h>
#include "coap-simple.h"

typedef struct {

    uint16_t co2_ppm;
    uint16_t tvoc_ppm;
    float bmp280_temperature;
    float bmp280_pressure;
    float mlx_object_temperature;
    float mlx_ambient_temperature;
    float humidity_dht;
    float temperature_dht;
    float pir_uptime;

    void print() 
    {
        Serial.printf("Temperature (BMP280): %.2f degC\n", bmp280_temperature);
        Serial.printf("Pressure (BMP280): %.3f Pa\n", bmp280_pressure);
        Serial.printf("Object Temperature (MLX IR): %.3f degC\n", mlx_object_temperature);
        Serial.printf("Ambient Temperature (MLX IR): %.3f degC\n", mlx_ambient_temperature);
        Serial.printf("Humidity (DHT): %.2f %\n", humidity_dht);
        Serial.printf("Temperature (DHT): %.2f degC\n", temperature_dht);
        Serial.printf("PIR last uptime: %.2f\n", pir_uptime);
    }
} __attribute__((packed)) Data;

typedef struct {
    float human_count = 0;
    int32_t ventilation_tag = 1;
} __attribute__((packed)) Prediction;

class Communication {

    const char* ssid;
    const char* pass;
    IPAddress coap_server;
    const int coap_port;
    WiFiUDP *udp;
    Coap *coap;

    static Communication* instance;

    public:
        Communication(const char* ssid, const char* pass, IPAddress coap_server_ip, int coap_port);
        ~Communication() { delete udp; WiFi.disconnect(); delete coap; }
        static void handleResponse(CoapPacket &packet, IPAddress ip, int port);
        void begin();
        void update();
        void sendData(const char* resource, Data* data);
        void sendPrediction(const char* resource, Prediction* data);
};