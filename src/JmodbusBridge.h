
#ifndef MODBUS_BRIDGE_H
#define MODBUS_BRIDGE_H

#include <Arduino.h>
#include "ModbusClient.h"

//---------------------------------MOSBUD TCP REQUEST--------------------
#define SIZE_MB_TCP_REQUEST 12 

struct modbusTCPStruct {
  uint16_t transactionID; // ID de Transacción (2 bytes). Si es petición interna, puedes usar 0xFFFF
  uint16_t protocolID;    // ID de Protocolo (2 bytes). Siempre es 0x0000 para Modbus TCP
  uint16_t length;        // Longitud de los bytes restantes (2 bytes). Típicamente 6 para lecturas
  uint8_t  slaveID;       // ID del Esclavo / Unit Identifier (1 byte)
  uint8_t  functionCode;  // Código de función (1 byte) -> Ej: 0x03 o 0x04
  uint16_t address;       // Dirección inicial del registro (2 bytes)
  uint16_t quantity;      // Cantidad de registros a leer (2 bytes)
  
  bool     isValid;       // Flag auxiliar para saber si la estructura contiene datos válidos
};

//byte modbusTCPBuffer[12] = {0}; 

//----------------------------------MODBUS RTU REQUEST-----------------------
// Una petición de lectura Modbus RTU mide exactamente 8 bytes:
// [1 byte ID] + [1 byte Func] + [2 bytes Address] + [2 bytes Quantity] + [2 bytes CRC]
#define SIZE_MB_RTU_REQUEST 8

struct modbusRTUStruct {
  uint8_t  slaveID;      // ID del Esclavo (1 byte)
  uint8_t  functionCode; // Código de función (0x03 o 0x04)
  uint16_t address;      // Dirección inicial del registro (2 bytes)
  uint16_t quantity;     // Cantidad de registros a leer (2 bytes)
  uint16_t crc;          // Código de redundancia cíclica (2 bytes) para la capa RTU
  
  bool     isValid;      // Flag auxiliar para saber si la estructura contiene datos válidos
};

//----------------------------------MODBUS RTU RESPONSE---------------------------------------------

struct modbusRTUResponseStruct {
  uint8_t  slaveID;       // ID del Esclavo (1 byte)
  uint8_t  functionCode;  // Código de función (1 byte)
  uint8_t  byteCount;     // Cuántos bytes de datos vienen (1 byte)
  byte     data[250];     // Buffer para almacenar los datos de los registros (Máx Modbus es ~250)
  uint16_t crc;           // CRC de la trama de respuesta (2 bytes)
};
//-----------------------------------modbus TCP RESPONSE------------------------------------------

// Estructura para gestionar las respuestas Modbus TCP hacia el SCADA
struct modbusTCPResponseStruct {
  uint16_t transactionID; // ID copiado de la petición (2 bytes)
  uint16_t protocolID;    // Siempre 0x0000 (2 bytes)
  uint16_t length;        // Longitud dinámica de los bytes restantes (2 bytes)
  uint8_t  slaveID;       // ID del esclavo (1 byte)
  uint8_t  functionCode;  // Código de función (1 byte)
  uint8_t  byteCount;     // Cuántos bytes de datos se envían (1 byte)
  byte     data[250];     // Valores de los registros
};

//-----------------------------------common functions--------------------------------

bool loadModbusRTUReqBuf(const byte tcp_request[SIZE_MB_TCP_REQUEST], byte* result_mb_request);

//Aux function
void showInfoModbusTCPReq(const byte tcp_request[SIZE_MB_TCP_REQUEST]); 
void showInfoModbusRTUReq(const byte rtu_request[SIZE_MB_RTU_REQUEST]); 

bool getStructModbusRTUReq(const byte rtu_request[SIZE_MB_RTU_REQUEST], modbusRTUStruct* mb_rtu_req_struct);
bool getStructModbusTCPReq(const byte tcp_request[SIZE_MB_TCP_REQUEST], modbusTCPStruct* mb_tcp_req_struct);

int getModbusClientDataType(uint8_t functionCode);

#endif


