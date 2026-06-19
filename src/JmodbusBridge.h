#ifndef MODBUS_BRIDGE_H
#define MODBUS_BRIDGE_H

#include <Arduino.h>

#define SIZE_MB_TCP_REQUEST 12 
#define SIZE_MB_RTU_REQUEST 8

// Estructuras de datos limpias para el paso de información
struct modbusTCPStruct {
  uint16_t transactionID;
  uint16_t protocolID;
  uint16_t length;
  uint8_t  slaveID;
  uint8_t  functionCode;
  uint16_t address;
  uint16_t quantity;
  bool     isValid;
};

struct modbusRTUStruct {
  uint8_t  slaveID;
  uint8_t  functionCode;
  uint16_t address;
  uint16_t quantity;
  uint16_t crc;
  bool     isValid;
};

// Utilidades de conversión y mapeo
bool parseTCPBufferToStruct(const byte* tcp_buf, modbusTCPStruct* out_struct);
int getModbusClientDataType(uint8_t functionCode);

#endif
