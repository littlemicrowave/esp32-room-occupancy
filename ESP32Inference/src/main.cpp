#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <freertos/task.h>
#include "BMP280.h"
#include "MLX90614.h"
#include "CCS811.h"
#include "DHT.h"
#include "PIR.h"
#include "communication.h"
#include "button.h"
#include "infer.h"

static const char* TAG = "main";

#define SDA 25
#define SCL 26
#define DHT_PIN 32
#define PIR_PIN 33
#define BUTTON_PIN 4

//Pripheral addresses for i2c
#define BMP_ADDR 0x76
#define MPU_ADDR 0x68
#define MLX_ADDR 0x5A
#define CCS_ADDR 0x5B

//Wake up pin for CCS sensor
#define NWAKE 27
#define POLL_INVERVAL 10000
#define TIME_TO_WAKEUP 1000

//Networking
#define WIFI_SSID "*********"
#define WIFI_PASS "*********"
#define COAP_IP IPAddress(192,168,1,178) //192.168.1.178:5683
#define COAP_PORT 5683

BMP280 BMP;
MLX90614 MLX;
CCS811 CCS(NWAKE, CCS_ADDR);
DHT DHT11(DHT_PIN);
PIR _PIR(PIR_PIN);
Communication comm(WIFI_SSID, WIFI_PASS, COAP_IP, COAP_PORT);
Button button(BUTTON_PIN, TIME_TO_WAKEUP);
Inference model;
SensorDataBatch data_pointer_array;

#define DATA_SET 1 << 0
#define PREDICTION_READY 1 << 1 
EventGroupHandle_t events;
//float *human_counts[BATCH_SIZE];
//int32_t *ventilation_tags[BATCH_SIZE];

void run_model(void*); //inference process

void setup() {
  button.system_start();
  Serial.begin(115200);
  Serial.println("System is starting...");
  Wire.begin(SDA, SCL);
  comm.begin();
  model.GetInputBuffers();
  events = xEventGroupCreate();
  xEventGroupClearBits(events, (DATA_SET) | (PREDICTION_READY));

  BMP.i2cScanner(Wire); //discovering the devices

  if(!BMP.begin(BMP_ADDR, &Wire, ConfigPresets::ElevatorFloor_ChangeDetection.config, ConfigPresets::ElevatorFloor_ChangeDetection.ctrl_meas))  //inits sensor configuration, wakes it up
    ESP_LOGE(TAG, "Failed to init BMP280");
  else
    BMP.MPUToSleep(MPU_ADDR); //disabling MPU sensor on GY-91 board (don't neeed it)

  if(!MLX.begin(MLX_ADDR, &Wire))
    ESP_LOGE(TAG, "Failed to init MLX90614"); //tests the connection obtains id.

  if(!CCS.begin())
    ESP_LOGE(TAG, "Failed to init the CSS811 sensor");
  else
    CCS.start(CCS811_MODE_1SEC);
  delay(10);
  
  //Allocation of pointers
  for (int i = 0; i < BATCH_SIZE; i++) 
    for (int k = 0; k < SEQUENCE_LENGTH; k++)
        data_pointer_array[i][k] = new Data();

  xTaskCreate(&run_model,"Inference", 2048, nullptr, 5, nullptr); //creating inference process thread
}


Data data;
uint16_t ccs_stat;
uint32_t last_poll = 0;
uint32_t calibration_counter = 0;
bool inference_mode = false;


void loop() {
  uint32_t now = millis();
  if (last_poll + POLL_INVERVAL <= now)
  {
    Serial.printf("--------------------------\n");

    if(!BMP.read(false))
      ESP_LOGE(TAG, "BMP280 SENSOR ERROR");

    if(!DHT11.read())
      ESP_LOGE(TAG, "DHT SENSOR ERROR");

    CCS.read(&data.co2_ppm, &data.tvoc_ppm, &ccs_stat, nullptr);
    if(ccs_stat == (CCS811_ERRSTAT_FW_MODE | CCS811_ERRSTAT_APP_VALID |CCS811_ERRSTAT_DATA_READY))
      Serial.printf("CO2: %d ppm, TVOC: %d ppm\n", data.co2_ppm, data.tvoc_ppm);
    else if (!(ccs_stat & CCS811_ERRSTAT_DATA_READY))
      ESP_LOGE(TAG, "CSS811 DATA NOT READY ERROR: %s", CCS.errstat_str(ccs_stat));
    else
      ESP_LOGE(TAG, "CSS811 ERROR: %s", CCS.errstat_str(ccs_stat));

    data.bmp280_temperature = BMP.getTemperature();
    data.bmp280_pressure = BMP.getPressure();
    data.mlx_ambient_temperature = MLX.readAmbientTempC();
    data.mlx_object_temperature = MLX.readObjectTempC();
    data.humidity_dht = DHT11.getHumidity();
    data.temperature_dht = DHT11.getTemperature();
    data.pir_uptime = (float)_PIR.read()/1000;
    last_poll = millis();
    data.print();
    if(!inference_mode)
      comm.sendData("data", &data);
    else
    {
      if(calibration_counter < SEQUENCE_LENGTH)
      {
        *(data_pointer_array[0][calibration_counter]) = data;
        if(calibration_counter == (SEQUENCE_LENGTH - 1))
        {
            model.SetDefaultLabels(0, 0);
            xEventGroupSetBits(events, DATA_SET);
            Serial.println("Calibration data ready.");
        }
        calibration_counter++;
      }
      else
        {
          uint8_t model_state = xEventGroupGetBits(events);
          if(model_state & PREDICTION_READY)
          {
            xEventGroupClearBits(events, PREDICTION_READY);
            *(data_pointer_array[0][SEQUENCE_LENGTH - 1]) = data;
            Prediction pred = model.GetRecentPrediction();
            xEventGroupSetBits(events, DATA_SET);
            Serial.printf("Human count: %.2f, Ventilation: %d\n", pred.human_count, pred.ventilation_tag);
            comm.sendPrediction("predictions", &pred);
          }
        }
    }
  }
  _PIR.update();
  comm.update();
  button.buttonCtrl(250, &inference_mode, &calibration_counter);
}


void run_model(void*)
{
    while(true)
    {
      xEventGroupWaitBits(events, DATA_SET, pdTRUE, pdTRUE, portMAX_DELAY);
      model.SetSequences(data_pointer_array);
      model.PrintBuffers();
      model.ComputeSensorDeltas();
      model.ScaleData();
      model.SetInputBuffers();
      if(!model.Predict())
      {
        ESP_LOGE(TAG, "Inference error, process is aborted.");
        break;
      }
      model.ShiftSequences(data_pointer_array);
      Serial.println("Prediction ready.");
      xEventGroupSetBits(events, PREDICTION_READY);
    }
}
