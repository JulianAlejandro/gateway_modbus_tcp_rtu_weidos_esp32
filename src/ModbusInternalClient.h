#ifndef MODBUS_INTERNAL_CLIENT_H
#define MODBUS_INTERNAL_CLIENT_H

#include "IModbusClient.h"
#include "InternalModbusSlave.h"

/**
 * @class ModbusInternalClient
 * @brief Implementation of IModbusClient that bridges queries directly into a local memory-mapped slave.
 * * This bypasses the physical serial/RS485 bus entirely, allowing the bridge to interact with
 * internal registers and local I/O using the exact same interface as a remote physical RTU client.
 */
class ModbusInternalClient : public IModbusClient {
private:
    InternalModbusSlave* _slave;       ///< Pointer to the local database mapping physical/logical registers.
    uint16_t _bufferRead[125];         ///< Temporal stream array simulating the incoming read transmission FIFO queue.
    int _bufferIndex;                  ///< Current tracking read pointer inside the temporal buffer.
    int _bufferLength;                 ///< Exact count of valid unread entries remaining in the buffer stream.
    
    int _writeDataType;                ///< Enqueued write packet context identifier (e.g., COILS vs HOLDING_REGISTERS).
    int _writeAddress;                 ///< Current baseline register cursor offset for sequence buffering.
    int _writeIndex;                   ///< Tracking step layout increment counter during sequence transmissions.

    int _lastError = 0;                ///< Internal error code (thread-safe, no errno dependency)

public:
    /**
     * @brief Constructor for the local internal memory client manager.
     * @param slave Pointer to the backend loop controller database instance.
     */
    ModbusInternalClient(InternalModbusSlave* slave);

    int coilRead(int id, int address) override;
    int discreteInputRead(int id, int address) override;
    long holdingRegisterRead(int id, int address) override;
    long inputRegisterRead(int id, int address) override;


    int coilWrite(int id, int address, uint8_t value) override;
    int holdingRegisterWrite(int id, int address, uint16_t value) override;

    int registerMaskWrite(int id, int address, uint16_t andMask, uint16_t orMask) override; 

    int beginTransmission(int id, int type, int address, int nb) override;
    void write(unsigned int value) override;
    int endTransmission() override;

    int requestFrom(int id, int type, int address, int nb) override;
    int available() override;
    long read() override;

    const char* lastError() override;
    int lastErrorCode() override;
    void end() override; 
    void setTimeout(unsigned long ms) override; 
     
}; 

#endif