// TODO MODIFICACION EN EL MUTEX, ESTE DEBE IR LIGADO AL HW EXTERNO
#ifndef MODBUS_TCP_BRIDGE_H
#define MODBUS_TCP_BRIDGE_H

#include <Arduino.h>
#include <Ethernet.h>
#include "IModbusClient.h"
#include "IThreadLock.h"
#include <functional>

#define SIZE_MB_TCP_FRAME 260

/**
 * @brief Structured representation of a Modbus TCP frame (MBAP + PDU fields).
 */
struct modbusStruct {
  // MBAP Header (7 bytes)
  uint16_t transactionID;
  uint16_t protocolID;
  uint16_t length;
  uint8_t  slaveID;
  // PDU Payload
  uint8_t  functionCode;
  uint16_t address;
  uint16_t quantity_value; 
};

/**
 * @brief Custom Ethernet Server override to standardize startup signatures.
 */
class WeidosEthernetServer : public EthernetServer {
public:
    WeidosEthernetServer(uint16_t port) : EthernetServer(port) {}
    virtual void begin(uint16_t port) override {
        EthernetServer::begin(); 
    }
};

// Callbacks definitions for custom frame processing and telemetry snooping
typedef std::function<void(const modbusStruct& req)> ModbusTCPReqCallback; 
typedef std::function<void(const modbusStruct& req, uint16_t index, uint16_t& value)> ModbusInterceptorCallback;

/**
 * @class ModbusTcpBridge
 * @brief Gateway bridge that processes Modbus TCP client traffic and routes it to Modbus RTU.
 */
class ModbusTcpBridge {
public:
    /**
     * @brief Initializes the Modbus TCP to RTU Bridge.
     * @param port TCP port to listen on (usually 502).
     * @param mbClient Pointer to the Modbus RTU hardware backend.
     * @param lock Optional thread lock for shared hardware safety.
     */
    ModbusTcpBridge(IModbusClient* mbClient, IThreadLock* lock = nullptr, uint16_t port = 502);

    /**
     * @brief Configures interface network parameters and starts the TCP server.
     */
    void begin(uint16_t port, byte mac[], IPAddress ip, IPAddress dns, IPAddress gateway, IPAddress subnet);

    //void setPort(uint16_t port) { _port = port; }
    /**
     * @brief Non-blocking poller to check and handle client connections. Must be called in loop().
     */
    void process(); // Called continuously within the main firmware loop

    /**
     * @brief Parses raw Modbus TCP bytes into a structured format.
     * @return true if valid and function code is supported, false otherwise.
     */
    static bool parseTCPBufferToStruct(const byte* tcp_buf, modbusStruct* out_struct);

    // Callbacks configuration
    void setInterceptor(ModbusInterceptorCallback callback) { _interceptor = callback; }
    void setTCPReqCallback(ModbusTCPReqCallback callback) { _tcpReqCallback = callback;}

    void setModbusClient(IModbusClient* mbClient);
    void setThreadLock(IThreadLock* lock);  

    /**
     * @brief Maps Modbus Function Codes to client standard data type constants.
     */
    static int getModbusClientDataType(uint8_t functionCode);

private:   
    uint16_t _port;
    WeidosEthernetServer _ethernetServer;  
    byte _modbusTcpBuffer[SIZE_MB_TCP_FRAME];
    IModbusClient* _mbClient;

    IThreadLock* _lock;
    DummyLock _defaultLock; // Fallback default lock object

    ModbusInterceptorCallback _interceptor = nullptr;
    ModbusTCPReqCallback _tcpReqCallback = nullptr; 

    void handleClient(EthernetClient& client);
    
    bool getModbusTcpBuffer(EthernetClient& client, size_t maxBufferSize, byte* out_buffer);
    bool processCommand(const modbusStruct& mbData);
    void sendTCPResponse(EthernetClient& client, const modbusStruct& req);
    void sendTCPException(EthernetClient& client, const modbusStruct& req, uint8_t exceptionCode);
};

#endif

