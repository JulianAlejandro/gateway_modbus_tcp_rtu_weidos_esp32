#ifndef MODBUS_RTU_CLIENT_WRAPPER_H
#define MODBUS_RTU_CLIENT_WRAPPER_H

#include "IModbusClient.h"
#include <ModbusRTUClient.h>

class ModbusRtuClient : public IModbusClient {
private:
    ModbusRTUClientClass* _rtu;
    int _lastError = 0;

public:
    ModbusRtuClient(ModbusRTUClientClass* rtu) : _rtu(rtu) {
    }

    int coilRead(int id, int address) override {
        int result = _rtu->coilRead(id, address);
        _lastError = (result == -1) ? errno : 0;
        return result;
    }

    int discreteInputRead(int id, int address) override {
        int result = _rtu->discreteInputRead(id, address);
        _lastError = (result == -1) ? errno : 0;
        return result;
    }

    long holdingRegisterRead(int id, int address) override {
        long result = _rtu->holdingRegisterRead(id, address);
        _lastError = (result == -1) ? errno : 0;
        return result;
    }

    long inputRegisterRead(int id, int address) override {
        long result = _rtu->inputRegisterRead(id, address);
        _lastError = (result == -1) ? errno : 0;
        return result;
    }

    // Escritura
    int coilWrite(int id, int address, uint8_t value) override {
        int result = _rtu->coilWrite(id, address, value);
        _lastError = (result == 0) ? errno : 0;
        return result;
    }

    int holdingRegisterWrite(int id, int address, uint16_t value) override {
        int result = _rtu->holdingRegisterWrite(id, address, value);
        _lastError = (result == 0) ? errno : 0;
        return result;
    }

    int registerMaskWrite(int id, int address, uint16_t andMask, uint16_t orMask) override {
        int result = _rtu->registerMaskWrite(id, address, andMask, orMask);
        _lastError = (result == 0) ? errno : 0;
        return result;
    }

    // Streaming de Escritura
    int beginTransmission(int id, int type, int address, int nb) override {
        int result = _rtu->beginTransmission(id, type, address, nb);
        _lastError = (result == 0) ? errno : 0;
        return result;
    }

    void write(unsigned int value) override {
        _rtu->write(value);
    }

    int endTransmission() override {
        int result = _rtu->endTransmission();
        _lastError = (result == 0) ? errno : 0;
        return result;
    }

    // Streaming de Lectura
    int requestFrom(int id, int type, int address, int nb) override {
        int result = _rtu->requestFrom(id, type, address, nb);
        _lastError = (result == 0) ? errno : 0;
        return result;
    }

    int available() override {
        return _rtu->available();
    }

    long read() override {
        return _rtu->read();
    }

    // Utilidades
    const char* lastError() override {
        return _rtu->lastError();
    }

    int lastErrorCode() override {
        return _lastError;
    }

    void end() override {
        _rtu->end();
    }

    void setTimeout(unsigned long ms) override {
        _rtu->setTimeout(ms);
    }
};

#endif
