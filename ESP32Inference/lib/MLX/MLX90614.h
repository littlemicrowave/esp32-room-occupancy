/***************************************************
  Written by Limor Fried/Ladyada for Adafruit in any redistribution
 ****************************************************/

#include <Wire.h>
#include <Arduino.h>

#define MLX90614_I2CADDR 0x5A

// RAM
#define MLX90614_RAWIR1 0x04
#define MLX90614_RAWIR2 0x05
#define MLX90614_TA 0x06
#define MLX90614_TOBJ1 0x07
#define MLX90614_TOBJ2 0x08
// EEPROM
#define MLX90614_TOMAX 0x20
#define MLX90614_TOMIN 0x21
#define MLX90614_PWMCTRL 0x22
#define MLX90614_TARANGE 0x23
#define MLX90614_EMISS 0x24
#define MLX90614_CONFIG 0x25
#define MLX90614_ADDR 0x2E
#define MLX90614_ID1 0x3C
#define MLX90614_ID2 0x3D
#define MLX90614_ID3 0x3E
#define MLX90614_ID4 0x3F
#define SLEEP_CODE 0xFF
/**
 * @brief Class to read from and control a MLX90614 Temp Sensor
 *
 */
class MLX90614 {
public:

  ~MLX90614();
  bool begin(uint8_t addr, TwoWire *wire);
  double readObjectTempC(void);
  double readAmbientTempC(void);
  double readObjectTempF(void);
  double readAmbientTempF(void);
  uint16_t readEmissivityReg(void);
  void writeEmissivityReg(uint16_t ereg);
  double readEmissivity(void);
  void writeEmissivity(double emissivity);
  void sleep(void);
  void awake(uint8_t SCL_PIN);

private:
  
  float readTemp(uint8_t reg);
  TwoWire *_wire;
  uint16_t read16(uint8_t addr);
  void write16(uint8_t addr, uint16_t data);
  byte crc8(byte *addr, byte len);
  uint8_t _addr;
};
