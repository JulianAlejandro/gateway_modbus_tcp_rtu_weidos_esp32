#include "InternalModbusSlave.h"
#include <Arduino.h> // Necesario para pinMode, digitalRead, analogRead, etc.

InternalModbusSlave esclavo10;

// Definición fija de pines para el Weidos ESP32 A1
static const uint8_t NUM_COILS = 4;
static const uint32_t pinCoils[NUM_COILS] = {DO_0, DO_1, DO_2, DO_3}; // Salidas digitales

static const uint8_t NUM_DISCRETE = 4;
static const uint32_t pinDiscreteInputs[NUM_DISCRETE] = {DI_4, DI_5, DI_6, DI_7}; // Entradas digitales

static const uint8_t NUM_INPUT_REGS = 4;
static const uint32_t pinInputRegisters[NUM_INPUT_REGS] = {ADI_0, ADI_1, ADI_2, ADI_3}; // Entradas analógicas híbridas

static const uint8_t NUM_HOLDING_REGS = 1;
static const uint32_t pinHoldingRegisters[NUM_HOLDING_REGS] = {AO_0}; // Salida analógica

static const uint16_t VOLTAGE_RESOLUTION = 1023;

InternalModbusSlave::InternalModbusSlave() : _mapping(nullptr) {}

InternalModbusSlave::~InternalModbusSlave() {
    if (_mapping != nullptr) {
        modbus_mapping_free(_mapping);
    }
}

// Inicializa los 4 bloques de memoria y configura los pines físicos
bool InternalModbusSlave::begin(int nb_coils, int nb_discrete, int nb_holding, int nb_input) {
    
    _mapping = modbus_mapping_new(nb_coils, nb_discrete, nb_holding, nb_input);
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
    // Nota: los pines analógicos de entrada no requieren pinMode(INPUT) estricto en ESP32

    // --- VALORES INICIALES POR DEFECTO PARA PRUEBAS ---
    // Inicializamos a cero las salidas y hacemos una lectura inicial de entradas
    updatePhysicalIO();

    return true;
}

// SINCRONIZACIÓN EN TIEMPO REAL ENTRE HARDWARE Y MEMORIA MODBUS
void InternalModbusSlave::updatePhysicalIO() {
    if (!_mapping) return;

    // 1. Leer Entradas Digitales Físicas -> Guardar en Discrete Inputs (0x02)
    for (int i = 0; i < NUM_DISCRETE && i < _mapping->nb_input_bits; i++) {
        _mapping->tab_input_bits[i] = digitalRead(pinDiscreteInputs[i]);
    }

    // 2. Leer Entradas Analógicas Físicas -> Guardar en Input Registers (0x04)
    for (int i = 0; i < NUM_INPUT_REGS && i < _mapping->nb_input_registers; i++) {
        _mapping->tab_input_registers[i] = analogRead(pinInputRegisters[i]);
    }

    // 3. Leer de Memoria Coils Modbus -> Escribir en Salidas Digitales Físicas (0x01 / 0x05 / 0x0F)
    for (int i = 0; i < NUM_COILS && i < _mapping->nb_bits; i++) {
        digitalWrite(pinCoils[i], _mapping->tab_bits[i] ? HIGH : LOW);
    }

    // 4. Leer de Memoria Holding Regs Modbus -> Escribir en Salida Analógica Física (0x03 / 0x06 / 0x10)
    for (int i = 0; i < NUM_HOLDING_REGS && i < _mapping->nb_registers; i++) {
        uint16_t val = _mapping->tab_registers[i];
        if (val > VOLTAGE_RESOLUTION) val = VOLTAGE_RESOLUTION;
        analogWrite(pinHoldingRegisters[i], val);
    }
}

// Métodos de lectura y escritura de registros internos (Sin cambios)
uint8_t InternalModbusSlave::coilRead(int address) { 
    return _mapping ? _mapping->tab_bits[address] : 0; 
}

uint8_t InternalModbusSlave::discreteInputRead(int address) { 
    return _mapping ? _mapping->tab_input_bits[address] : 0; 
}

uint16_t InternalModbusSlave::holdingRegisterRead(int address) {
     return _mapping ? _mapping->tab_registers[address] : 0; 
}

uint16_t InternalModbusSlave::inputRegisterRead(int address) { 
    return _mapping ? _mapping->tab_input_registers[address] : 0; 
}

void InternalModbusSlave::coilWrite(int address, bool value) { 
    if (_mapping) _mapping->tab_bits[address] = value ? 1 : 0; 
}

void InternalModbusSlave::holdingRegisterWrite(int address, uint16_t value) { 
    if (_mapping) _mapping->tab_registers[address] = value; 
}


