#include "ModbusTCPToRTUGateway.h"

extern "C" {
  #include "libmodbus/modbus.h"
  #include "libmodbus/modbus-tcp.h"
}

ModbusTCPToRTUGateway::ModbusTCPToRTUGateway() :
  _rtuClient(NULL)
{
}

ModbusTCPToRTUGateway::~ModbusTCPToRTUGateway()
{
}

int ModbusTCPToRTUGateway::setClient(ModbusClient* client)
{
  if (client == NULL) {
    return 0;
  }
  _rtuClient = client;
  return 1;
}

int ModbusTCPToRTUGateway::configureCoils(int startAddress, int nb)
{
  return ModbusServer::configureCoils(startAddress, nb);
}

int ModbusTCPToRTUGateway::configureDiscreteInputs(int startAddress, int nb)
{
  return ModbusServer::configureDiscreteInputs(startAddress, nb);
}

int ModbusTCPToRTUGateway::configureHoldingRegisters(int startAddress, int nb)
{
  return ModbusServer::configureHoldingRegisters(startAddress, nb);
}

int ModbusTCPToRTUGateway::configureInputRegisters(int startAddress, int nb)
{
  return ModbusServer::configureInputRegisters(startAddress, nb);
}

void ModbusTCPToRTUGateway::poll()
{
  if (_rtuClient == NULL) {
    return;
  }

  uint8_t request[MODBUS_TCP_MAX_ADU_LENGTH];

  int requestLength = modbus_receive(_mb, request);

  if (requestLength <= 0) {
    return;
  }

  uint8_t unitId = request[6];
  uint8_t fc     = request[7];

  bool success = false;

  switch (fc) {
    case 0x01: {
      uint16_t address  = (request[8] << 8) | request[9];
      uint16_t quantity = (request[10] << 8) | request[11];
      success = handleReadCoils(unitId, address, quantity);
      break;
    }
    case 0x02: {
      uint16_t address  = (request[8] << 8) | request[9];
      uint16_t quantity = (request[10] << 8) | request[11];
      success = handleReadDiscreteInputs(unitId, address, quantity);
      break;
    }
    case 0x03: {
      uint16_t address  = (request[8] << 8) | request[9];
      uint16_t quantity = (request[10] << 8) | request[11];
      success = handleReadHoldingRegisters(unitId, address, quantity);
      break;
    }
    case 0x04: {
      uint16_t address  = (request[8] << 8) | request[9];
      uint16_t quantity = (request[10] << 8) | request[11];
      success = handleReadInputRegisters(unitId, address, quantity);
      break;
    }
    case 0x05: {
      uint16_t address = (request[8] << 8) | request[9];
      uint16_t value   = (request[10] << 8) | request[11];
      success = handleWriteSingleCoil(unitId, address, value);
      break;
    }
    case 0x06: {
      uint16_t address = (request[8] << 8) | request[9];
      uint16_t value   = (request[10] << 8) | request[11];
      success = handleWriteSingleRegister(unitId, address, value);
      break;
    }
    case 0x0F: {
      uint16_t address  = (request[8] << 8) | request[9];
      uint16_t quantity = (request[10] << 8) | request[11];
      uint8_t byteCount = request[12];
      success = handleWriteMultipleCoils(unitId, address, quantity, &request[13], byteCount);
      break;
    }
    case 0x10: {
      uint16_t address  = (request[8] << 8) | request[9];
      uint16_t quantity = (request[10] << 8) | request[11];
      uint8_t byteCount = request[12];
      success = handleWriteMultipleRegisters(unitId, address, quantity, &request[13], byteCount);
      break;
    }
    default:
      modbus_reply_exception(_mb, request, MODBUS_EXCEPTION_ILLEGAL_FUNCTION);
      return;
  }

  if (success) {
    modbus_reply(_mb, request, requestLength, &_mbMapping);
  } else {
    modbus_reply_exception(_mb, request, MODBUS_EXCEPTION_SLAVE_OR_SERVER_FAILURE);
  }
}

bool ModbusTCPToRTUGateway::handleReadCoils(uint8_t unitId, uint16_t address, uint16_t quantity)
{
  if (_mbMapping.tab_bits == NULL ||
      address < _mbMapping.start_bits ||
      (address + quantity) > (_mbMapping.start_bits + _mbMapping.nb_bits)) {
    return false;
  }

  int count = _rtuClient->requestFrom(unitId, COILS, address, quantity);
  if (count != quantity) {
    return false;
  }

  uint8_t* bits = (uint8_t*)malloc(quantity * sizeof(uint8_t));
  if (bits == NULL) {
    return false;
  }

  for (int i = 0; i < quantity; i++) {
    long val = _rtuClient->read();
    if (val < 0) {
      free(bits);
      return false;
    }
    bits[i] = (uint8_t)val;
  }

  copyBitsToMapping(&_mbMapping, address, quantity, bits);
  free(bits);
  return true;
}

bool ModbusTCPToRTUGateway::handleReadDiscreteInputs(uint8_t unitId, uint16_t address, uint16_t quantity)
{
  if (_mbMapping.tab_input_bits == NULL ||
      address < _mbMapping.start_input_bits ||
      (address + quantity) > (_mbMapping.start_input_bits + _mbMapping.nb_input_bits)) {
    return false;
  }

  int count = _rtuClient->requestFrom(unitId, DISCRETE_INPUTS, address, quantity);
  if (count != quantity) {
    return false;
  }

  uint8_t* bits = (uint8_t*)malloc(quantity * sizeof(uint8_t));
  if (bits == NULL) {
    return false;
  }

  for (int i = 0; i < quantity; i++) {
    long val = _rtuClient->read();
    if (val < 0) {
      free(bits);
      return false;
    }
    bits[i] = (uint8_t)val;
  }

  memcpy(&_mbMapping.tab_input_bits[address - _mbMapping.start_input_bits], bits, quantity * sizeof(uint8_t));
  free(bits);
  return true;
}

bool ModbusTCPToRTUGateway::handleReadHoldingRegisters(uint8_t unitId, uint16_t address, uint16_t quantity)
{
  if (_mbMapping.tab_registers == NULL ||
      address < _mbMapping.start_registers ||
      (address + quantity) > (_mbMapping.start_registers + _mbMapping.nb_registers)) {
    return false;
  }

  int count = _rtuClient->requestFrom(unitId, HOLDING_REGISTERS, address, quantity);
  if (count != quantity) {
    return false;
  }

  uint16_t* regs = (uint16_t*)malloc(quantity * sizeof(uint16_t));
  if (regs == NULL) {
    return false;
  }

  for (int i = 0; i < quantity; i++) {
    long val = _rtuClient->read();
    if (val < 0) {
      free(regs);
      return false;
    }
    regs[i] = (uint16_t)val;
  }

  copyRegistersToMapping(&_mbMapping, address, quantity, regs);
  free(regs);
  return true;
}

bool ModbusTCPToRTUGateway::handleReadInputRegisters(uint8_t unitId, uint16_t address, uint16_t quantity)
{
  if (_mbMapping.tab_input_registers == NULL ||
      address < _mbMapping.start_input_registers ||
      (address + quantity) > (_mbMapping.start_input_registers + _mbMapping.nb_input_registers)) {
    return false;
  }

  int count = _rtuClient->requestFrom(unitId, INPUT_REGISTERS, address, quantity);
  if (count != quantity) {
    return false;
  }

  uint16_t* regs = (uint16_t*)malloc(quantity * sizeof(uint16_t));
  if (regs == NULL) {
    return false;
  }

  for (int i = 0; i < quantity; i++) {
    long val = _rtuClient->read();
    if (val < 0) {
      free(regs);
      return false;
    }
    regs[i] = (uint16_t)val;
  }

  memcpy(&_mbMapping.tab_input_registers[address - _mbMapping.start_input_registers], regs, quantity * sizeof(uint16_t));
  free(regs);
  return true;
}

bool ModbusTCPToRTUGateway::handleWriteSingleCoil(uint8_t unitId, uint16_t address, uint16_t value)
{
  if (_mbMapping.tab_bits == NULL ||
      address < _mbMapping.start_bits ||
      address >= (_mbMapping.start_bits + _mbMapping.nb_bits)) {
    return false;
  }

  uint8_t coilValue = (value == 0xFF00) ? 1 : 0;
  return _rtuClient->coilWrite(unitId, address, coilValue) == 1;
}

bool ModbusTCPToRTUGateway::handleWriteSingleRegister(uint8_t unitId, uint16_t address, uint16_t value)
{
  if (_mbMapping.tab_registers == NULL ||
      address < _mbMapping.start_registers ||
      address >= (_mbMapping.start_registers + _mbMapping.nb_registers)) {
    return false;
  }

  return _rtuClient->holdingRegisterWrite(unitId, address, value) == 1;
}

bool ModbusTCPToRTUGateway::handleWriteMultipleCoils(uint8_t unitId, uint16_t address, uint16_t quantity, const uint8_t* data, int dataLen)
{
  if (_mbMapping.tab_bits == NULL ||
      address < _mbMapping.start_bits ||
      (address + quantity) > (_mbMapping.start_bits + _mbMapping.nb_bits)) {
    return false;
  }

  if (!_rtuClient->beginTransmission(unitId, COILS, address, quantity)) {
    return false;
  }

  for (int i = 0; i < quantity; i++) {
    int byteIndex = i / 8;
    int bitIndex  = i % 8;
    uint8_t bitValue = (data[byteIndex] >> bitIndex) & 0x01;
    _rtuClient->write(bitValue);
  }

  return _rtuClient->endTransmission() == 1;
}

bool ModbusTCPToRTUGateway::handleWriteMultipleRegisters(uint8_t unitId, uint16_t address, uint16_t quantity, const uint8_t* data, int dataLen)
{
  if (_mbMapping.tab_registers == NULL ||
      address < _mbMapping.start_registers ||
      (address + quantity) > (_mbMapping.start_registers + _mbMapping.nb_registers)) {
    return false;
  }

  if (!_rtuClient->beginTransmission(unitId, HOLDING_REGISTERS, address, quantity)) {
    return false;
  }

  for (int i = 0; i < quantity; i++) {
    uint16_t value = (data[i * 2] << 8) | data[i * 2 + 1];
    _rtuClient->write(value);
  }

  return _rtuClient->endTransmission() == 1;
}

void ModbusTCPToRTUGateway::copyBitsToMapping(modbus_mapping_t* m, uint16_t address, uint16_t quantity, const uint8_t* values)
{
  memcpy(&m->tab_bits[address - m->start_bits], values, quantity * sizeof(uint8_t));
}

void ModbusTCPToRTUGateway::copyRegistersToMapping(modbus_mapping_t* m, uint16_t address, uint16_t quantity, const uint16_t* values)
{
  memcpy(&m->tab_registers[address - m->start_registers], values, quantity * sizeof(uint16_t));
}
