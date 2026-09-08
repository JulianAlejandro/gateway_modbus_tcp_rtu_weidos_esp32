#include "ModbusInternalClient.h"
#include "esp_log.h"

static const char* TAG = "MB_INT_CLIENT";

/*
ModbusInternalClient::ModbusInternalClient(InternalModbusSlave* slave) 
    : _slave(slave), _bufferIndex(0), _bufferLength(0), _writeIndex(0) {}
*/
ModbusInternalClient::ModbusInternalClient(InternalModbusSlave* slave) 
    : _slave(slave), _bufferIndex(0), _bufferLength(0), _writeIndex(0), 
      _writeDataType(0), _writeAddress(0), _lastError(0){
    if (_slave == nullptr) {
        ESP_LOGE(TAG, "CRITICAL: InternalModbusSlave pointer is NULL!");
    }
}

//no usado
int ModbusInternalClient::coilRead(int id, int address) {
    if (!_slave) { _lastError = EINVAL; return -1; }
    if (address < 0 || address >= _slave->getCoilsCount()) {
        ESP_LOGE(TAG, "Coil read out of range. Address: %d", address);
        _lastError = CODE_ILLEGAL_ADDRES;
        return -1;
    }
    _slave->updatePhysicalIO();
    return _slave->readCoil(address);
}

//no usado
int ModbusInternalClient::discreteInputRead(int id, int address) {
    if (!_slave) { _lastError = EINVAL; return -1; }
    if (address < 0 || address >= _slave->getDiscreteInputsCount()) {
        ESP_LOGE(TAG, "Discrete input read out of range. Address: %d", address);
        _lastError = CODE_ILLEGAL_ADDRES;
        return -1;
    }
    _slave->updatePhysicalIO();
    return _slave->readDiscreteInput(address);
}

//no usado
long ModbusInternalClient::holdingRegisterRead(int id, int address) {
    if (!_slave) { _lastError = EINVAL; return -1; }
    if (address < 0 || address >= _slave->getHoldingRegistersCount()) {
        ESP_LOGE(TAG, "Holding register read out of range. Address: %d", address);
        _lastError = CODE_ILLEGAL_ADDRES;
        return -1;
    }
    _slave->updatePhysicalIO();
    return _slave->readHoldingRegister(address);
}

//no usado
long ModbusInternalClient::inputRegisterRead(int id, int address) {
    if (!_slave) { _lastError = EINVAL; return -1; }
    if (address < 0 || address >= _slave->getInputRegistersCount()) {
        ESP_LOGE(TAG, "Input register read out of range. Address: %d", address);
        _lastError = CODE_ILLEGAL_ADDRES;
        return -1;
    }
    _slave->updatePhysicalIO();
    return _slave->readInputRegister(address);
}


int ModbusInternalClient::coilWrite(int id, int address, uint8_t value) {
    if (!_slave) { _lastError = EINVAL; return 0; }
    if (!_slave->writeSinglecoil(address, value != 0)) {
        ESP_LOGE(TAG, "Coil write rejected by slave. Address: %d", address);
        _lastError = CODE_ILLEGAL_ADDRES; 
        return 0; // 0 = Fallo en escritura
    }
    _slave->updatePhysicalIO();
    return 1; // 1 = Éxito
}


int ModbusInternalClient::holdingRegisterWrite(int id, int address, uint16_t value){
    if (!_slave) { _lastError = EINVAL; return 0; }
    if (!_slave->writeSingleRegister(address, value)) {
        ESP_LOGE(TAG, "Register write rejected by slave. Address: %i", address);
        _lastError = EINVAL; 
        return 0;
    }
    _slave->updatePhysicalIO();
    return 1;
}

//no usado
int ModbusInternalClient::registerMaskWrite(int id, int address, uint16_t andMask, uint16_t orMask) {
    if (!_slave) { _lastError = EINVAL; return 0; }
    if (address < 0 || address >= _slave->getHoldingRegistersCount()) {
        ESP_LOGE(TAG, "Mask write address out of range: %d", address);
        _lastError = CODE_ILLEGAL_ADDRES;
        return 0;
    }
    
    // Leer valor actual, aplicar máscaras y volver a escribir
    uint16_t currentVal = _slave->readHoldingRegister(address);
    uint16_t newVal = (currentVal & andMask) | orMask;
    
    if (!_slave->writeSingleRegister(address, newVal)) {
        _lastError = EINVAL;
        return 0;
    }
    _slave->updatePhysicalIO();
    return 1;
}

int ModbusInternalClient::beginTransmission(int id, int type, int address, int nb) {
    if (!_slave) { _lastError = EINVAL; return 0; }
    int maxCount = 0;
    if (type == COILS) maxCount = _slave->getCoilsCount();
    else if (type == HOLDING_REGISTERS) maxCount = _slave->getHoldingRegistersCount();
    else {
        _lastError = EINVAL;
        return 0;
    }

    if (address < 0 || (address + nb) > maxCount) {
        ESP_LOGE(TAG, "Write block out of range. Start: %d, Count: %d", address, nb);
        _lastError = CODE_ILLEGAL_ADDRES;
        return 0; 
    }

    _writeDataType = type;
    _writeAddress = address;
    _writeIndex = 0;
    return 1; // Listo para escribir
}

void ModbusInternalClient::write(unsigned int value) {
    if (!_slave) return;
    if (_writeDataType == COILS) { 
        _slave->writeSinglecoil(_writeAddress + _writeIndex, value != 0);
    } else { 
        _slave->writeSingleRegister(_writeAddress + _writeIndex, (uint16_t)value);
    }
    _writeIndex++;
}

int ModbusInternalClient::endTransmission() {
    if (!_slave) return 0;
    _slave->updatePhysicalIO();
    return 1; // Confirmar transmisión exitosa
}

int ModbusInternalClient::requestFrom(int id, int type, int address, int nb) {
    if (!_slave) { _lastError = EINVAL; return 0; }
    int maxCount = 0;
    switch (type) {
        case COILS:             maxCount = _slave->getCoilsCount(); break;
        case DISCRETE_INPUTS:   maxCount = _slave->getDiscreteInputsCount(); break;
        case HOLDING_REGISTERS: maxCount = _slave->getHoldingRegistersCount(); break;
        case INPUT_REGISTERS:   maxCount = _slave->getInputRegistersCount(); break;
        default: 
            _lastError = EINVAL;
            return 0; 
    }

    if (address < 0 || (address + nb) > maxCount || nb > 125) {
        ESP_LOGE(TAG, "Read request out of range or block too large. Count: %d", nb);
        _lastError = CODE_ILLEGAL_ADDRES; 
        return 0;   
    }

    _slave->updatePhysicalIO(); 
    _bufferLength = nb;
    _bufferIndex = 0;

    // Guardar los datos en el buffer de lectura simulado
    for (int i = 0; i < nb; i++) {
        switch (type) {
            case COILS:             _bufferRead[i] = _slave->readCoil(address + i); break; 
            case DISCRETE_INPUTS:   _bufferRead[i] = _slave->readDiscreteInput(address + i); break;
            case HOLDING_REGISTERS: _bufferRead[i] = _slave->readHoldingRegister(address + i); break;
            case INPUT_REGISTERS:   _bufferRead[i] = _slave->readInputRegister(address + i); break;
        }
    }
    return nb; // Devuelve la cantidad de registros leídos con éxito
}

int ModbusInternalClient::available() {
    return _bufferLength - _bufferIndex;
}

long ModbusInternalClient::read() {
    if (_bufferIndex < _bufferLength) {
        return _bufferRead[_bufferIndex++];
    }
    return -1;
}

const char* ModbusInternalClient::lastError() { 
    if (_lastError == 0) {
        return ""; 
    }
    if (_lastError == CODE_ILLEGAL_ADDRES) {
        return "Modbus Exception: Illegal Data Address (Internal Slave)";
    }
    if (_lastError == EINVAL) {
        return "Modbus Exception: Illegal Data Value (Internal Slave)";
    }
    return modbus_strerror(_lastError); 
}

int ModbusInternalClient::lastErrorCode() {
    return _lastError;
}

void ModbusInternalClient::end() {
    // No requiere hardware que cerrar
}

void ModbusInternalClient::setTimeout(unsigned long ms) {
    // Al ser memoria local, las respuestas son instantáneas. Ignoramos el timeout.
}