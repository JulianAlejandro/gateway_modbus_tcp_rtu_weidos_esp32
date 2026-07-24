#ifndef I_MODBUS_CLIENT_H
#define I_MODBUS_CLIENT_H

#include <Arduino.h>
#include "ArduinoModbus.h"

/*
#define COILS             0
#define DISCRETE_INPUTS   1
#define HOLDING_REGISTERS 2
#define INPUT_REGISTERS   3
*/

#define CODE_ILEGAL_ADDRES 112345680
#define ILEGAL_DATA_VALUE 0x03 // Illegal Data Value
#define SLAVE_DEVICE_FAILURE 0x04 // Default: Slave Device Failure
//#define GATEWAY_TARGET_DEVICE_FAILED //Gateway Target Device Failed to Respond

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
     * @brief Reads the state of a single discrete output (Coil) from a target device (FC 01).
     * * @param id Target slave device address (Slave ID / Unit ID).
     * @param address Zero-indexed coil memory address.
     * @return int State of the coil (1 = ON, 0 = OFF) on success; -1 if the operation failed.
     */
    virtual int coilRead(int id, int address) = 0; 

    /**
     * @brief Reads the state of a single discrete input from a target device (FC 02).
     * * @param id Target slave device address (Slave ID / Unit ID).
     * @param address Zero-indexed discrete input memory address.
     * @return int State of the input (1 = ON, 0 = OFF) on success; -1 if the operation failed.
     */
    virtual int discreteInputRead(int id, int address) = 0; 

    /**
     * @brief Reads the value of a single 16-bit analog output (Holding Register) (FC 03).
     * * @param id Target slave device address (Slave ID / Unit ID).
     * @param address Zero-indexed starting register memory address.
     * @return long Register 16-bit value (0 to 65535) on success; -1 if the operation failed.
     */
    virtual long holdingRegisterRead(int id, int address) = 0; 

    /**
     * @brief Reads the value of a single 16-bit analog input (Input Register) (FC 04).
     * * @param id Target slave device address (Slave ID / Unit ID).
     * @param address Zero-indexed starting register memory address.
     * @return long Register 16-bit value (0 to 65535) on success; -1 if the operation failed.
     */
    virtual long inputRegisterRead(int id, int address) = 0;

    /**
     * @brief Writes a single digital output (Coil) status to a remote slave device (FC 05).
     * * @param id Target slave device address (Slave ID / Unit ID).
     * @param address Target zero-indexed coil register address.
     * @param value Status value to write (0 for OFF, non-zero/1 for ON).
     * @return int Returns 1 if the write transaction succeeded; 0 if the write failed.
     */
    virtual int coilWrite(int id, int address, uint8_t value) = 0;

    /**
     * @brief Writes a single 16-bit analog output (Holding Register) to a remote slave device (FC 06).
     * * @param id Target slave device address (Slave ID / Unit ID).
     * @param address Target zero-indexed holding register address.
     * @param value 16-bit data value payload to transmit.
     * @return int Returns 1 if the write transaction succeeded; 0 if the write failed.
     */
    virtual int holdingRegisterWrite(int id, int address, uint16_t value) = 0;

    /**
     * @brief Safely modifies specific bits of a single holding register using an AND/OR mask (FC 22).
     * * @param id Target slave device address (Slave ID / Unit ID).
     * @param address Target zero-indexed holding register address.
     * @param andMask Bitmask applied to clear existing bits (AND logic).
     * @param orMask Bitmask applied to set new bits (OR logic).
     * @return int Returns 1 if the mask operation succeeded; 0 if the operation failed.
     */
    virtual int registerMaskWrite(int id, int address, uint16_t andMask, uint16_t orMask) = 0;
    
    /**
     * @brief Initializes a buffered stream session for multiple sequential register or coil writes.
     * * Use write() to feed data into the transmit queue, and endTransmission() to broadcast it.
     * * @param id Target slave device address (Slave ID / Unit ID).
     * @param type Memory region to write to (either COILS or HOLDING_REGISTERS).
     * @param address Base zero-indexed starting target memory address.
     * @param nb Total count of sequential data slots requested to write.
     * @return int Returns 1 if the streaming buffer is successfully prepared; 0 if initialization failed.
     */
    virtual int beginTransmission(int id, int type, int address, int nb) = 0;

    /**
     * @brief Pushes a single data unit entry onto the active transmission queue buffer.
     * * @param value The raw 16-bit word data value payload to append (or 0/1 for coils).
     */
    virtual void write(unsigned int value) = 0;

    /**
     * @brief Broadcasts the accumulated buffered payload stream out onto the active bus (FC 15 or FC 16).
     * * @return int Returns 1 if the frame handshake and physical write transaction succeeded; 0 if failed.
     */
    virtual int endTransmission() = 0;

    /**
     * @brief Dispatches a transaction inquiry request frame to fetch structured read boundaries.
     * * Fills an internal FIFO cache. Use available() and read() to extract the received values.
     * * @param id Target slave device address (Slave ID / Unit ID).
     * @param type Memory block source identifier constant (COILS, DISCRETE_INPUTS, HOLDING_REGISTERS, INPUT_REGISTERS).
     * @param address Base zero-indexed starting remote read address cursor.
     * @param nb Total count of data register positions to snapshot.
     * @return int Returns the number of registers successfully read on success; 0 on failure.
     */
    virtual int requestFrom(int id, int type, int address, int nb) = 0;

    /**
     * @brief Queries the number of unread values remaining in the internal FIFO cache.
     * * @return int Count of elements currently available for reading via read().
     */
    virtual int available() = 0;

    /**
     * @brief Fetches and extracts the next sequentially cached data element out of the active data stream.
     * * @return long The next 16-bit word/bit payload component on success; -1 if the buffer is empty.
     */
    virtual long read() = 0; 
    
    /**
     * @brief Retrieves a human-readable text label describing the last captured interface runtime error context.
     * * @return const char* Constant pointer to a character string detailing the error code summary.
     */
    virtual const char* lastError() = 0;

    /**
     * @brief Retrieves the numeric error code from the last failed operation.
     * @return int Error code (0 = no error). Unlike lastError(), this is task-safe
     * because each implementation stores its own error code internally.
     */
    virtual int lastErrorCode() = 0;

    /**
     * @brief Stops active transactions, releases system resources, and closes the client interface.
     */
    virtual void end() = 0;

    /**
     * @brief Configures the response timeout interval for physical network handshake responses.
     * * @param ms Timeout limit specified in milliseconds.
     */
    virtual void setTimeout(unsigned long ms) = 0;
};

#endif