#ifndef I_MODBUS_CLIENT_H
#define I_MODBUS_CLIENT_H

#include <Arduino.h>
#include "ArduinoModbus.h"

/**
 * @class IModbusClient
 * @brief Interface abstraction for Modbus client operations.
 * * Decouples high-level gateway logic from specific hardware implementations,
 * allowing uniform access to physical RTU buses, TCP networks, or internal memory maps.
 */
class IModbusClient {
public:
    /**
     * @brief Virtual destructor to ensure correct resource cleanup in derived classes.
     */
    virtual ~IModbusClient() {}

    /**
     * @brief Writes a single coil status to a remote slave device.
     * @param slaveID Target Modbus device address.
     * @param address Target zero-indexed coil register address.
     * @param value Status value (0 for OFF, non-zero for ON).
     * @return true if the write transaction succeeded, false otherwise.
     */
    virtual bool coilWrite(uint8_t slaveID, int address, uint8_t value) = 0;

    /**
     * @brief Writes a single 16-bit holding register word to a remote slave device.
     * @param slaveID Target Modbus device address.
     * @param address Target zero-indexed holding register address.
     * @param value 16-bit data value payload to transmit.
     * @return true if the write transaction succeeded, false otherwise.
     */
    virtual bool holdingRegisterWrite(uint8_t slaveID, int address, uint16_t value) = 0;
    
    /**
     * @brief Initializes a buffered stream session for multiple register or coil writes.
     * @param slaveID Target Modbus device address.
     * @param dataType Memory region identification constant (e.g., COILS, HOLDING_REGISTERS).
     * @param address Base zero-indexed starting target memory position.
     * @param quantity Total amount of sequential data slots requested to write.
     * @return true if the transmission framework is successfully prepared, false otherwise.
     */
    virtual bool beginTransmission(uint8_t slaveID, int dataType, int address, int quantity) = 0;

    /**
     * @brief Pushes a single data unit entry onto the active transmission queue buffer.
     * @param value The raw 16-bit word data value payload to append.
     */
    virtual void write(uint16_t value) = 0;

    /**
     * @brief Broadcasts the accumulated transaction stream frame payload out onto the active bus.
     * @return true if the frame transmission handshake and verification succeeded, false otherwise.
     */
    virtual bool endTransmission() = 0;

    /**
     * @brief Dispatches a transaction inquiry request frame to fetch structured read boundaries.
     * @param slaveID Target Modbus device address.
     * @param dataType Memory block source descriptor identifier constant.
     * @param address Base zero-indexed starting remote read address cursor.
     * @param quantity Total count of data register positions to snapshot.
     * @return true if the response validation and payload caching succeeded, false otherwise.
     */
    virtual bool requestFrom(uint8_t slaveID, int dataType, int address, int quantity) = 0;

    /**
     * @brief Fetches and increments the next sequentially cached data element out of the active data stream.
     * @return The 16-bit word payload component, or -1 if the response buffer bounds are empty.
     */
    virtual int read() = 0; 
    
    /**
     * @brief Retrieves a human-readable text label describing the last captured interface runtime error context.
     * @return Constant pointer to a character string detailing the error code summary.
     */
    virtual const char* lastError() = 0;
};

#endif