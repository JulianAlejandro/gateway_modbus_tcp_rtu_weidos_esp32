#ifndef COMMON_MODBUS_BRIDGE_H
#define COMMON_MODBUS_BRIDGE_H

#include <Arduino.h>

#define SIZE_MB_TCP_REQUEST 12 
#define SIZE_MB_RTU_REQUEST 8

// Estructuras de datos limpias para el paso de información
struct modbusTCPStruct {
  //MBAP 7 bytes
  uint16_t transactionID;
  uint16_t protocolID;
  uint16_t length;
  uint8_t  slaveID;
  // PDU
  uint8_t  functionCode;
  uint16_t address;
  uint16_t quantity;

  bool     isValid; // Esto aqui puede sobrar 
};

struct modbusRTUStruct {
  uint8_t  slaveID;
  uint8_t  functionCode;
  uint16_t address;
  uint16_t quantity;

  uint16_t crc; // esto no lo usamos 
  bool     isValid;
};

#endif
