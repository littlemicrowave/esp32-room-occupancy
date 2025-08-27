#include "communication.h"
#include "esp_log.h"
static const char* TAG = "COMM";

#define RESOLVE_CODE(code) RESPONSE_CODE(code >> 5, code & 0x1F)

Communication* Communication::instance = nullptr;

Communication::Communication(const char *ssid, const char *pass, IPAddress coap_server_ip, int coap_port):
    ssid(ssid),
    pass(pass),
    coap_server(coap_server_ip), 
    coap_port(coap_port)
{ 
    instance = this;
    udp = new WiFiUDP();
    coap = new Coap(*udp);
}

void Communication::begin()
{
    WiFi.begin(ssid, pass);
    Serial.print("Connecting");
    while(WiFi.status() != WL_CONNECTED)
    {
        delay(100);
        Serial.print(".");
    }
    Serial.printf("\nWIFI Connected! SSID: %s\n", ssid);
    coap->response(&Communication::handleResponse);
    coap->start();
}

void Communication::update()
{
    coap->loop();
}


void Communication::sendData(const char* resource, Data* data)
{
    coap->send(coap_server, coap_port, resource, COAP_CON, COAP_POST, nullptr, 0, (uint8_t*)data, sizeof(Data));
}

void Communication::sendPrediction(const char* resource, Prediction* data)
{
    coap->send(coap_server, coap_port, resource, COAP_CON, COAP_POST, nullptr, 0, (uint8_t*)data, sizeof(Prediction));
}



void Communication::handleResponse(CoapPacket &packet, IPAddress ip, int port)
{
    uint8_t code = RESOLVE_CODE(packet.code);
    if(instance)
    {

        if(code == COAP_CREATED || code == COAP_CHANGED)
            Serial.println("Server Response: OK");
        else
            ESP_LOGE(TAG,"Server Response code: %d", code);
    }
}