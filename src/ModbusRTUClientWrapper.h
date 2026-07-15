#ifndef MODBUS_RTU_CLIENT_H
#define MODBUS_RTU_CLIENT_H

#include "IModbusClient.h"
#include <ModbusRTUClient.h>

class ModbusRtuClient : public IModbusClient {
private:
    ModbusRTUClientClass* _rtu;

public:
    ModbusRtuClient(ModbusRTUClientClass* rtu) : _rtu(rtu) {
        _rtu->setTimeout(500);
    }

    int coilRead(int id, int address){
        return _rtu->coilRead(id, address); 
    } 

    int discreteInputRead(int id, int address) override {
        return _rtu->discreteInputRead(id, address);
    }

    long holdingRegisterRead(int id, int address) override {
        return _rtu->holdingRegisterRead(id, address);
    }

    long inputRegisterRead(int id, int address) override {
        return _rtu->inputRegisterRead(id, address);
    }

    // Escritura
    int coilWrite(int id, int address, uint8_t value) override {
        return _rtu->coilWrite(id, address, value); 
    }

    int holdingRegisterWrite(int id, int address, uint16_t value) override {
        return _rtu->holdingRegisterWrite(id, address, value);
    }

    int registerMaskWrite(int id, int address, uint16_t andMask, uint16_t orMask) override {
        return _rtu->registerMaskWrite(id, address, andMask, orMask);
    }

    // Streaming de Escritura
    int beginTransmission(int id, int type, int address, int nb) override {
        return _rtu->beginTransmission(id, type, address, nb);
    }

    void write(unsigned int value) override {
        _rtu->write(value);
    }

    int endTransmission() override {
        return _rtu->endTransmission();
    }

    // Streaming de Lectura
    int requestFrom(int id, int type, int address,int nb) override {
        return _rtu->requestFrom(id, type, address, nb);
    }

    int available(){
        return _rtu->available(); 
    }

    long read() override {
        return _rtu->read();
    }

    // Utilidades
    const char* lastError() override {
        return _rtu->lastError();
    }

    void end(){
        _rtu->end();  
    }

    void setTimeout(unsigned long ms) override {
        _rtu->setTimeout(ms);
    }
};

#endif