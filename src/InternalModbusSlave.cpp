#include "InternalModbusSlave.h"
#include <Arduino.h> // Necesario para pinMode, digitalRead, analogRead, etc.

InternalModbusSlave internalSlaveID10;

// Definición fija de pines para el Weidos ESP32 A1
const uint32_t InternalModbusSlave::pinCoils[NUM_COILS] = {DO_0, DO_1, DO_2, DO_3};
const uint32_t InternalModbusSlave::pinDiscreteInputs[NUM_DISCRETE] = {DI_4, DI_5, DI_6, DI_7};
const uint32_t InternalModbusSlave::pinInputRegisters[NUM_INPUT_REGS] = {ADI_0, ADI_1, ADI_2, ADI_3};
const uint32_t InternalModbusSlave::pinHoldingRegisters[NUM_HOLDING_REGS] = {AO_0};

InternalModbusSlave::InternalModbusSlave() : _mapping(nullptr) {}

InternalModbusSlave::~InternalModbusSlave() {
    if (_mapping != nullptr) {
        modbus_mapping_free(_mapping); //liberacion de memoria dinamica
    }
}

// Inicializa los 4 bloques de memoria y configura los pines físicos
bool InternalModbusSlave::begin() {
    
    _mapping = modbus_mapping_new(NUM_COILS, NUM_DISCRETE, NUM_HOLDING_REGS, NUM_INPUT_REGS);
    if (_mapping == nullptr) return false;

    // Configuración inicial de hardware (Dirección de pines)
    for (int i = 0; i < NUM_COILS; i++) {
        pinMode(pinCoils[i], OUTPUT);
    }
    for (int i = 0; i < NUM_DISCRETE; i++) {
        pinMode(pinDiscreteInputs[i], INPUT);
    }
    for (int i = 0; i < NUM_HOLDING_REGS; i++) {
        pinMode(pinHoldingRegisters[i], OUTPUT);
    }
    
    // Inicializar registros espejo a 0
    memset(_shadowCoils, 0, sizeof(_shadowCoils));
    memset(_shadowHoldingRegs, 0, sizeof(_shadowHoldingRegs));
    
    updatePhysicalIO();

    return true;
}

void InternalModbusSlave::updatePhysicalIO() {
    if (!_mapping) return;

    // 1. Leer Entradas Digitales Físicas -> Guardar en Discrete Inputs
    for (int i = 0; i < NUM_DISCRETE && i < _mapping->nb_input_bits; i++) {
        _mapping->tab_input_bits[i] = digitalRead(pinDiscreteInputs[i]);
    }

    // 2. Leer Entradas Analógicas Físicas -> Guardar en Input Registers
    for (int i = 0; i < NUM_INPUT_REGS && i < _mapping->nb_input_registers; i++) {
        _mapping->tab_input_registers[i] = analogRead(pinInputRegisters[i]);
    }

    // 3. Sincronización BIDIRECCIONAL Inteligente de Coils (Salidas)
    for (int i = 0; i < NUM_COILS && i < _mapping->nb_bits; i++) {
        uint8_t currentModbusValue = _mapping->tab_bits[i];

        if (currentModbusValue != _shadowCoils[i]) {
            // Caso A: Modbus cambió el valor (Prioridad alta por red)
            digitalWrite(pinCoils[i], currentModbusValue ? HIGH : LOW);
            _shadowCoils[i] = currentModbusValue; // Sincronizar espejo
        } else {
            // Caso B: Modbus no ha cambiado. Verificamos si cambió por código local/HW
            uint8_t currentHwValue = digitalRead(pinCoils[i]);
            if (currentHwValue != _shadowCoils[i]) {
                _mapping->tab_bits[i] = currentHwValue; // Actualizar Modbus
                _shadowCoils[i] = currentHwValue;       // Sincronizar espejo
            }
        }
    }

    // 4. Sincronización BIDIRECCIONAL Inteligente de Holding Registers (Salidas Analógicas)
    for (int i = 0; i < NUM_HOLDING_REGS && i < _mapping->nb_registers; i++) {
        uint16_t currentModbusValue = _mapping->tab_registers[i];
        if (currentModbusValue > VOLTAGE_RESOLUTION) currentModbusValue = VOLTAGE_RESOLUTION;

        if (currentModbusValue != _shadowHoldingRegs[i]) {
            // Caso A: Modbus cambió el registro
            analogWrite(pinHoldingRegisters[i], currentModbusValue);
            _shadowHoldingRegs[i] = currentModbusValue;

        } else { // TODO: problemas, no podemos leer facilmente un analogOutput
            // Caso B: Modbus no ha cambiado. Verificamos si cambió por código local/HW
           // uint16_t currentHwValue = analogRead(pinHoldingRegisters[i]);
           // if (currentHwValue != _shadowHoldingRegs[i]) {
           //     _mapping->tab_registers[i] = currentHwValue; // Actualizar Modbus
           //     _shadowHoldingRegs[i] = currentHwValue;       // Sincronizar espejo
           // }
        }
    }
}

// Métodos de lectura y escritura de registros internos (Sin cambios)
uint8_t InternalModbusSlave::readCoil(int address) { 
    if (!_mapping || address < 0 || address >= NUM_COILS) {
        return 0; // Dirección fuera de rango o mapa no inicializado
    }
    return _mapping->tab_bits[address]; 
}

uint8_t InternalModbusSlave::readDiscreteInput(int address) { 
    if (!_mapping || address < 0 || address >= NUM_DISCRETE) {
        return 0; 
    }
    return _mapping->tab_input_bits[address]; 
}

uint16_t InternalModbusSlave::readHoldingRegister(int address) {
    if (!_mapping || address < 0 || address >= NUM_HOLDING_REGS) {
        return 0; 
    }
    return _mapping->tab_registers[address]; 
}

uint16_t InternalModbusSlave::readInputRegister(int address) { 
    if (!_mapping || address < 0 || address >= NUM_INPUT_REGS) {
        return 0; 
    }
    return _mapping->tab_input_registers[address]; 
}

bool InternalModbusSlave::writeSinglecoil(int address, bool value) { 
    if (!_mapping || address < 0 || address >= NUM_COILS) {
        return false; 
    }
    _mapping->tab_bits[address] = value ? 1 : 0;
    return true;  
}

bool InternalModbusSlave::writeSingleRegister(int address, uint16_t value) { 
    if (!_mapping || address < 0 || address >= NUM_HOLDING_REGS) {
        return false; 
    }
    _mapping->tab_registers[address] = value; 
    return true; 
}


