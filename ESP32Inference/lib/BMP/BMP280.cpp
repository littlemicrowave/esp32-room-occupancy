#include "BMP280.h"
#include "esp_log.h"
static const char* TAG = "BMP280";


bool BMP280::begin(uint8_t address, TwoWire *wire, uint8_t config_data, uint8_t ctrl_meas_data)
{
  _wire = wire;
  _addr = address;
  char id = readId();
  if(id)
  {
      Serial.printf("Sensor chip ID: 0x%02x\n", id);
      if (!GetCalibrationValues())
      {
        ESP_LOGE(TAG, "Failed to read compensation value registers");
        return false;
      }
      SetConfig(config_data, ctrl_meas_data);
      return true;
  }
  return false; 
}

void BMP280::i2cScanner(TwoWire &wire)
{
  uint8_t error;
  for (uint8_t i = 1; i < 127; i++)
  {
    wire.beginTransmission(i);
    error = wire.endTransmission();
    if (error == 0)
      Serial.printf("Device at address: 0x%02x\n", i);
  }
}

int16_t BMP280::read16s_bigendian(bool init, uint8_t reg)
{
  if (init == false)  
    return (_wire->read()) | (_wire->read() << 8); //lsb first
  else
    if (reg)
    {
      _wire->beginTransmission(_addr);
      _wire->write(reg);
      if (!(_wire->endTransmission(false) == 0))
        return 0;
      if(_wire->requestFrom(_addr, (uint8_t)2) == 2)
        return (_wire->read()) | (_wire->read() << 8);
      return 0;
    }
  return 0;
}

int8_t BMP280::read8s(uint8_t reg)
{
    _wire->beginTransmission(_addr);
    _wire->write(reg);
    if(!_wire->endTransmission(false) == 0)
      return 0;
    if(!(_wire->requestFrom(_addr, (uint8_t)1) == 1))
      return 0;
    return _wire->read();
}

bool BMP280::write8u(uint8_t data, uint8_t reg)
{
    _wire->beginTransmission(_addr);
    _wire->write(reg);
    _wire->write(data);
    if(_wire->endTransmission() == 0)
      return true;
    return false;
}


bool BMP280::read(bool forced_mode)
{ 
  if (forced_mode)
  {
    SetOperationMode(FORCED);
    bool status = false;
    while(!status)
      if (!((uint8_t)read8s(STATUS_REG) & 0x08)) // 0x08 bit is 1 when measuring
        status = true;
  }
  _wire->beginTransmission(_addr);
  _wire->write(START);
  _wire->endTransmission(false);
  if(_wire->requestFrom(_addr, (uint8_t)6) != 6)
    return false;
  BMP280_S32_t adc_P = ((BMP280_S32_t)_wire->read() << 12) | ((BMP280_S32_t)_wire->read() << 4) | ((BMP280_S32_t)_wire->read() >> 4);
  BMP280_S32_t adc_T = ((BMP280_S32_t)_wire->read() << 12) | ((BMP280_S32_t)_wire->read() << 4) | ((BMP280_S32_t)_wire->read() >> 4);
  temperature = bmp280_compensate_T_int32(adc_T);
  pressure = bmp280_compensate_P_int32(adc_P);
  return true;
}

void BMP280::MPUToSleep(uint8_t MPU_ADDR)
{
  uint8_t reg_value;
  #define PWR_MGMT 0x6B
  reg_value = (uint8_t)read8s(PWR_MGMT);
  reg_value |= (1 << 6);
  write8u(reg_value, PWR_MGMT);
  Wire.endTransmission();
}


void BMP280::SetConfig(uint8_t config_data, uint8_t ctrl_meas_data)
{
    try
    {
      if (!write8u(config_data, CONFIG_REG))
        throw std::runtime_error("Failed to write config_data");
      delay(5);
      if (!write8u(ctrl_meas_data, CTRL_MEAS_REG))
        throw std::runtime_error("Failed to write ctrl_meas_data");
    }
    catch(const std::exception& e)
    {
      ESP_LOGE(TAG, "Exception: %s", e.what());
    }
    delay(5);
}


bool BMP280::GetCalibrationValues(void)
{
   int16_t *values[12] = {(int16_t*)(&dig_T1), &dig_T2, &dig_T3, (int16_t*)(&dig_P1), &dig_P2, &dig_P3, &dig_P4, &dig_P5, &dig_P6, &dig_P7, &dig_P8, &dig_P9}; //technically it is enough to use only dig_T1 address so write all the values, since they should be contignous in the memory.
   _wire->beginTransmission(_addr);
   _wire->write(COMPENSTATION_REG);
   _wire->endTransmission(false);
   if(_wire->requestFrom(_addr, (uint8_t)24) != 24)
    return false;
   for(int i = 0; i < 12; i++)
      *values[i] = read16s_bigendian(false);
   delay(5);
   return true;
}


#define ID_REG 0xD0
uint8_t BMP280::readId()
{
  return (uint8_t)read8s(ID_REG);
}

BMP280_S32_t BMP280::bmp280_compensate_T_int32(BMP280_S32_t adc_T)
{
    BMP280_S32_t var1, var2, T;
    var1 = ((((adc_T>>3) - ((BMP280_S32_t)dig_T1<<1))) * ((BMP280_S32_t)dig_T2)) >> 11;
    var2 = (((((adc_T>>4) - ((BMP280_S32_t)dig_T1)) * ((adc_T>>4) - ((BMP280_S32_t)dig_T1))) >> 12) * ((BMP280_S32_t)dig_T3)) >> 14;
    t_fine = var1 + var2;
    T = (t_fine * 5 + 128) >> 8;
    return T;
}

BMP280_S32_t BMP280::bmp280_compensate_P_int32(BMP280_S32_t adc_P)
{
    
    BMP280_S64_t var1, var2, p;
    var1 = ((BMP280_S64_t)t_fine) - 128000;
    var2 = var1 * var1 * (BMP280_S64_t)dig_P6;
    var2 = var2 + ((var1*(BMP280_S64_t)dig_P5)<<17);
    var2 = var2 + (((BMP280_S64_t)dig_P4)<<35);
    var1 = ((var1 * var1 * (BMP280_S64_t)dig_P3)>>8) + ((var1 * (BMP280_S64_t)dig_P2)<<12);
    var1 = (((((BMP280_S64_t)1)<<47)+var1))*((BMP280_S64_t)dig_P1)>>33;
    if (var1 == 0)
      return 0; // avoid exception caused by division by zero
    p = 1048576-adc_P;
    p = (((p<<31)-var2)*3125)/var1;
    var1 = (((BMP280_S64_t)dig_P9) * (p>>13) * (p>>13)) >> 25;
    var2 = (((BMP280_S64_t)dig_P8) * p) >> 19;
    p = ((p + var1 + var2) >> 8) + (((BMP280_S64_t)dig_P7)<<4);
    return (BMP280_U32_t)p;
}

bool BMP280::SetTempSampling(uint8_t mode)
{
  uint8_t reg_data = (uint8_t)read8s(CTRL_MEAS_REG);
  reg_data = ((reg_data << 3) >> 3) | (mode << 5);
  if(write8u(reg_data, CTRL_MEAS_REG))
    return true;
  return false;
}

bool BMP280::SetPressureSampling(uint8_t mode)
{
  uint8_t reg_data = (uint8_t)read8s(CTRL_MEAS_REG);
  reg_data = (reg_data & 0xE3) | (mode << 2);
  if(write8u(reg_data, CTRL_MEAS_REG))
    return true;
  return false;
}

bool BMP280::SetOperationMode(uint8_t mode)
{
  uint8_t reg_data = (uint8_t)read8s(CTRL_MEAS_REG);
  reg_data = ((reg_data >> 2) << 2) | mode;
  if(write8u(reg_data, CTRL_MEAS_REG))
    return true;
  return false;
}

bool BMP280::SetFilterCoef(uint8_t mode)
{
  uint8_t reg_data = (uint8_t)read8s(CONFIG_REG);
  reg_data = (reg_data & 0xE3) | (mode << 2);
  if(write8u(reg_data, CONFIG_REG))
    return true;
  return false;
}

bool BMP280::SetDelay(uint8_t mode)
{
  uint8_t reg_data = (uint8_t)read8s(CONFIG_REG);
  reg_data = ((reg_data << 3) >> 3) | (mode << 5);
  if(write8u(reg_data, CONFIG_REG))
    return true;
  return false;
}

bool BMP280::SetInterface(bool spi)
{
  uint8_t reg_data = (uint8_t)read8s(CONFIG_REG);
  if(spi)
    reg_data = ((reg_data >> 1) << 1) + SPI_ON;
  else
    reg_data = ((reg_data >> 1) << 1);
  if(write8u(reg_data, CONFIG_REG))
    return true;
  return false;
}