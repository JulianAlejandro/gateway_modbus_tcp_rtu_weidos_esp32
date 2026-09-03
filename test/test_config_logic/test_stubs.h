#ifndef TEST_STUBS_H
#define TEST_STUBS_H

// Minimal Arduino stubs for native compilation of SystemConfig.cpp
#ifndef ARDUINO_H
#define ARDUINO_H
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cstdlib>
typedef uint8_t byte;
#endif

// Stub SD.h
#ifndef _SD_H
#define _SD_H
namespace fs { class FS {}; }
class SDClass : public fs::FS {
public:
    bool begin(int ssPin = -1, const char * mountpoint = "/SD", uint8_t maxFiles = 5) { return false; }
    bool exists(const char* path) { return false; }
    void end() {}
};
static SDClass SD;
#endif

// Stub esp_err.h
#ifndef ESP_ERR_H
#define ESP_ERR_H
typedef int esp_err_t;
#define ESP_OK 0
#define ESP_FAIL -1
#endif

// Stub ESP logging
#ifndef ESP_LOG_H
#define ESP_LOG_H
#define ESP_LOGI(tag, fmt, ...) ((void)0)
#define ESP_LOGW(tag, fmt, ...) ((void)0)
#define ESP_LOGE(tag, fmt, ...) ((void)0)
#define ESP_LOGD(tag, fmt, ...) ((void)0)
#endif

// Stub E2PROM.h
#ifndef E2PROM_H
#define E2PROM_H
class EEPROMClass {
public:
    void begin() {}
    template<typename T>
    void get(int addr, T &val) { memset(&val, 0, sizeof(T)); }
    template<typename T>
    void put(int addr, const T &val) {}
};
static EEPROMClass E2PROM;
#endif

// RS485 pin constants
#ifndef RS485_TX
#define RS485_TX 17
#define RS485_DE 22
#define RS485_RE 23
#endif

// Serial config constants (Arduino HardwareSerial values)
#ifndef SERIAL_8N1
#define SERIAL_5N1 0x06
#define SERIAL_6N1 0x0E
#define SERIAL_7N1 0x16
#define SERIAL_8N1 0x1E
#define SERIAL_5N2 0x26
#define SERIAL_6N2 0x2E
#define SERIAL_7N2 0x36
#define SERIAL_8N2 0x3E
#define SERIAL_5E1 0x24
#define SERIAL_6E1 0x2C
#define SERIAL_7E1 0x34
#define SERIAL_8E1 0x3C
#define SERIAL_5E2 0x64
#define SERIAL_6E2 0x6C
#define SERIAL_7E2 0x74
#define SERIAL_8E2 0x7C
#define SERIAL_5O1 0x34
#define SERIAL_6O1 0x3C
#define SERIAL_7O1 0x44
#define SERIAL_8O1 0x4C
#define SERIAL_5O2 0x74
#define SERIAL_6O2 0x7C
#define SERIAL_7O2 0x84
#define SERIAL_8O2 0x8C
#endif

// CSV_Parser stub (only needed if SystemConfig.cpp includes it)
#ifndef CSV_PARSER_H
#define CSV_PARSER_H
class CSV_Parser {
public:
    CSV_Parser(const char* csv, const char* sep = ",", bool hasHeader = true, char quote = '"') {}
    int getSelectedRows() { return 0; }
    char** getColumnAsString(int col) { return nullptr; }
};
#endif

#endif // TEST_STUBS_H
