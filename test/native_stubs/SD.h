#ifndef SD_H_STUB
#define SD_H_STUB

#include <cstdint>
#include <cstdio>

namespace fs {
    class FS {};
}

#define FILE_WRITE "w"
#define FILE_READ "r"
#define FILE_APPEND "a"
#define O_WRITE 0
#define O_CREAT 0
#define O_TRUNC 0

class File : public fs::FS, public Stream {
public:
    operator bool() { return false; }
    size_t write(uint8_t) { return 0; }
    size_t write(const uint8_t*, size_t) { return 0; }
    int available() { return 0; }
    int read() { return -1; }
    size_t read(uint8_t*, size_t) { return 0; }
    void close() {}
    size_t size() { return 0; }
    const char* name() { return ""; }
    bool isDirectory() { return false; }
    File openNextFile() { return File(); }
    bool printf(const char*, ...) { return false; }
    void flush() {}
    String readStringUntil(char) { return String(); }
};

class SDClass : public fs::FS {
public:
    bool begin(int ssPin = -1, const char* mountpoint = "/SD", uint8_t maxFiles = 5) { return false; }
    bool exists(const char* path) { return false; }
    File open(const char* path, const char* mode = "r") { return File(); }
    File open(const char* path, int mode) { return File(); }
    bool mkdir(const char* path) { return false; }
    bool remove(const char* path) { return false; }
    bool rmdir(const char* path) { return false; }
    void end() {}
};

extern SDClass SD;

#endif
