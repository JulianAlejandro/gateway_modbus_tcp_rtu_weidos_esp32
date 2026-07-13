#ifndef MODBUS_INTERNAL_CLIENT_H
#define MODBUS_INTERNAL_CLIENT_H

#include "IModbusClient.h"
#include "InternalModbusSlave.h"

#define CODE_ILEGAL_ADDRES 112345680

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

public:
    /**
     * @brief Constructor for the local internal memory client manager.
     * @param slave Pointer to the backend loop controller database instance.
     */
    ModbusInternalClient(InternalModbusSlave* slave);

    /**
     * @brief Directly toggles a single internal coil state inside the local database map.
     * @return true if the address exists and updated successfully, false otherwise.
     */
    bool coilWrite(uint8_t slaveID, int address, uint8_t value) override;

    /**
     * @brief Overwrites a single internal 16-bit register inside the local database map.
     * @return true if the address exists and accepted the payload value, false otherwise.
     */
    bool holdingRegisterWrite(uint8_t slaveID, int address, uint16_t value) override;

    /**
     * @brief Checks boundaries and prepares local pointers to allocate a multiple write sequence stream.
     * @return true if the requested memory range is entirely within the local limits, false otherwise.
     */
    bool beginTransmission(uint8_t slaveID, int dataType, int address, int quantity) override;

    /**
     * @brief Writes a pending entry value onto the indexed targeted offset from beginTransmission().
     */
    void write(uint16_t value) override;

    /**
     * @brief Flushes modifications and triggers the direct physical hardware/registers updates.
     * @return Always true upon local commit synchronization.
     */
    bool endTransmission() override;

    /**
     * @brief Synchronizes hardware inputs and populates the local streaming buffer with fetched snapshots.
     * @return true if the range is valid and data was cached, false on out-of-bounds mapping errors.
     */
    bool requestFrom(uint8_t slaveID, int dataType, int address, int quantity) override;

    /**
     * @brief Pops the next sequential data point out of the active cached stream queue.
     * @return The 16-bit payload element, or -1 if the local read bounds are exhausted.
     */
    int read() override;

    /**
     * @brief Translates localized errno error state signals back into human-readable text labels.
     */
    const char* lastError() override;
}; 

#endif