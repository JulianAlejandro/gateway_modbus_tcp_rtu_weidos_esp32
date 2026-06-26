

#include "ModbusRTUClientManager.h"
#include <ArduinoRS485.h>
#include <ModbusRTUClient.h>

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

    return ModbusRTUClient.requestFrom(request.slaveID, dataType, request.address, request.quantity);
}

int16_t ModbusRTUClientManager::readRegister() {
    return ModbusRTUClient.read();
}

const char* ModbusRTUClientManager::getLastError() {
    return ModbusRTUClient.lastError();
}

