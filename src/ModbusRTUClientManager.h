#ifndef MODBUS_RTU_MODULE_H
#define MODBUS_RTU_MODULE_H

#include <Arduino.h>
#include "JmodbusBridge.h"
//
class ModbusRTUClientManager {
public:
    ModbusRTUClientManager(uint32_t baudrate);
    bool begin();
    
    // Ejecuta la lectura en el bus físico. Retorna true si el esclavo respondió correctamente.
    bool readFromSlave(const modbusTCPStruct& request);
    
    // Permite que otros módulos extraigan las respuestas del buffer interno de ModbusRTUClient
    int16_t readRegister();
    const char* getLastError();

private:
    uint32_t _baudrate;
};

#endif