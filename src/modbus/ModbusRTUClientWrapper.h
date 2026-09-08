#ifndef MODBUS_RTU_CLIENT_WRAPPER_H
#define MODBUS_RTU_CLIENT_WRAPPER_H

#include "IModbusClient.h"
#include <ModbusRTUClient.h>
#include "esp_log.h"

static const char* TAG_RTW = "MB_RTU_WRAP";

class ModbusRtuClient : public IModbusClient {
private:
    ModbusRTUClientClass* _rtu;
    int _lastError = 0;

public:
    ModbusRtuClient(ModbusRTUClientClass* rtu) : _rtu(rtu) {
        if (_rtu == nullptr) {
            ESP_LOGE(TAG_RTW, "CRITICAL: ModbusRTUClientClass pointer is NULL!");
        }
    }

    int coilRead(int id, int address) override {
        if (!_rtu) { _lastError = EINVAL; return -1; }
        int result = _rtu->coilRead(id, address);
        _lastError = (result == -1) ? errno : 0;
        return result;
    }

    int discreteInputRead(int id, int address) override {
        if (!_rtu) { _lastError = EINVAL; return -1; }
        int result = _rtu->discreteInputRead(id, address);
        _lastError = (result == -1) ? errno : 0;
        return result;
    }

    long holdingRegisterRead(int id, int address) override {
        if (!_rtu) { _lastError = EINVAL; return -1; }
        long result = _rtu->holdingRegisterRead(id, address);
        _lastError = (result == -1) ? errno : 0;
        return result;
    }

    long inputRegisterRead(int id, int address) override {
        if (!_rtu) { _lastError = EINVAL; return -1; }
        long result = _rtu->inputRegisterRead(id, address);
        _lastError = (result == -1) ? errno : 0;
        return result;
    }

    // Escritura
    int coilWrite(int id, int address, uint8_t value) override {
        if (!_rtu) { _lastError = EINVAL; return 0; }
        int result = _rtu->coilWrite(id, address, value);
        _lastError = (result == 0) ? errno : 0;
        return result;
    }

    int holdingRegisterWrite(int id, int address, uint16_t value) override {
        if (!_rtu) { _lastError = EINVAL; return 0; }
        int result = _rtu->holdingRegisterWrite(id, address, value);
        _lastError = (result == 0) ? errno : 0;
        return result;
    }

    int registerMaskWrite(int id, int address, uint16_t andMask, uint16_t orMask) override {
        if (!_rtu) { _lastError = EINVAL; return 0; }
        int result = _rtu->registerMaskWrite(id, address, andMask, orMask);
        _lastError = (result == 0) ? errno : 0;
        return result;
    }

    // Streaming de Escritura
    int beginTransmission(int id, int type, int address, int nb) override {
        if (!_rtu) { _lastError = EINVAL; return 0; }
        int result = _rtu->beginTransmission(id, type, address, nb);
        _lastError = (result == 0) ? errno : 0;
        return result;
    }

    void write(unsigned int value) override {
        if (!_rtu) return;
        _rtu->write(value);
    }

    int endTransmission() override {
        if (!_rtu) { _lastError = EINVAL; return 0; }
        int result = _rtu->endTransmission();
        _lastError = (result == 0) ? errno : 0;
        return result;
    }

    // Streaming de Lectura
    int requestFrom(int id, int type, int address, int nb) override {
        if (!_rtu) { _lastError = EINVAL; return 0; }
        int result = _rtu->requestFrom(id, type, address, nb);
        _lastError = (result == 0) ? errno : 0;
        return result;
    }

    int available() override {
        if (!_rtu) return 0;
        return _rtu->available();
    }

    long read() override {
        if (!_rtu) return -1;
        return _rtu->read();
    }

    // Utilidades
    const char* lastError() override {
        if (!_rtu) return "RTU client not initialized";
        return _rtu->lastError();
    }

    int lastErrorCode() override {
        return _lastError;
    }

    void end() override {
        if (!_rtu) return;
        _rtu->end();
    }

    void setTimeout(unsigned long ms) override {
        if (!_rtu) return;
        _rtu->setTimeout(ms);
    }
};

#endif
