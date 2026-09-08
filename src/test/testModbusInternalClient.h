#ifndef TEST_MODBUS_INTERNAL_CLIENT_H
#define TEST_MODBUS_INTERNAL_CLIENT_H

#include "IModbusClient.h"
//#include "InternalModbusSlave.h"


class TestModbusInternalClient : public IModbusClient {
private:
    uint16_t _bufferRead[125];
    int _bufferIndex;
    int _bufferLength;

public:
    TestModbusInternalClient() : _bufferIndex(0), _bufferLength(0) {};

    int coilRead(int id, int address) override { return 1; };
    int discreteInputRead(int id, int address) override { return 1; };
    long holdingRegisterRead(int id, int address) override { return (long)address; };
    long inputRegisterRead(int id, int address) override { return (long)address; };

    int coilWrite(int id, int address, uint8_t value) override { return 1; };
    int holdingRegisterWrite(int id, int address, uint16_t value) override { return 1; };

    int registerMaskWrite(int id, int address, uint16_t andMask, uint16_t orMask) override { return 1; };

    int beginTransmission(int id, int type, int address, int nb) override { return 1; };
    void write(unsigned int value) override {};
    int endTransmission() override { return 1; };

    int requestFrom(int id, int type, int address, int nb) override {
        if (nb > 125) nb = 125;
        _bufferLength = nb;
        _bufferIndex = 0;
        for (int i = 0; i < nb; i++) {
            _bufferRead[i] = (uint16_t)(address + i);
        }
        return nb;
    };

    int available() override { return _bufferLength - _bufferIndex; };

    long read() override {
        if (_bufferIndex < _bufferLength) {
            return (long)_bufferRead[_bufferIndex++];
        }
        return -1;
    };

    const char* lastError() override { return ""; };
    int lastErrorCode() override { return 0; };
    void end() override {};
    void setTimeout(unsigned long ms) override {};

}; 

#endif