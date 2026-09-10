#include "Arduino.h"
#include "SD.h"
#include "SPI.h"
#include "E2PROM.h"
#include "SDManager.h"
#include "esp_log.h"

HardwareSerial Serial;
SDClass SD;
SPIClass SPI;
EEPROMClass E2PROM;

const char* SDManager::TAG = "SD_MGR";

SDManager::SDManager() {}
SDManager::~SDManager() { end(); }

esp_err_t SDManager::begin() {
    if (_initialized) return ESP_OK;
    if (!SD.begin()) return ESP_ERR_SD_MOUNT;
    _initialized = true;
    return ESP_OK;
}

bool SDManager::isReady() const { return _initialized; }

esp_err_t SDManager::createFile(const char* path) {
    if (!_initialized) return ESP_ERR_SD_NOT_INIT;
    if (SD.exists(path)) return ESP_OK;
    File f = SD.open(path, FILE_WRITE);
    if (f) { f.close(); return ESP_OK; }
    return ESP_ERR_SD_WRITE_FAIL;
}

bool SDManager::exists(const char* path) const { return SD.exists(path); }

void SDManager::clearFile(const char* path) {
    if (!_initialized) return;
    File f = SD.open(path, FILE_WRITE);
    if (f) f.close();
}

void SDManager::printFileToSerial(const char* path) {
    if (!_initialized) return;
    File f = SD.open(path, FILE_READ);
    if (!f) return;
    while (f.available()) Serial.write(f.read());
    f.close();
}

esp_err_t SDManager::createDirectory(const char* path) {
    if (!_initialized) return ESP_ERR_SD_NOT_INIT;
    if (SD.exists(path)) return ESP_OK;
    if (SD.mkdir(path)) return ESP_OK;
    return ESP_ERR_SD_DIR_FAIL;
}

esp_err_t SDManager::deleteFile(const char* path) {
    if (!_initialized) return ESP_ERR_SD_NOT_INIT;
    if (!SD.exists(path)) return ESP_OK;
    if (SD.remove(path)) return ESP_OK;
    return ESP_ERR_SD_WRITE_FAIL;
}

esp_err_t SDManager::withFile(const char* path, StreamCallback callback, void* context) {
    if (!_initialized) return ESP_ERR_SD_NOT_INIT;
    File file = SD.open(path, FILE_READ);
    if (!file) return ESP_ERR_SD_FILE_NOT_FOUND;
    callback(file, context);
    file.close();
    return ESP_OK;
}

esp_err_t SDManager::withFileWrite(const char* path, StreamCallback callback, void* context) {
    if (!_initialized) return ESP_ERR_SD_NOT_INIT;
    File file = SD.open(path, FILE_WRITE);
    if (!file) return ESP_ERR_SD_WRITE_FAIL;
    callback(file, context);
    file.flush();
    file.close();
    return ESP_OK;
}

esp_err_t SDManager::listDirectory(const char* dirPath, FileIterationCallback callback, void* context) {
    if (!_initialized) return ESP_ERR_SD_NOT_INIT;
    File root = SD.open(dirPath);
    if (!root) return ESP_ERR_SD_DIR_FAIL;
    if (!root.isDirectory()) { root.close(); return ESP_ERR_SD_DIR_FAIL; }
    File file = root.openNextFile();
    while (file) {
        callback(file.name(), file.isDirectory(), context);
        file.close();
        file = root.openNextFile();
    }
    root.close();
    return ESP_OK;
}

esp_err_t SDManager::getFileSize(const char* path, uint32_t* outSize) {
    if (!_initialized) return ESP_ERR_SD_NOT_INIT;
    if (!path || !outSize) return ESP_ERR_INVALID_ARG;
    if (!SD.exists(path)) { *outSize = 0; return ESP_ERR_SD_FILE_NOT_FOUND; }
    File file = SD.open(path, FILE_READ);
    if (!file) { *outSize = 0; return ESP_ERR_SD_WRITE_FAIL; }
    *outSize = file.size();
    file.close();
    return ESP_OK;
}

void SDManager::end() {
    if (!_initialized) return;
    SD.end();
    _initialized = false;
}
