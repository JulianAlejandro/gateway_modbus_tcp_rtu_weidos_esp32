#include "JmodbusBridge.h"
#include <ModbusRTUClient.h> // Para las constantes COILS, HOLDING_REGISTERS, etc.

bool parseTCPBufferToStruct(const byte* tcp_buf, modbusTCPStruct* out_struct) {
  if (out_struct == nullptr) return false;

  uint8_t fCode = tcp_buf[7];
  if (fCode != 0x03 && fCode != 0x04) {
    out_struct->isValid = false;
    return false;
  }

  out_struct->transactionID = (tcp_buf[0] << 8) | tcp_buf[1];
  out_struct->protocolID    = (tcp_buf[2] << 8) | tcp_buf[3];
  out_struct->length        = (tcp_buf[4] << 8) | tcp_buf[5];
  out_struct->slaveID       = tcp_buf[6];
  out_struct->functionCode  = fCode;
  out_struct->address       = (tcp_buf[8] << 8) | tcp_buf[9];
  out_struct->quantity      = (tcp_buf[10] << 8) | tcp_buf[11];
  out_struct->isValid       = true;

  return true;
}

int getModbusClientDataType(uint8_t functionCode) {
  switch (functionCode) {
    case 0x01: return COILS;
    case 0x02: return DISCRETE_INPUTS;
    case 0x03: return HOLDING_REGISTERS;
    case 0x04: return INPUT_REGISTERS;
    default:   return -1;
  }
}

/*

#include "JmodbusBridge.h"

bool loadModbusRTUReqBuf(const byte tcp_request[SIZE_MB_TCP_REQUEST], byte* result_mb_request) {
  
  // 1. Validación rápida: verificar que sea una función de lectura soportada (0x03 o 0x04)
  byte functionCode = tcp_request[7];
  if (functionCode != 0x03 && functionCode != 0x04) {
    return false; // Trama no válida, devolvemos false
  }

  // 2. Mapear los datos de TCP a RTU
  result_mb_request[0] = tcp_request[6];  // Unit ID (Slave ID)
  result_mb_request[1] = tcp_request[7];  // Function Code
  result_mb_request[2] = tcp_request[8];  // Address High
  result_mb_request[3] = tcp_request[9];  // Address Low
  result_mb_request[4] = tcp_request[10]; // Quantity High
  result_mb_request[5] = tcp_request[11]; // Quantity Low

  // Nota: Los bytes result_mb_request[6] y [7] quedan reservados 
  // para el CRC que calcularás justo antes de enviarlo por el RS485.

  return true; // Conversión exitosa
}



// ---------------------------------------------------------
// Muestra la información desglosada de una trama Modbus TCP
// ---------------------------------------------------------
void showInfoModbusTCPReq(const byte tcp_request[SIZE_MB_TCP_REQUEST]) {
  // Reconstruimos los valores de 16 bits juntando sus dos bytes (High y Low)
  uint16_t transactionId = (tcp_request[0] << 8) | tcp_request[1];
  uint16_t protocolId    = (tcp_request[2] << 8) | tcp_request[3];
  uint16_t length        = (tcp_request[4] << 8) | tcp_request[5];
  uint8_t  unitId        = tcp_request[6];
  uint8_t  functionCode  = tcp_request[7];
  uint16_t address       = (tcp_request[8] << 8) | tcp_request[9];
  uint16_t quantity      = (tcp_request[10] << 8) | tcp_request[11];

  Serial.println(F("====== [DEBUG MODBUS TCP REQ] ======"));
  Serial.printf("  Transaction ID : %d (0x%04X)\n", transactionId, transactionId);
  Serial.printf("  Protocol ID    : %d (0x%04X) %s\n", protocolId, protocolId, (protocolId == 0 ? "[OK]" : "[ERR]"));
  Serial.printf("  Length (Resto) : %d bytes\n", length);
  Serial.printf("  Unit ID (Slave): %d\n", unitId);
  Serial.printf("  Function Code  : 0x%02X %s\n", functionCode, (functionCode == 0x03 ? "(Read Holding)" : (functionCode == 0x04 ? "(Read Input)" : "(Unknown)")));
  Serial.printf("  Start Address  : %d (0x%04X)\n", address, address);
  Serial.printf("  Quantity Regs  : %d\n", quantity);
  
  // Imprimir trama en formato HEX continuo para comprobar byte a byte
  Serial.print(F("  RAW Bytes      : "));
  for (int i = 0; i < SIZE_MB_TCP_REQUEST; i++) {
    Serial.printf("%02X ", tcp_request[i]);
  }
  Serial.println(F("\n===================================="));
}

// ---------------------------------------------------------
// Muestra la información desglosada de una trama Modbus RTU
// ---------------------------------------------------------
void showInfoModbusRTUReq(const byte rtu_request[SIZE_MB_RTU_REQUEST]) {
  uint8_t  slaveId      = rtu_request[0];
  uint8_t  functionCode = rtu_request[1];
  uint16_t address      = (rtu_request[2] << 8) | rtu_request[3];
  uint16_t quantity     = (rtu_request[4] << 8) | rtu_request[5];
  
  // Ojo: En RTU el CRC se almacena en formato Little Endian (Byte Bajo primero, luego Byte Alto)
  uint16_t crc          = (rtu_request[7] << 8) | rtu_request[6]; 

  Serial.println(F("====== [DEBUG MODBUS RTU REQ] ======"));
  Serial.printf("  Slave ID       : %d\n", slaveId);
  Serial.printf("  Function Code  : 0x%02X %s\n", functionCode, (functionCode == 0x03 ? "(Read Holding)" : (functionCode == 0x04 ? "(Read Input)" : "(Unknown)")));
  Serial.printf("  Start Address  : %d (0x%04X)\n", address, address);
  Serial.printf("  Quantity Regs  : %d\n", quantity);
  Serial.printf("  CRC 16 Check   : 0x%04X (Low: 0x%02X, High: 0x%02X)\n", crc, rtu_request[6], rtu_request[7]);
  
  // Imprimir trama en formato HEX continuo
  Serial.print(F("  RAW Bytes      : "));
  for (int i = 0; i < SIZE_MB_RTU_REQUEST; i++) {
    Serial.printf("%02X ", rtu_request[i]);
  }
  Serial.println(F("\n===================================="));
}

// ---------------------------------------------------------------------------------
// Extrae los datos de un buffer físico RTU (8 bytes) y llena la estructura Modbus RTU
// ---------------------------------------------------------------------------------
bool getStructModbusRTUReq(const byte rtu_request[SIZE_MB_RTU_REQUEST], modbusRTUStruct* mb_rtu_req_struct) {
  // 1. Control de seguridad ante punteros nulos
  if (mb_rtu_req_struct == nullptr) return false;

  // 2. Validación del código de función (Lecturas soportadas: 0x03 o 0x04)
  uint8_t fCode = rtu_request[1];
  if (fCode != 0x03 && fCode != 0x04) {
    mb_rtu_req_struct->isValid = false;
    return false;
  }

  // 3. Mapear datos directos desde el array hacia la estructura
  mb_rtu_req_struct->slaveID      = rtu_request[0];
  mb_rtu_req_struct->functionCode = fCode;
  mb_rtu_req_struct->address      = (rtu_request[2] << 8) | rtu_request[3];
  mb_rtu_req_struct->quantity     = (rtu_request[4] << 8) | rtu_request[5];
  
  // Ojo: El CRC en RTU se recibe en formato Little Endian (Low byte primero)
  mb_rtu_req_struct->crc          = (rtu_request[7] << 8) | rtu_request[6]; 
  mb_rtu_req_struct->isValid      = true;

  return true;
}

// ---------------------------------------------------------------------------------
// Extrae los datos de un buffer físico TCP (12 bytes) y llena la estructura Modbus TCP
// ---------------------------------------------------------------------------------
bool getStructModbusTCPReq(const byte tcp_request[SIZE_MB_TCP_REQUEST], modbusTCPStruct* mb_tcp_req_struct) {
  // 1. Control de seguridad ante punteros nulos
  if (mb_tcp_req_struct == nullptr) return false;

  // 2. Validación del código de función (Lecturas soportadas: 0x03 o 0x04)
  uint8_t fCode = tcp_request[7];
  if (fCode != 0x03 && fCode != 0x04) {
    mb_tcp_req_struct->isValid = false;
    return false;
  }

  // 3. Mapear datos de la cabecera MBAP (Campos de 16 bits en Big Endian)
  mb_tcp_req_struct->transactionID = (tcp_request[0] << 8) | tcp_request[1];
  mb_tcp_req_struct->protocolID    = (tcp_request[2] << 8) | tcp_request[3];
  mb_tcp_req_struct->length        = (tcp_request[4] << 8) | tcp_request[5];
  mb_tcp_req_struct->slaveID       = tcp_request[6]; // El Unit ID de TCP

  // 4. Mapear datos de la PDU
  mb_tcp_req_struct->functionCode  = fCode;
  mb_tcp_req_struct->address       = (tcp_request[8] << 8) | tcp_request[9];
  mb_tcp_req_struct->quantity      = (tcp_request[10] << 8) | tcp_request[11];
  mb_tcp_req_struct->isValid       = true;

  return true;
}


int getModbusClientDataType(uint8_t functionCode) {
  switch (functionCode) {
    case 0x01: return COILS;
    case 0x02: return DISCRETE_INPUTS;
    case 0x03: return HOLDING_REGISTERS;
    case 0x04: return INPUT_REGISTERS;
    
    // Si en el futuro añades escrituras en Holding Registers:
    case 0x06: return HOLDING_REGISTERS; 
    case 0x10: return HOLDING_REGISTERS;

    default:   return -1; // Código de función no soportado o inválido
  }
}

*/