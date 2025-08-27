#include <Wire.h>
#include <Arduino.h>

typedef int32_t BMP280_S32_t;
typedef uint32_t BMP280_U32_t;
typedef int64_t BMP280_S64_t;

#define X0 0 // 0 coef, 0.5ms, Filter 0
#define X1 1 // 1 coef, 62.5ms, Filter 2
#define X2 2 // 2 coef, 125ms, Filter 4
#define X4 3 // 4 coef, 250ms, Filter 8
#define X8 4 // 8 coef, 500ms, Filter 16
#define X16 5 // 16 coef, 1000ms
#define X32 6 // 2000ms
#define X64 7 // 4000ms
#define SLEEP 0
#define NORM 3
#define FORCED 1
#define SPI_OFF 0
#define SPI_ON 1
#define CONFIG_REG 0xF5 // Timing[8-6], IIRFilter[5-3], SPI[1]
#define CTRL_MEAS_REG 0xF4 // osrs_t[8-6], osrs_p[5-3], Mode[1]
#define START 0xF7
#define STATUS_REG 0xF3
#define COMPENSTATION_REG 0x88

namespace ConfigPresets{

    struct {
            const uint8_t config = ((X1 << 5) | (X2 << 2) | SPI_OFF);
            const uint8_t ctrl_meas = ((X2 << 5) | (X16 << 2) | NORM);
            } HandheldDevice_LowPower;

    struct {
            const uint8_t config = ((X0 << 5) | (X8 << 2) | SPI_OFF);
            const uint8_t ctrl_meas = ((X1 << 5) | (X4 << 2) | NORM);
            } HandheldDevice_Dynamic;

    struct {
            const uint8_t  config = ((X0 << 5) | (X0 << 2) | SPI_OFF);
            const uint8_t ctrl_meas = ((X1 << 5) | (X1 << 2) | SLEEP);
            } Weather_Monitoring;

    struct {
            const uint8_t config = ((X4 << 5) | (X2 << 2) | SPI_OFF);
            const uint8_t ctrl_meas = ((X1 << 5) | (X4 << 2) | NORM);
            } ElevatorFloor_ChangeDetection;

    struct {
            const uint8_t config = ((X0 << 5) | (X0 << 2) | SPI_OFF);
            const uint8_t ctrl_meas = ((X1 << 5) | (X2 << 2) | NORM);
            } DropDetection;

    struct {
            const uint8_t config = ((X0 << 5) | (X8 << 2) | SPI_OFF);
            const uint8_t ctrl_meas = ((X2 << 5) | (X16 << 2) | NORM);
            } IndoorNavigation;
}

class BMP280{
    private:
    uint8_t _addr;
    TwoWire *_wire;
    BMP280_S32_t t_fine;
    uint16_t dig_T1, dig_P1;
    int16_t dig_T2, dig_T3, dig_P2, dig_P3, dig_P4, dig_P5, dig_P6, dig_P7, dig_P8, dig_P9;
    int16_t read16s_bigendian(bool init, uint8_t reg = 0x00);
    int8_t read8s(uint8_t reg);
    bool write8u(uint8_t data, uint8_t reg);
    bool GetCalibrationValues();
    uint8_t readId();
    BMP280_S32_t bmp280_compensate_T_int32(BMP280_S32_t adc_T);
    BMP280_S32_t bmp280_compensate_P_int32(BMP280_S32_t adc_P);
    int32_t temperature;
    int32_t pressure;

    public: 
        ~BMP280() { _wire->end(); }
        void SetConfig(uint8_t config_data, uint8_t ctrl_meas_data);
        bool begin(uint8_t address, TwoWire *wire, uint8_t config, uint8_t ctrl_meas);
        void i2cScanner(TwoWire &wire);
        bool SetTempSampling(uint8_t mode);
        bool SetPressureSampling(uint8_t mode);
        bool SetFilterCoef(uint8_t mode);
        bool SetDelay(uint8_t mode);
        bool SetInterface(bool spi);
        bool SetOperationMode(uint8_t mode);
        bool read(bool forced_mode);
        float getTemperature() { return (float)temperature/100; }
        double getPressure() { return (double)pressure/256; }
        void MPUToSleep(uint8_t MPU_ADDR);
};