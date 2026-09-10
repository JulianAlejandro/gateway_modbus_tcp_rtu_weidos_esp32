#ifndef SPI_H_STUB
#define SPI_H_STUB

#include "Arduino.h"

class SPISettings {
public:
    SPISettings(uint32_t, uint8_t, uint8_t) {}
};

class SPIClass {
public:
    void begin() {}
    void end() {}
    void beginTransaction(SPISettings) {}
    void endTransaction() {}
};

extern SPIClass SPI;

#endif
