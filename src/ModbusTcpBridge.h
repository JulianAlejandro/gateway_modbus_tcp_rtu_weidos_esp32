
#ifndef MODBUS_TCP_BRIDGE_H
#define MODBUS_TCP_BRIDGE_H

#include <Arduino.h>
#include <Ethernet.h>
//#include <ModbusRTUClient.h>
#include "IModbusClient.h"
#include "IThreadLock.h" // Interfaz que permite entrada de un mutex generico para gestionar uso compartido de HW (rtuCLient)
#include <functional>

#define SIZE_MB_TCP_FRAME 260

struct modbusStruct {
  //MBAP 7 bytes
  uint16_t transactionID;
  uint16_t protocolID;
  uint16_t length;
  uint8_t  slaveID;
  // PDU
  uint8_t  functionCode;
  uint16_t address;
  uint16_t quantity_value; 
};

class WeidosEthernetServer : public EthernetServer {
public:
    WeidosEthernetServer(uint16_t port) : EthernetServer(port) {}
    virtual void begin(uint16_t port) override {
        EthernetServer::begin(); 
    }
};

// Pasamos el valor por referencia (uint16_t&) para permitir modificaciones al vuelo si fuera necesario
typedef std::function<void(const modbusStruct& req, uint16_t index, uint16_t& value)> ModbusInterceptorCallback;

class ModbusTcpBridge {
public:
    ModbusTcpBridge(uint16_t port, IModbusClient* mbClient, IThreadLock* lock = nullptr); // recibe HW y mutex
    void begin(byte mac[], IPAddress ip);
    void process(); // Esta función se llamará repetidamente en el loop central
    static bool parseTCPBufferToStruct(const byte* tcp_buf, modbusStruct* out_struct);

    void setInterceptor(ModbusInterceptorCallback callback) { _interceptor = callback; }
    static int getModbusClientDataType(uint8_t functionCode);

    void setThreadLock(IThreadLock* lock) { 
        if (lock != nullptr) _lock = lock; 
    }

private:   
    uint16_t _port;
    WeidosEthernetServer _ethernetServer;  
    byte _modbusTcpBuffer[SIZE_MB_TCP_FRAME];
    IModbusClient* _mbClient;

    IThreadLock* _lock;
    DummyLock _defaultLock; // en caso de nullptr , uso de por defecto

    ModbusInterceptorCallback _interceptor = nullptr;

    void handleClient(EthernetClient& client);
    bool processRtuCommand(const modbusStruct& mbData);
    void sendTCPResponse(EthernetClient& client, const modbusStruct& req);
    void sendTCPException(EthernetClient& client, const modbusStruct& req, uint8_t exceptionCode);

};

#endif

