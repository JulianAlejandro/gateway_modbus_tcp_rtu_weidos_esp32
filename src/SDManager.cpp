#include "SDManager.h"
#include <SPI.h>


const char* SDManager::TAG = "SD_MGR";
/**
 * @brief Default constructor.
 */
SDManager::SDManager() {}

SDManager::~SDManager() {
    end(); 
}

/**
 * @brief Starts the SD card hardware communication.
 * Validates the SPI connection and FAT file system mounting.
 */
esp_err_t SDManager::begin() {
    if (_initialized) return ESP_OK;

    // Intentamos montar la tarjeta
    if (!SD.begin()) {
        ESP_LOGE(TAG, "Fallo crítico: No se pudo montar la tarjeta SD");
        return ESP_ERR_SD_MOUNT;
    }

    _initialized = true;
    ESP_LOGI(TAG, "Tarjeta SD montada correctamente");
    return ESP_OK;
}

/**
 * @brief Returns the initialization status.
 */
bool SDManager::isReady() {
    return _initialized;
}

/**
 * @brief Ensures a file exists. If it doesn't, it creates an empty one.
 */
esp_err_t SDManager::createFile(const char* path) {
    if (!_initialized) return ESP_ERR_SD_NOT_INIT;

    if (SD.exists(path)) return ESP_OK;

    File f = SD.open(path, FILE_WRITE);
    if (f) {
        f.close();
        return ESP_OK;
    }
    
    ESP_LOGE(TAG, "Error al crear archivo: %s", path);
    return ESP_ERR_SD_WRITE_FAIL;
}

/**
 * @brief Checks existence of a resource at the given path.
 */
bool SDManager::exists(const char* path) {
    return SD.exists(path);
}

/**
 * @brief Overwrites an existing file with an empty state.
 * Uses O_TRUNC to reset file size to 0.
 */
void SDManager::clearFile(const char* path) {
    if (!_initialized) return;
    // O_TRUNC borra el contenido
    File f = SD.open(path, O_WRITE | O_CREAT | O_TRUNC); 
    if (f) f.close(); 
}

/**
 * @brief Opens file in append mode and adds a new line of data.
 */
/*
bool SDManager::appendLine(const char* path, const char* data) {
    File f = SD.open(path, FILE_WRITE);
    if (!f) return false;
    
    f.println(data); // Appends data + line jump (\n)
    f.close();
    return true;
}
*/
/**
 * @brief Utility for debugging. Prints the full file content to Serial.
 */
void SDManager::printFileToSerial(const char* path) {
    if (!_initialized) return;
    File f = SD.open(path, FILE_READ);
    if (!f) return;
    
    while (f.available()) {
        Serial.write(f.read());
    }
    f.close();
}

/**
 * @brief Creates a directory structure.
 */
esp_err_t SDManager::createDirectory(const char* path) {
    if (!_initialized) return ESP_ERR_SD_NOT_INIT;

    if (SD.exists(path)) return ESP_OK;

    if (SD.mkdir(path)) {
        return ESP_OK;
    }

    ESP_LOGE(TAG, "Fallo al crear directorio: %s", path);
    return ESP_ERR_SD_DIR_FAIL;
}

/**
 * @brief Deletes a file from the SD card.
 * @param path Full path to the file.
 * @return true if the file was successfully deleted or didn't exist, false if deletion failed.
 */
esp_err_t SDManager::deleteFile(const char* path) {
    if (!_initialized) return ESP_ERR_SD_NOT_INIT;

    if (!SD.exists(path)) return ESP_OK;

    if (SD.remove(path)) {
        return ESP_OK;
    }

    ESP_LOGE(TAG, "Fallo al eliminar archivo: %s", path);
    return ESP_ERR_SD_WRITE_FAIL;
}

/**
 * @brief Higher-order function to handle file streaming via callbacks.
 * Ensures the file is safely closed after the callback execution.
 */
esp_err_t SDManager::withFile(const char* path, StreamCallback callback, void* context) {
    if (!_initialized) return ESP_ERR_SD_NOT_INIT;

    File file = SD.open(path, FILE_READ);
    if (!file) {
        ESP_LOGW(TAG, "Archivo no encontrado para lectura: %s", path);
        return ESP_ERR_SD_FILE_NOT_FOUND;
    }

    callback(file, context);
    file.close();
    return ESP_OK;
}


// Versión modificada de withFile para permitir escritura
esp_err_t SDManager::withFileWrite(const char* path, StreamCallback callback, void* context) {
    if (!_initialized) return ESP_ERR_SD_NOT_INIT;

    File file = SD.open(path, FILE_WRITE);
    if (!file) {
        ESP_LOGE(TAG, "No se pudo abrir para escritura: %s", path);
        return ESP_ERR_SD_WRITE_FAIL;
    }

    callback(file, context);
    file.flush();
    file.close();
    return ESP_OK;
}


/**
 * @brief Recorre los elementos de un directorio y ejecuta un callback por cada uno.
 * @param dirPath Ruta del directorio a listar (ej: "/LOG/2026")
 * @param callback Función que se ejecutará por cada archivo/carpeta encontrado.
 * @param context Puntero opcional para pasar datos externos al callback.
 */
esp_err_t SDManager::listDirectory(const char* dirPath, FileIterationCallback callback, void* context) {
    if (!_initialized) return ESP_ERR_SD_NOT_INIT;

    // Abrimos la ruta del directorio
    File root = SD.open(dirPath);
    if (!root) {
        ESP_LOGE(TAG, "Fallo al abrir el directorio: %s", dirPath);
        return ESP_ERR_SD_DIR_FAIL; // O un error equivalente tuyo
    }

    // Validamos que realmente sea un directorio y no un archivo suelto
    if (!root.isDirectory()) {
        ESP_LOGW(TAG, "La ruta provista no es un directorio: %s", dirPath);
        root.close();
        return ESP_ERR_SD_DIR_FAIL; 
    }

    // Iteramos sobre cada elemento interno
    File file = root.openNextFile();
    while (file) {
        // Ejecutamos el callback pasando el nombre del archivo y si es carpeta
        callback(file.name(), file.isDirectory(), context);
        
        file.close(); // Cerramos el archivo actual antes de pedir el siguiente
        file = root.openNextFile(); // Siguiente elemento
    }

    root.close();
    return ESP_OK;
}


/**
 * @brief Obtiene el tamaño de un archivo en bytes de forma segura.
 */
esp_err_t SDManager::getFileSize(const char* path, uint32_t* outSize) {
    // 1. Validación de inicialización de la tarjeta
    if (!_initialized) return ESP_ERR_SD_NOT_INIT;

    // 2. Validación de punteros de salida seguros
    if (path == nullptr || outSize == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }

    // 3. Comprobar si el archivo físico existe en la SD
    if (!SD.exists(path)) {
        *outSize = 0; // Inicialización de seguridad en caso de fallo
        ESP_LOGW(TAG, "No se puede obtener tamaño. El archivo no existe: %s", path);
        return ESP_ERR_SD_FILE_NOT_FOUND;
    }

    // 4. Abrir el archivo en modo Lectura
    File file = SD.open(path, FILE_READ);
    if (!file) {
        *outSize = 0;
        ESP_LOGE(TAG, "Fallo al abrir archivo para verificar tamaño: %s", path);
        return ESP_ERR_SD_WRITE_FAIL; // O un error genérico de acceso a la SD
    }

    // 5. Extraer el tamaño usando el método nativo del objeto File
    *outSize = file.size();

    // 6. Cerrar el archivo inmediatamente para liberar recursos de hardware
    file.close();

    return ESP_OK;
}

void SDManager::end() {
    if (!_initialized) return;

    SD.end(); // Libera los recursos del driver nativo y desmonta el volumen FAT
    _initialized = false;
    ESP_LOGI(TAG, "Tarjeta SD desmontada y recursos liberados correctamente.");
}