
#ifndef MODBUS_RTU_LOCK_H
#define MODBUS_RTU_LOCK_H

#include "IThreadLock.h"
#include "freertos/FreeRTOS.h" // <--- CRÍTICO: Define tipos base y configuraciones
#include "freertos/semphr.h"
#include "esp_log.h"

static const char* TAG_LOCK = "MODBUS_LOCK";


class ModbusRtuLock : public IThreadLock {
public:
    ModbusRtuLock() : _mutex(NULL) {} // Constructor vacío seguro
    
    void init(SemaphoreHandle_t mutex) { _mutex = mutex; } // Configurar tras crear el mutex
    
    bool lock(uint32_t timeoutMs = 0) override {
        if (_mutex == NULL) return false;
        TickType_t timeout = (timeoutMs == 0) ? portMAX_DELAY : pdMS_TO_TICKS(timeoutMs);
        if (xSemaphoreTake(_mutex, timeout) == pdTRUE) {
            return true;
        }
        ESP_LOGW(TAG_LOCK, "Lock timeout after %lu ms", (unsigned long)timeoutMs);
        return false;
    }

    bool tryLock(uint32_t timeoutMs = 0) override {
        if (_mutex == NULL) return false;
        TickType_t timeout = (timeoutMs == 0) ? portMAX_DELAY : pdMS_TO_TICKS(timeoutMs);
        return (xSemaphoreTake(_mutex, timeout) == pdTRUE);
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