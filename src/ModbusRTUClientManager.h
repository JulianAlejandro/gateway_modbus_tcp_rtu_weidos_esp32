#ifndef MODBUS_RTU_CLIENT_MANAGER_H
#define MODBUS_RTU_CLIENT_MANAGER_H

#include "IModbusClient.h"
#include <ModbusRTUClient.h>

/**
 * @class ModbusRtuClientManager
 * @brief Wrapper class that implements the IModbusClient interface using ModbusRTUClientClass.
 * * This class acts as an adapter layer to decouple the Modbus TCP bridge or other components
 * from the concrete Arduino ModbusRTUClient implementation, allowing for easier testing and hardware swapping.
 */
class ModbusRtuClientManager : public IModbusClient {
private:
    ModbusRTUClientClass* _rtu; ///< Pointer to the underlying Arduino Modbus RTU hardware client instance.

public:
    /**
     * @brief Constructor for the Modbus RTU Client Manager wrapper.
     * @param rtu Pointer to the concrete ModbusRTUClientClass instance to be wrapped.
     */
    ModbusRtuClientManager(ModbusRTUClientClass* rtu);

    /**
     * @brief Writes a single coil value to a remote Modbus RTU slave device.
     * @param slaveID Target Modbus slave address.
     * @param address Zero-indexed target register address.
     * @param value Status value to write (0x00 for OFF, 0x01 for ON).
     * @return true if the write transaction succeeded, false otherwise.
     */
    bool coilWrite(uint8_t slaveID, int address, uint8_t value) override;

    /**
     * @brief Writes a single holding register to a remote Modbus RTU slave device.
     * @param slaveID Target Modbus slave address.
     * @param address Zero-indexed target register address.
     * @param value 16-bit word data value to write.
     * @return true if the write transaction succeeded, false otherwise.
     */
    bool holdingRegisterWrite(uint8_t slaveID, int address, uint16_t value) override;
    
    /**
     * @brief Prepares and begins a multiple registers/coils write transmission payload buffer.
     * @param slaveID Target Modbus slave address.
     * @param dataType Target memory type constant (e.g., COILS, HOLDING_REGISTERS).
     * @param address Starting zero-indexed register address.
     * @param quantity Total count of registers or coils to write.
     * @return true if the transmission stream initialization succeeded, false otherwise.
     */
    bool beginTransmission(uint8_t slaveID, int dataType, int address, int quantity) override;
    
    /**
     * @brief Enqueues a single 16-bit word value into the active transmission broadcast buffer.
     * @param value The 16-bit data value to append to the payload.
     */
    void write(uint16_t value) override;

    /**
     * @brief Closes the transmission sequence and physically broadcasts the transaction payload over the RTU bus.
     * @return true if the packet transmission and response handshake succeeded, false otherwise.
     */
    bool endTransmission() override;

    /**
     * @brief Sends a read request for discrete/register inputs data from a remote Modbus RTU slave device.
     * @param slaveID Target Modbus slave address.
     * @param dataType Target memory type constant (e.g., HOLDING_REGISTERS, INPUT_REGISTERS).
     * @param address Starting zero-indexed remote address.
     * @param quantity Total count of data units requested to fetch.
     * @return true if the slave response validation succeeded, false otherwise.
     */
    bool requestFrom(uint8_t slaveID, int dataType, int address, int quantity) override;

    /**
     * @brief Retrieves the next available data unit from the local incoming transaction response stream buffer.
     * @return The read integer data value, or -1 if the buffer stream is empty.
     */
    int read() override;

    /**
     * @brief Fetches a descriptive text representation of the last encountered RTU bus error state.
     * @return Pointer to a constant character string explaining the error reason.
     */
    const char* lastError() override;
};
#endif