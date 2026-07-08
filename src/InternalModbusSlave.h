

#ifndef INTERNAL_MODBUS_SLAVE_H
#define INTERNAL_MODBUS_SLAVE_H

extern "C" {
  #include "libmodbus/modbus.h"
}

class InternalModbusSlave {

public: 
    static constexpr uint8_t NUM_COILS = 4;
    static constexpr uint8_t NUM_DISCRETE = 4;
    static constexpr uint8_t NUM_INPUT_REGS = 4;
    static constexpr uint8_t NUM_HOLDING_REGS = 1;
    static constexpr uint16_t VOLTAGE_RESOLUTION = 1023;

private:
    modbus_mapping_t* _mapping;

    uint8_t _shadowCoils[NUM_COILS];          // variables auxiliares para deteccion de cambios en variables output 
    uint16_t _shadowHoldingRegs[NUM_HOLDING_REGS];

    static const uint32_t pinCoils[NUM_COILS];
    static const uint32_t pinDiscreteInputs[NUM_DISCRETE];
    static const uint32_t pinInputRegisters[NUM_INPUT_REGS];
    static const uint32_t pinHoldingRegisters[NUM_HOLDING_REGS];

public:
    InternalModbusSlave();

    // Inicializa los 4 bloques de memoria
    bool begin();

    // Refresca las entradas y aplica las salidas físicas
    void updatePhysicalIO();

    uint8_t readCoil(int address); // function code 0x01
    uint8_t readDiscreteInput(int address); // function code 0x02
    uint16_t readHoldingRegister(int address); // function code 0x03
    uint16_t readInputRegister(int address); // function code 0x04

    // Métodos para actualizar datos internos por software
    bool writeSinglecoil(int address, bool value); // function code 0x05
    bool writeSingleRegister(int address, uint16_t value); // function code 0x06 
    
    ~InternalModbusSlave();

    uint16_t getCoilsCount() const { return NUM_COILS; }
    uint16_t getDiscreteInputsCount() const { return NUM_DISCRETE; }
    uint16_t getInputRegistersCount() const { return NUM_INPUT_REGS; }
    uint16_t getHoldingRegistersCount() const { return NUM_HOLDING_REGS; }
};

// Instancia global para tu ID 10
extern InternalModbusSlave internalSlaveID10;

#endif

