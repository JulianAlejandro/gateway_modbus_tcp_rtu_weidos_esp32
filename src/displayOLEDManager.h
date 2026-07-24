#ifndef DISPLAY_OLED_MANAGER_H
#define DISPLAY_OLED_MANAGER_H

#include <Arduino.h>
#include <U8g2lib.h>
#include <WPriv_OLED128X32emasesa.h>  // Librería nativa con emasesa_icon y showLogo2LinesLeft

const unsigned long INTERNAL_POLL_THRESHOLD = 30000; 
#define STALE_TIMEOUT 60000 
#define DISPLAY_INTERVAL 4000

// Re-declaramos la estructura para que los ficheros compartan el mismo tipo de datos
struct ModbusSlaveData {

//const values
    const char* name;
    const char* unit;
    const uint8_t decimals; 

    const uint8_t slaveID; 
    const uint16_t address;
    const uint16_t quantity;
    const uint8_t functionCode;

//variables    
    uint16_t rawBuffer[2];
    float convertedData;
    bool flagUpdate;
    unsigned long lastTimeReference;
   // bool disable; 
    bool isNew; 
    uint16_t errCounter; 
    
};

// Máquina de estados privada del archivo
enum DisplayState {
  OLED_IDLE,
  START_RENDER,
  RENDERING_PAGES
};


class displayOLEDManager{
private: 
    //DisplayState displayState = OLED_IDLE; // TODO inicializar en el constructor. 
    U8G2_SSD1306_128X32_UNIVISION_1_HW_I2C _u8g2;
    //U8G2 *_u8g2; 
    DisplayState _displayState;
    unsigned long _lastChange = 0;
    uint8_t _currentIdx = 0;
    char _cachedLine1[20];
    char _cachedLine2[40];

    ModbusSlaveData* _slaves = nullptr; 
    uint8_t _numSlaves = 0; 
    SemaphoreHandle_t _dataMutex = NULL; 

    void prepareTextData(uint8_t idx); 
public: 
    // Funciones expuestas
    displayOLEDManager();  // hay que implementar 
    void initOLED(ModbusSlaveData* slaves, uint8_t numSlaves, SemaphoreHandle_t dataMutex); 
    void updateOLED();
};

#endif