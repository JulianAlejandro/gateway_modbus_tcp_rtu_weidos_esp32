

#include "ModbusRTUClientManager.h"
#include <ArduinoRS485.h>
#include <ModbusRTUClient.h>
#include "esp_log.h"

static const char* TAG = "MB_RTU_CLN_MNGR"; 

ModbusRTUClientManager::ModbusRTUClientManager(uint32_t baudrate) : _baudrate(baudrate) {}

bool ModbusRTUClientManager::begin() {
    // Configuración de los pines RS485 nativos de la placa Weidmüller
    RS485.setPins(RS485_TX, RS485_DE, RS485_RE);
   // ModbusRTUClient.setTimeout(150);
    return ModbusRTUClient.begin(_baudrate, (uint16_t)SERIAL_8N1);
}

bool ModbusRTUClientManager::readFromSlave(const modbusTCPStruct& request) {
    int dataType = getModbusClientDataType(request.functionCode);
    if (dataType == -1) return false;
    ESP_LOGI(TAG, "Modbus RTU Client, slave id: %d, DataType: %d, Adress: %d, Quantity: %d \n", request.slaveID, dataType, request.address, request.quantity);
    return ModbusRTUClient.requestFrom(request.slaveID, dataType, request.address, request.quantity);
}

int16_t ModbusRTUClientManager::readRegister() {
    return ModbusRTUClient.read();
}

const char* ModbusRTUClientManager::getLastError() {
    return ModbusRTUClient.lastError();
}

int ModbusRTUClientManager::getModbusClientDataType(uint8_t functionCode) {
  switch (functionCode) {
    case 0x01: return COILS;
    case 0x02: return DISCRETE_INPUTS;
    case 0x03: return HOLDING_REGISTERS;
    case 0x04: return INPUT_REGISTERS;
    default:   return -1;
  }
}

