#ifndef FUNCTION_INTERNAL_CLIENT_H
#define FUNCTION_INTERNAL_CLIENT_H

#include <Arduino.h>
#include <U8g2lib.h>
#include <WPriv_OLED128X32emasesa.h>  // Librería nativa con emasesa_icon y showLogo2LinesLeft

#define STALE_TIMEOUT 60000 
#define DISPLAY_INTERVAL 4000

// Re-declaramos la estructura para que los ficheros compartan el mismo tipo de datos
struct ModbusSlaveData {
    const char* name;
    const char* unit;
    uint16_t address;     
    uint8_t slaveID;      
    uint16_t rawBuffer[2];
    volatile bool flagUpdate;
    float convertedData;
    unsigned long lastUpload;
    bool disable; 
    bool isNew; 
};

// Declaración externa del array y mutex que viven en el main
extern ModbusSlaveData slaves[];
extern const uint8_t NUM_SLAVES;
extern SemaphoreHandle_t xModbusDataMutex;

// Puntero global de la pantalla
extern U8G2 *u8g2;

// Funciones expuestas
void initOLED(); 
void updateOLED(); 

#endif