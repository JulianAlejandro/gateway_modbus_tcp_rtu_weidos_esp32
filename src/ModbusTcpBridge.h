
#ifndef MODBUS_TCP_BRIDGE_H
#define MODBUS_TCP_BRIDGE_H

#include <Arduino.h>
#include <Ethernet.h>
#include "CommonModbusBridge.h"
#include "ModbusRTUClientManager.h"

class WeidosEthernetServer : public EthernetServer {
public:
    WeidosEthernetServer(uint16_t port) : EthernetServer(port) {}
    virtual void begin(uint16_t port) override {
        EthernetServer::begin(); 
    }
};

class ModbusTcpBridge {
public:
    ModbusTcpBridge(uint16_t port, ModbusRTUClientManager* rtuModule);
    void begin(byte mac[], IPAddress ip);
    void process(); // Esta función se llamará repetidamente en el loop central
    static bool parseTCPBufferToStruct(const byte* tcp_buf, modbusTCPStruct* out_struct);

private: 
    

protected:
    uint16_t _port;
    WeidosEthernetServer _ethernetServer;
    ModbusRTUClientManager* _rtu; // Referencia al módulo físico de RTU
    
    byte _tcpRequestBuffer[SIZE_MB_TCP_REQUEST];

    virtual void handleClient(EthernetClient& client);
    virtual void sendTCPResponse(EthernetClient& client, const modbusTCPStruct& req);
};
//
#endif

