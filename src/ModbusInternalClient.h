#ifndef MODBUS_INTERNAL_CLIENT_H
#define MODBUS_INTERNAL_CLIENT_H

#include "IModbusClient.h"
#include "InternalModbusSlave.h"

class ModbusInternalClient : IModbusClient {
private:
    InternalModbusSlave* _slave;
    uint16_t _bufferRead[125]; // Buffer temporal para simular la cola de lectura
    int _bufferIndex;
    int _bufferLength;
    
    // Para escrituras múltiples
    int _writeDataType;
    int _writeAddress;
    int _writeIndex;

public:
    ModbusInternalClient(InternalModbusSlave* slave);

    bool coilWrite(uint8_t slaveID, int address, uint8_t value) override;

    bool holdingRegisterWrite(uint8_t slaveID, int address, uint16_t value) override;

    bool beginTransmission(uint8_t slaveID, int dataType, int address, int quantity) override;

    void write(uint16_t value) override;

    bool endTransmission() override;

    bool requestFrom(uint8_t slaveID, int dataType, int address, int quantity) override;

    int read() override;

    const char* lastError() override;
}; 

#endif