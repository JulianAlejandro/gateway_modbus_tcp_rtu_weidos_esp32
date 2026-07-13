#ifndef INTERNAL_MODBUS_SLAVE_H
#define INTERNAL_MODBUS_SLAVE_H

extern "C" {
  #include "libmodbus/modbus.h"
}

/**
 * @class InternalModbusSlave
 * @brief Memory-mapped Modbus slave that mirrors physical ESP32 I/O pins into raw libmodbus register tables.
 * * This class provides bidirectional synchronization between physical hardware (GPIOs, ADCs, DACs/PWM)
 * and virtual Modbus registers, facilitating seamless local processing and remote data mapping.
 */
class InternalModbusSlave {

public: 
    static constexpr uint8_t NUM_COILS = 4;             ///< Allocated size for Digital Outputs (Coils).
    static constexpr uint8_t NUM_DISCRETE = 4;          ///< Allocated size for Digital Inputs (Discrete Inputs).
    static constexpr uint8_t NUM_INPUT_REGS = 4;         ///< Allocated size for Analog Inputs (Input Registers).
    static constexpr uint8_t NUM_HOLDING_REGS = 1;       ///< Allocated size for Analog Outputs (Holding Registers).
    static constexpr uint16_t VOLTAGE_RESOLUTION = 1023; ///< Maximum scaling bounds threshold limit for analog outputs.

private:
    modbus_mapping_t* _mapping;                          ///< Core libmodbus structural dynamic data tables pointer.

    uint8_t _shadowCoils[NUM_COILS];                     ///< Shadow mirror array to trap data changes on Digital Outputs.
    uint16_t _shadowHoldingRegs[NUM_HOLDING_REGS];       ///< Shadow mirror array to trap data changes on Analog Outputs.

    static const uint32_t pinCoils[NUM_COILS];           ///< Assigned physical GPIO pins array for digital outputs.
    static const uint32_t pinDiscreteInputs[NUM_DISCRETE];     ///< Assigned physical GPIO pins array for digital inputs.
    static const uint32_t pinInputRegisters[NUM_INPUT_REGS];   ///< Assigned physical ADC channels array for analog inputs.
    static const uint32_t pinHoldingRegisters[NUM_HOLDING_REGS]; ///< Assigned physical PWM/DAC channels array for analog outputs.

public:
    /**
     * @brief Constructs the internal slave hardware-mapping controller instance.
     */
    InternalModbusSlave();

    /**
     * @brief Allocates libmodbus structures and configures physical hardware pin directions.
     * @return true if memory allocation succeeded, false on critical errors.
     */
    bool begin();

    /**
     * @brief Performs a bidirectional synchronization sweep between the physical hardware and Modbus registers.
     * * Reads physical inputs (Digital/Analog) into the Modbus registry tables, and pushes changes 
     * from Modbus write requests down to physical outputs using shadow-mirror comparison filtering.
     */
    void updatePhysicalIO();

    /**
     * @brief Directly reads a localized value out of the raw Modbus Coils table (FC 01).
     * @param address Zero-indexed destination register layout offset.
     * @return 1 if ON/active, 0 if OFF or out-of-bounds.
     */
    uint8_t readCoil(int address);

    /**
     * @brief Directly reads a localized value out of the raw Modbus Discrete Inputs table (FC 02).
     * @param address Zero-indexed destination register layout offset.
     * @return 1 if ON/active, 0 if OFF or out-of-bounds.
     */
    uint8_t readDiscreteInput(int address);

    /**
     * @brief Directly reads a localized value out of the raw Modbus Holding Registers table (FC 03).
     * @param address Zero-indexed destination register layout offset.
     * @return The cached 16-bit word data value, or 0 if out-of-bounds.
     */
    uint16_t readHoldingRegister(int address);

    /**
     * @brief Directly reads a localized value out of the raw Modbus Input Registers table (FC 04).
     * @param address Zero-indexed destination register layout offset.
     * @return The cached 16-bit word data value, or 0 if out-of-bounds.
     */
    uint16_t readInputRegister(int address);

    /**
     * @brief Directly updates a localized target value inside the Modbus Coils table (FC 05).
     * @param address Zero-indexed target register layout offset.
     * @param value Boolean activation state payload to assign.
     * @return true if address is valid and accepted the state change, false otherwise.
     */
    bool writeSinglecoil(int address, bool value);

    /**
     * @brief Directly updates a localized target value inside the Modbus Holding Registers table (FC 06).
     * @param address Zero-indexed target register layout offset.
     * @param value 16-bit word data packet to write.
     * @return true if address is valid and accepted the payload data, false otherwise.
     */
    bool writeSingleRegister(int address, uint16_t value); 
    
    /**
     * @brief Destructor that deallocates dynamically instantiated libmodbus tracking maps.
     */
    ~InternalModbusSlave();

    uint16_t getCoilsCount() const { return NUM_COILS; }
    uint16_t getDiscreteInputsCount() const { return NUM_DISCRETE; }
    uint16_t getInputRegistersCount() const { return NUM_INPUT_REGS; }
    uint16_t getHoldingRegistersCount() const { return NUM_HOLDING_REGS; }
};

/**
 * @brief Global extern reference hook pointing to Slave Instance ID 10.
 */
extern InternalModbusSlave internalSlaveID10;

#endif