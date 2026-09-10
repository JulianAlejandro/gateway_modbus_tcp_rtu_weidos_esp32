#ifndef CSV_PARSER_H_STUB
#define CSV_PARSER_H_STUB

#include <cstdint>
#include <cstdio>
#include <cstring>

class CSV_Parser {
public:
    CSV_Parser(const char* csv, const char* sep = ",", bool hasHeader = true, char quote = '"') {}
    CSV_Parser(const char* csv, bool hasHeader, char sep) {}
    int getSelectedRows() { return 0; }
    int getRowsCount() { return 0; }
    char** getColumnAsString(int col) { return nullptr; }
    int32_t* getColumnAsLong(int col) { return nullptr; }
    uint32_t* getColumnAsUnsignedLong(int col) { return nullptr; }
    void* operator[](int col) { return nullptr; }
    CSV_Parser& operator<<(const char*) { return *this; }
};

#endif
