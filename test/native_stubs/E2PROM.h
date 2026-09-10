#ifndef E2PROM_H_STUB
#define E2PROM_H_STUB

#include <cstdint>
#include <cstring>

class EEPROMClass {
public:
    void begin() {}
    template<typename T>
    void get(int addr, T& val) { memset(&val, 0, sizeof(T)); }
    template<typename T>
    void put(int addr, const T& val) {}
};

extern EEPROMClass E2PROM;

#endif
