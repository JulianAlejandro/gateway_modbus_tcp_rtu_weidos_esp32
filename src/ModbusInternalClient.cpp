#include "ModbusInternalClient.h"
#include "esp_log.h"
#include <errno.h> 

static const char* TAG = "MB_INT_CLIENT";

ModbusInternalClient::ModbusInternalClient(InternalModbusSlave* slave) 
    : _slave(slave), _bufferIndex(0), _bufferLength(0), _writeIndex(0) {}

bool ModbusInternalClient::coilWrite(uint8_t slaveID, int address, uint8_t value){

    if(!_slave->writeSinglecoil(address, value != 0)){
        ESP_LOGE(TAG, "Escritura de Coil rechazada por el esclavo. Direccion: %i", address);
        errno = EINVAL;
        return false; 
    }
    _slave->updatePhysicalIO(); // Sincroniza hardware inmediatamente
    return true;
}

bool ModbusInternalClient::holdingRegisterWrite(uint8_t slaveID, int address, uint16_t value){
    if (!_slave->writeSingleRegister(address, value)) {
        ESP_LOGE(TAG, "Escritura de Register rechazada por el esclavo. Dirección: %i", address);
        errno = EINVAL; 
        return false;
    }
    _slave->updatePhysicalIO();
    return true;
}

bool ModbusInternalClient::beginTransmission(uint8_t slaveID, int dataType, int address, int quantity){
    // Usamos los nuevos métodos de consulta del Slave para validar bloques antes de transmitir
    int maxCount = 0;
    if (dataType == COILS) maxCount = _slave->getCoilsCount();
    else maxCount = _slave->getHoldingRegistersCount();

    if (address < 0 || (address + quantity) > maxCount) {
        ESP_LOGE(TAG, "Bloque de escritura fuera de rango. Inicio: %i, Cant: %i", address, quantity);
        errno = 112345680;
        return false; 
    }

    _writeDataType = dataType;
    _writeAddress = address;
    _writeIndex = 0;
    return true;
}

void ModbusInternalClient::write(uint16_t value){
    if (_writeDataType == COILS) { 
        _slave->writeSinglecoil(_writeAddress + _writeIndex, value != 0);
    } else { 
        _slave->writeSingleRegister(_writeAddress + _writeIndex, value);
    }
    _writeIndex++;
}

bool ModbusInternalClient::endTransmission(){
    _slave->updatePhysicalIO();
    return true;
}

bool ModbusInternalClient::requestFrom(uint8_t slaveID, int dataType, int address, int quantity){
    // Validamos el rango de lectura usando los límites del Slave
    int maxCount = 0;
    switch (dataType) {
        case COILS:             maxCount = _slave->getCoilsCount(); break;
        case DISCRETE_INPUTS:   maxCount = _slave->getDiscreteInputsCount(); break;
        case HOLDING_REGISTERS: maxCount = _slave->getHoldingRegistersCount(); break;
        case INPUT_REGISTERS:    maxCount = _slave->getInputRegistersCount(); break;
        default: maxCount = 0; break;
    }

    if (address < 0 || (address + quantity) > maxCount) {
        ESP_LOGE(TAG, "Petición de lectura fuera de rango en mapa de memoria interna.");
        errno = 112345680; 
        return false;   
    }

    _slave->updatePhysicalIO(); 
    _bufferLength = quantity;
    _bufferIndex = 0;

    for (int i = 0; i < quantity; i++) {
        switch (dataType) {
            case COILS:           _bufferRead[i] = _slave->readCoil(address + i); break; 
            case DISCRETE_INPUTS: _bufferRead[i] = _slave->readDiscreteInput(address + i); break;
            case HOLDING_REGISTERS: _bufferRead[i] = _slave->readHoldingRegister(address + i); break;
            case INPUT_REGISTERS:  _bufferRead[i] = _slave->readInputRegister(address + i); break;
        }
    }
    return true;
}

int ModbusInternalClient::read(){
    if (_bufferIndex < _bufferLength) {
        return _bufferRead[_bufferIndex++];
    }
    return -1;
}

const char* ModbusInternalClient::lastError() { 
    if (errno == 0) {
        return ""; 
    }
    if (errno == EINVAL) {
        return "Modbus Exception: Illegal Data Address (Internal Slave)";
    }
    return modbus_strerror(errno); 
}