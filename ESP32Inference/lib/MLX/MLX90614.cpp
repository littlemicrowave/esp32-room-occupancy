#include "MLX90614.h"

MLX90614::~MLX90614() { _wire->end(); }

/**
 * @brief Begin the I2C connection
 * @param addr I2C address for the device.
 * @param wire Reference to Wire instance
 * @return True if the device was successfully initialized, otherwise false.
 */
bool MLX90614::begin(uint8_t addr, TwoWire *wire) {
  _addr = addr; // needed for CRC
  _wire = wire;
  uint16_t id = read16(0x3E);
  if(!id)
    return false;
  Serial.printf("MLX Chip ID: 0x%04x\n", id);
  return true;
}

/**
 * @brief Read the raw value from the emissivity register
 *
 * @return uint16_t The unscaled emissivity value or '0' if reading failed
 */
uint16_t MLX90614::readEmissivityReg(void) {
  return read16(MLX90614_EMISS);
}
/**
 * @brief Write the raw unscaled emissivity value to the emissivity register
 *
 * @param ereg The unscaled emissivity value
 */
void MLX90614::writeEmissivityReg(uint16_t ereg) {
  write16(MLX90614_EMISS, 0); // erase
  delay(10);
  write16(MLX90614_EMISS, ereg);
  delay(10);
}
/**
 * @brief Read the emissivity value from the sensor's register and scale
 *
 * @return double The emissivity value, ranging from 0.1 - 1.0 or NAN if reading
 * failed
 */
double MLX90614::readEmissivity(void) {
  uint16_t ereg = read16(MLX90614_EMISS);
  if (ereg == 0)
    return NAN;
  return ((double)ereg) / 65535.0;
}
/**
 * @brief Set the emissivity value
 *
 * @param emissivity The emissivity value to use, between 0.1 and 1.0
 */
void MLX90614::writeEmissivity(double emissivity) {
  uint16_t ereg = (uint16_t)(0xffff * emissivity);

  writeEmissivityReg(ereg);
}

/**
 * @brief Get the current temperature of an object in degrees Farenheit
 *
 * @return double The temperature in degrees Farenheit or NAN if reading failed
 */
double MLX90614::readObjectTempF(void) {
  return (readTemp(MLX90614_TOBJ1) * 9 / 5) + 32;
}
/**
 * @brief Get the current ambient temperature in degrees Farenheit
 *
 * @return double The temperature in degrees Farenheit or NAN if reading failed
 */
double MLX90614::readAmbientTempF(void) {
  return (readTemp(MLX90614_TA) * 9 / 5) + 32;
}

/**
 * @brief Get the current temperature of an object in degrees Celcius
 *
 * @return double The temperature in degrees Celcius or NAN if reading failed
 */
double MLX90614::readObjectTempC(void) {
  return readTemp(MLX90614_TOBJ1);
}

/**
 * @brief Get the current ambient temperature in degrees Celcius
 *
 * @return double The temperature in degrees Celcius or NAN if reading failed
 */
double MLX90614::readAmbientTempC(void) {
  return readTemp(MLX90614_TA);
}

float MLX90614::readTemp(uint8_t reg) {
  float temp;

  temp = read16(reg);
  if (temp == 0)
    return NAN;
  temp *= .02;
  temp -= 273.15;
  return temp;
}

/*********************************************************************/

uint16_t MLX90614::read16(uint8_t a) {
  uint8_t buffer[2];
  buffer[0] = a;
  _wire->beginTransmission(_addr);
  _wire->write(buffer[0]);
  _wire->endTransmission(false);
  if(_wire->requestFrom(_addr, (uint8_t) 2) != 2)
    return 0;
  buffer[0] = _wire->read();
  buffer[1] = _wire->read();
  return uint16_t(buffer[0]) | (uint16_t(buffer[1]) << 8);
}

byte MLX90614::crc8(byte *addr, byte len)
// The PEC calculation includes all bits except the START, REPEATED START, STOP,
// ACK, and NACK bits. The PEC is a CRC-8 with polynomial X8+X2+X1+1.
{
  byte crc = 0;
  while (len--) {
    byte inbyte = *addr++;
    for (byte i = 8; i; i--) {
      byte carry = (crc ^ inbyte) & 0x80;
      crc <<= 1;
      if (carry)
        crc ^= 0x7;
      inbyte <<= 1;
    }
  }
  return crc;
}

void MLX90614::write16(uint8_t a, uint16_t v) {
  uint8_t buffer[4];

  buffer[0] = _addr << 1;
  buffer[1] = a;
  buffer[2] = v & 0xff;
  buffer[3] = v >> 8;

  uint8_t pec = crc8(buffer, 4);
  _wire->beginTransmission(_addr);
  _wire->write(a);
  _wire->write(v & 0xff);
  _wire->write(v >> 8);
  _wire->write(pec);
  _wire->endTransmission();
}

void MLX90614::sleep(void){
  byte crc = crc8(0, _addr << 1);
  crc = crc8(&crc, SLEEP_CODE);
  _wire->beginTransmission(_addr);
  _wire->write(SLEEP_CODE);
  _wire->write(crc);
  _wire->endTransmission();
  _wire->end();
}

void MLX90614::awake(uint8_t SCL_PIN){
  pinMode(SCL_PIN, OUTPUT);
  digitalWrite(SCL_PIN, LOW);
  delay(40);
  pinMode(SCL_PIN, INPUT);  // let Wire control it
  _wire->begin();             // reinitialize I2C
  delay(5);
}