    #include "ModbusRTUClientManager.h"
    
    ModbusRtuClientManager::ModbusRtuClientManager(ModbusRTUClientClass* rtu) : _rtu(rtu) {}

    bool ModbusRtuClientManager::coilWrite(uint8_t slaveID, int address, uint8_t value){ 
        return _rtu->coilWrite(slaveID, address, value); 
    }
    bool ModbusRtuClientManager::holdingRegisterWrite(uint8_t slaveID, int address, uint16_t value){ 
        return _rtu->holdingRegisterWrite(slaveID, address, value); 
    }
    
    bool ModbusRtuClientManager::beginTransmission(uint8_t slaveID, int dataType, int address, int quantity){ 
        return _rtu->beginTransmission(slaveID, dataType, address, quantity); 
    }
    void ModbusRtuClientManager::write(uint16_t value){ 
        _rtu->write(value); 
    }
    bool ModbusRtuClientManager::endTransmission(){
         return _rtu->endTransmission(); 
        }

    bool ModbusRtuClientManager::requestFrom(uint8_t slaveID, int dataType, int address, int quantity){ 
        return _rtu->requestFrom(slaveID, dataType, address, quantity); 
    }
    int ModbusRtuClientManager::read(){ 
        return _rtu->read(); 
    }
    const char* ModbusRtuClientManager::lastError(){ 
        return _rtu->lastError(); 
    }