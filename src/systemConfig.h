#pragma once

#include "SDManager.h"
#include <E2PROM.h>

#define PARAM_FILE "/sysconf.csv"
#define MAX_TEXT_SIZE 32

#define CONFIG_MAGIC_KEY 0x4D425331 // "MBS1" - Clave para verificar si la EEPROM está inicializada

/**
 * @struct SystemConfigRaw
 * @brief Estructura para almacenar en texto plano los parámetros leídos del CSV.
 */
struct CSVSystemConfig {
    // Modbus TCP
    char mac[MAX_TEXT_SIZE];
    char ip[MAX_TEXT_SIZE];
    char gateway[MAX_TEXT_SIZE];
    char subnet[MAX_TEXT_SIZE];
    char dns[MAX_TEXT_SIZE];
    char port[MAX_TEXT_SIZE];

    // Modbus RTU
    char baudrate[MAX_TEXT_SIZE];
    char txPin[MAX_TEXT_SIZE];
    char dePin[MAX_TEXT_SIZE];
    char rePin[MAX_TEXT_SIZE];
    char rtuConfig[MAX_TEXT_SIZE];

    // Internal Modbus Slave
    char internalSlaveId[MAX_TEXT_SIZE];
};

struct EEPROMSystemConfig {
    uint32_t magic;           // Marcador para validar la configuración

    // --- MODBUS TCP ---
    uint8_t mac[6];
    uint8_t ip[4];
    uint8_t gateway[4];
    uint8_t subnet[4];
    uint8_t dns[4];
    uint16_t modbusPort;

    // --- MODBUS RTU ---
    uint32_t baudrate;
    int txPin;
    int dePin;
    int rePin;
    uint32_t rtuClientConfig;

    // --- INTERNAL SLAVE ---
    uint8_t internal_slave_id;
};

/**
 * @brief Lee el archivo CSV de configuración de la SD y llena la estructura SystemConfigRaw.
 * @param _sd Puntero al manejador de la tarjeta SD.
 * @return Estructura SystemConfigRaw con las cadenas extraídas.
 */
CSVSystemConfig SDgetSystemConfig(SDManager* _sd);


/**
 * @brief Lee el archivo CSV de configuración de la SD y llena la estructura SystemConfigRaw.
 * @param _sd Puntero al manejador de la tarjeta SD.
 * @return Estructura SystemConfigRaw con las cadenas extraídas.
 */
EEPROMSystemConfig rawToSystemConfig(const CSVSystemConfig& raw); 

void printConfig(const EEPROMSystemConfig& cfg);

