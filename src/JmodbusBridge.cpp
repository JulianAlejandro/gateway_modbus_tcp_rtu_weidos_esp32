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