
#ifndef MODBUS_TCP_MODULE_H
#define MODBUS_TCP_MODULE_H

#include <Arduino.h>
#include <Ethernet.h>
#include "JmodbusBridge.h"
#include "ModbusRTUModule.h"

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

typedef void (*ModbusInterceptorCallback)(const modbusTCPStruct& req, uint16_t index, uint16_t value); 
// 2. INSTANCIA DEL SERVIDOR SEGURO
class WeidosModbusServer : public EthernetServer {
public:
    WeidosModbusServer(uint16_t port) : EthernetServer(port) {}
    virtual void begin(uint16_t port) override {
        EthernetServer::begin(); 
    }
};

class ModbusTCPModule {
public:
    ModbusTCPModule(uint16_t port, ModbusRTUModule* rtuModule);
    void begin(byte mac[], IPAddress ip);
    void setHardwareMutex(SemaphoreHandle_t rtuMutex); 
    void process(); // Esta función se llamará repetidamente en el loop central

    //Metodo para registrar el callback desde el exterior
    void setInterceptor(ModbusInterceptorCallback callback); 

private:
    uint16_t _port;
    WeidosModbusServer _server;
    ModbusRTUModule* _rtu; // Referencia al módulo físico de RTU
    SemaphoreHandle_t _rtuMutex;
    byte _tcpRequestBuffer[SIZE_MB_TCP_REQUEST];

    ModbusInterceptorCallback _interceptor = nullptr; 

    void handleClient(EthernetClient& client);
    void sendTCPResponse(EthernetClient& client, const modbusTCPStruct& req);
};
//
#endif

