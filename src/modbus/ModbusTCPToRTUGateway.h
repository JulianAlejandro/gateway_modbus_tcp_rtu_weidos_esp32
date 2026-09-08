#ifndef _MODBUS_TCP_TO_RTU_GATEWAY_H_INCLUDED
#define _MODBUS_TCP_TO_RTU_GATEWAY_H_INCLUDED

#include <Arduino.h>
#include "ModbusTCPServer.h"
#include <ArduinoModbus.h>

extern "C" {
  #include "libmodbus/modbus.h"
}

class ModbusTCPToRTUGateway : public ModbusTCPServer {
public:
  ModbusTCPToRTUGateway();
  virtual ~ModbusTCPToRTUGateway();

  int setClient(ModbusClient* client);
  int configureCoils(int startAddress, int nb);
  int configureDiscreteInputs(int startAddress, int nb);
  int configureHoldingRegisters(int startAddress, int nb);
  int configureInputRegisters(int startAddress, int nb);

  virtual void poll() override;

private:
  ModbusClient* _rtuClient;

  bool handleReadCoils(uint8_t unitId, uint16_t address, uint16_t quantity);
  bool handleReadDiscreteInputs(uint8_t unitId, uint16_t address, uint16_t quantity);
  bool handleReadHoldingRegisters(uint8_t unitId, uint16_t address, uint16_t quantity);
  bool handleReadInputRegisters(uint8_t unitId, uint16_t address, uint16_t quantity);
  bool handleWriteSingleCoil(uint8_t unitId, uint16_t address, uint16_t value);
  bool handleWriteSingleRegister(uint8_t unitId, uint16_t address, uint16_t value);
  bool handleWriteMultipleCoils(uint8_t unitId, uint16_t address, uint16_t quantity, const uint8_t* data, int dataLen);
  bool handleWriteMultipleRegisters(uint8_t unitId, uint16_t address, uint16_t quantity, const uint8_t* data, int dataLen);

  void copyBitsToMapping(modbus_mapping_t* m, uint16_t address, uint16_t quantity, const uint8_t* values);
  void copyRegistersToMapping(modbus_mapping_t* m, uint16_t address, uint16_t quantity, const uint16_t* values);
};

#endif
