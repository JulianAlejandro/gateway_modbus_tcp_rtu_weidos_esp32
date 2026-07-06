

#ifndef INTERNAL_MODBUS_SLAVE_H
#define INTERNAL_MODBUS_SLAVE_H

extern "C" {
  #include "libmodbus/modbus.h"
}

class InternalModbusSlave {
private:
    modbus_mapping_t* _mapping;

public:
    InternalModbusSlave();

    // Inicializa los 4 bloques de memoria
    bool begin(int nb_coils, int nb_discrete, int nb_holding, int nb_input);

    // Refresca las entradas y aplica las salidas físicas
    void updatePhysicalIO();

    uint8_t coilRead(int address);
    uint8_t discreteInputRead(int address);
    uint16_t holdingRegisterRead(int address);
    uint16_t inputRegisterRead(int address);

    // Métodos para actualizar datos internos por software
    void holdingRegisterWrite(int address, uint16_t value);
    void coilWrite(int address, bool value);

    ~InternalModbusSlave();
};

// Instancia global para tu ID 10
extern InternalModbusSlave esclavo10;

#endif

