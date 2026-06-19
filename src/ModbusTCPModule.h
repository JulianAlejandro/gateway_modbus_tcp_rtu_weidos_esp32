
#ifndef MODBUS_TCP_MODULE_H
#define MODBUS_TCP_MODULE_H

#include <Arduino.h>
#include <Ethernet.h>
#include "JmodbusBridge.h"
#include "ModbusRTUModule.h"


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
    void process(); // Esta función se llamará repetidamente en el loop central

private:
    uint16_t _port;
    WeidosModbusServer _server;
    ModbusRTUModule* _rtu; // Referencia al módulo físico de RTU
    byte _tcpRequestBuffer[SIZE_MB_TCP_REQUEST];

    void handleClient(EthernetClient& client);
    void sendTCPResponse(EthernetClient& client, const modbusTCPStruct& req);
};
//
#endif

