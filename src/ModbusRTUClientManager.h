#ifndef MODBUS_RTU_CLIENT_MANAGER_H
#define MODBUS_RTU_CLIENT_MANAGER_H

#include "IModbusClient.h"
#include <ModbusRTUClient.h>

class ModbusRtuClientManager : public IModbusClient {
private:
    ModbusRTUClientClass* _rtu;
public:
    ModbusRtuClientManager(ModbusRTUClientClass* rtu);

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