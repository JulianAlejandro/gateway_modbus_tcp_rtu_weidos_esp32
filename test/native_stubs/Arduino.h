#ifndef ARDUINO_H_STUB
#define ARDUINO_H_STUB

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cstddef>
#include <cstdarg>

typedef uint8_t byte;
typedef uint16_t word;
typedef bool boolean;

class String {
    char* _buf;
public:
    String() : _buf(nullptr) {}
    String(const char* s) : _buf(nullptr) { (void)s; }
    String(const String&) {}
    String& operator=(const String&) { return *this; }
    String& operator=(const char*) { return *this; }
    String& operator+=(const char*) { return *this; }
    String& operator+=(const String&) { return *this; }
    void trim() {}
    int length() { return 0; }
    const char* c_str() { return ""; }
    operator bool() { return false; }
};

class Stream {
public:
    virtual int available() { return 0; }
    virtual int read() { return -1; }
    virtual size_t write(uint8_t) { return 0; }
    virtual size_t write(const uint8_t* buf, size_t size) { (void)buf; return size; }
    String readStringUntil(char) { return String(); }
};

class HardwareSerial : public Stream {
public:
    void begin(unsigned long) {}
    void begin(unsigned long, uint8_t) {}
    void end() {}
    int available() override { return 0; }
    int read() override { return -1; }
    size_t write(uint8_t c) override { (void)c; return 0; }
    size_t write(const uint8_t* buf, size_t size) override { (void)buf; return size; }
    bool println(const char*) { return true; }
    bool println(int) { return true; }
    bool printf(const char*, ...) { return true; }
    operator bool() { return true; }
};

extern HardwareSerial Serial;

#define HIGH 0x1
#define LOW  0x0
#define INPUT 0x0
#define OUTPUT 0x1
#define INPUT_PULLUP 0x2

#define A0 0
#define A1 1
#define A2 2

#define F(x) (x)

#define delay(x) ((void)0)
#define millis() 0UL
#define micros() 0UL

#endif
