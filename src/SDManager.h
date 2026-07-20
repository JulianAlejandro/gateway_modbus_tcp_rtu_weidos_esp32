#ifndef SD_MANAGER_H
#define SD_MANAGER_H

#include <Arduino.h>
#include <SD.h>
#include <esp_err.h>

#define ESP_ERR_SD_BASE           0x40000
#define ESP_ERR_SD_NOT_INIT       (ESP_ERR_SD_BASE + 1)
#define ESP_ERR_SD_MOUNT          (ESP_ERR_SD_BASE + 2) // Fallo físico o de formato
#define ESP_ERR_SD_FILE_NOT_FOUND (ESP_ERR_SD_BASE + 3)
#define ESP_ERR_SD_WRITE_FAIL     (ESP_ERR_SD_BASE + 4)
#define ESP_ERR_SD_DIR_FAIL       (ESP_ERR_SD_BASE + 5)

/** * @brief Callback types for file processing.
 * LineCallback: Used for processing a file line by line.
 * StreamCallback: Used for direct access to the File stream.
 */
//typedef void (*LineCallback)(const char* line, void* context);
typedef void (*FileIterationCallback)(const char* fileName, bool isDir, void* context);
typedef void (*StreamCallback)(Stream& stream, void* context);

class SDManager {
private:
    //File _currentFile; 
    bool _initialized = false;
    static const char* TAG; 

public:
    /**
     * @brief Constructor for SDManager.
     */
    SDManager();
    ~SDManager(); // <-- Declaración del destructor

    /**
     * @brief Initializes the SD card using the default SPI bus.
     * @return true if initialization was successful, false otherwise.
     */
    esp_err_t begin();

    /**
     * @brief Checks if the SD card is initialized and ready for operations.
     * @return Current initialization state.
     */
    bool isReady();

    /**
     * @brief Creates a new empty file if it doesn't exist.
     * @param path Full path to the file.
     * @return true if file exists or was created successfully.
     */
    esp_err_t createFile(const char* path);

    /**
     * @brief Creates a directory on the SD card.
     * @param path Full path of the directory.
     * @return true if directory exists or was created successfully.
     */
    esp_err_t createDirectory(const char* path);

    /**
     * @brief Checks if a file or directory exists at the given path.
     * @param path Path to check.
     * @return true if found.
     */
    bool exists(const char* path);

    /**
     * @brief Deletes all content from a file (truncates to zero size).
     * @param path Path to the file.
     */
    void clearFile(const char* path);

    /**
     * @brief Appends a line of text to a file.
     * @param path Path to the file.
     * @param data String data to append.
     * @return true if successfully written.
     */
    //bool appendLine(const char* path, const char* data);

    /**
     * @brief Reads the entire content of a file and prints it to the Serial monitor.
     * @param path Path to the file.
     */
    void printFileToSerial(const char* path);

    /**
     * @brief Executes a callback function providing a Stream reference to the file.
     * Useful for parsing files without loading them entirely into RAM.
     * @param path Path to the file.
     * @param callback Function to execute.
     * @param context Pointer to user data for the callback.
     * @return true if file was opened successfully.
     */
    esp_err_t withFile(const char* path, StreamCallback callback, void* context);

    esp_err_t withFileWrite(const char* path, StreamCallback callback, void* context);

    esp_err_t deleteFile(const char* path);

    esp_err_t listDirectory(const char* dirPath, FileIterationCallback callback, void* context);

    esp_err_t getFileSize(const char* path, uint32_t* outSize);

    void end();
};

#endif