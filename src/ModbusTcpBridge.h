
#ifndef MODBUS_TCP_BRIDGE_H
#define MODBUS_TCP_BRIDGE_H

#include <Arduino.h>
#include <Ethernet.h>
#include "CommonModbusBridge.h"
//#include "ModbusRTUClientManager.h"
#include <ModbusRTUClient.h>


class WeidosEthernetServer : public EthernetServer {
public:
    WeidosEthernetServer(uint16_t port) : EthernetServer(port) {}
    virtual void begin(uint16_t port) override {
        EthernetServer::begin(); 
    }
};

class ModbusTcpBridge {
public:
    ModbusTcpBridge(uint16_t port, ModbusRTUClientClass* rtuClient);
    void begin(byte mac[], IPAddress ip);
    void process(); // Esta función se llamará repetidamente en el loop central
    static bool parseTCPBufferToStruct(const byte* tcp_buf, modbusTCPStruct* out_struct);

private: 
    int getModbusClientDataType(uint8_t functionCode);

protected:
    uint16_t _port;
    WeidosEthernetServer _ethernetServer;
    ModbusRTUClientClass* _rtuClient; // Referencia al módulo físico de RTU
    
    byte _tcpRequestBuffer[SIZE_MB_TCP_REQUEST];

    virtual void handleClient(EthernetClient& client);
    virtual void sendTCPResponse(EthernetClient& client, const modbusTCPStruct& req);
    virtual void sendTCPException(EthernetClient& client, const modbusTCPStruct& req, uint8_t exceptionCode);
};

#endif

