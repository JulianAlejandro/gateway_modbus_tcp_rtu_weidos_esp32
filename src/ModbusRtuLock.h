
#ifndef MODBUS_RTU_LOCK_H
#define MODBUS_RTU_LOCK_H

#include "IThreadLock.h"
#include "freertos/FreeRTOS.h" // <--- CRÍTICO: Define tipos base y configuraciones
#include "freertos/semphr.h"


class ModbusRtuLock : public IThreadLock {
public:
    ModbusRtuLock() : _mutex(NULL) {} // Constructor vacío seguro
    
    void init(SemaphoreHandle_t mutex) { _mutex = mutex; } // Configurar tras crear el mutex
    
        void lock() override {
            if (_mutex != NULL) {
            xSemaphoreTake(_mutex, portMAX_DELAY);
        }
    }
    
    void unlock() override {
        if (_mutex != NULL) {
            xSemaphoreGive(_mutex);
        }
    }
private:
    SemaphoreHandle_t _mutex;
};

#endif