#ifndef I_MODBUS_CLIENT_H
#define I_MODBUS_CLIENT_H

#include <Arduino.h>
#include "ArduinoModbus.h"

class IModbusClient {
public:
    virtual ~IModbusClient() {}

    // Operaciones de Escritura
    virtual bool coilWrite(uint8_t slaveID, int address, uint8_t value) = 0;
    virtual bool holdingRegisterWrite(uint8_t slaveID, int address, uint16_t value) = 0;
    
    // Operaciones de Escritura Múltiple (Preparación y envío)
    virtual bool beginTransmission(uint8_t slaveID, int dataType, int address, int quantity) = 0;
    virtual void write(uint16_t value) = 0;
    virtual bool endTransmission() = 0;

    // Operaciones de Lectura (Simulamos el requestFrom + read secuencial)
    virtual bool requestFrom(uint8_t slaveID, int dataType, int address, int quantity) = 0;
    virtual int read() = 0; // Retorna el siguiente valor de la cola de lectura
    
    virtual const char* lastError() = 0;
};

#endif