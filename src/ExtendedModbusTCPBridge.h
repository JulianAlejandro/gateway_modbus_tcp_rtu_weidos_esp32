#ifndef EXTENDED_MODBUS_TCP_BRIDGE_H
#define EXTENDED_MODBUS_TCP_BRIDGE_H

#include "ModbusTcpBridge.h"
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

// Definimos el tipo de callback
typedef void (*ModbusInterceptorCallback)(const modbusTCPStruct& req, uint16_t index, uint16_t value); 

class ExtendedModbusTCPBridge : public ModbusTcpBridge {
public:
    // El constructor pasa los parámetros obligatorios a la clase base
    ExtendedModbusTCPBridge(uint16_t port, ModbusRTUClientManager* rtuModule);

    void setHardwareMutex(SemaphoreHandle_t rtuMutex); 
    void setInterceptor(ModbusInterceptorCallback callback);

protected:
    // Sobreescribimos handleClient para inyectar el Mutex y el Callback
    void handleClient(EthernetClient& client) override;
    void sendTCPResponse(EthernetClient& client, const modbusTCPStruct& req) override;

private:
    SemaphoreHandle_t _rtuMutex = nullptr;
    ModbusInterceptorCallback _interceptor = nullptr;
};

#endif