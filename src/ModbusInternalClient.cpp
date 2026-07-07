#include "ModbusInternalClient.h"
#include "esp_log.h"

static const char* TAG = "MB_INT_CLIENT";

ModbusInternalClient::ModbusInternalClient(InternalModbusSlave* slave) 
    : _slave(slave), _bufferIndex(0), _bufferLength(0), _writeIndex(0) {}

bool ModbusInternalClient::coilWrite(uint8_t slaveID, int address, uint8_t value){
    ESP_LOGD(TAG, "Solicitud de escritura de coil slave ID = %u , adress = %i , valor = %u \n", slaveID, address, value); 
    _slave->writeSinglecoil(address, value != 0);
    _slave->updatePhysicalIO(); // Sincroniza hardware inmediatamente
    return true;
}

bool ModbusInternalClient::holdingRegisterWrite(uint8_t slaveID, int address, uint16_t value){
    _slave->writeSingleRegister(address, value);
    _slave->updatePhysicalIO();
    return true;
}

bool ModbusInternalClient::beginTransmission(uint8_t slaveID, int dataType, int address, int quantity){
    _writeDataType = dataType;
    _writeAddress = address;
    _writeIndex = 0;
    return true;
}

void ModbusInternalClient::write(uint16_t value){
    // En tu código original pasabas dataType (HOLDING_REGISTERS o COILS)
    // Nota: Asegúrate de que las macros COILS / HOLDING_REGISTERS estén accesibles
    if (_writeDataType == COILS) { // COILS (Depende del valor de tu macro)
        _slave->writeSinglecoil(_writeAddress + _writeIndex, value != 0);
    } else { // HOLDING
        _slave->writeSingleRegister(_writeAddress + _writeIndex, value);
    }
    _writeIndex++;
}

bool ModbusInternalClient::endTransmission(){
    _slave->updatePhysicalIO();
    return true;
}

bool ModbusInternalClient::requestFrom(uint8_t slaveID, int dataType, int address, int quantity){

     ESP_LOGD(TAG, "Solicitud de lectura slave ID = %u , dataType = %i , adress = %i ,  quantity = %i \n", slaveID, dataType, address, quantity); 
    _slave->updatePhysicalIO(); // Refrescar antes de leer
    _bufferLength = quantity;
    _bufferIndex = 0;

    for (int i = 0; i < quantity; i++) {
        switch (dataType) {
            case COILS: 
                _bufferRead[i] = _slave->readCoil(address + i); 
                break; 
            case DISCRETE_INPUTS: 
                _bufferRead[i] = _slave->readDiscreteInput(address + i); 
                break;
            case HOLDING_REGISTERS: 
                _bufferRead[i] = _slave->readHoldingRegister(address + i); 
                break;
            case INPUT_REGISTERS: 
                _bufferRead[i] = _slave->readInputRegister(address + i); 
                break;
            default: 
                _bufferRead[i] = 0; 
                break;
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

const char* ModbusInternalClient::lastError(){ 
    return "Internal Slave Error"; 
}