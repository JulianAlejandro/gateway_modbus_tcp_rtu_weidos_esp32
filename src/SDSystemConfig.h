#pragma once

#include "SDManager.h"

#define PARAM_FILE "/sysconf.csv"
#define MAX_TEXT_SIZE 32

/**
 * @struct SystemConfigRaw
 * @brief Estructura para almacenar en texto plano los parámetros leídos del CSV.
 */
struct SystemConfigRaw {
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

/**
 * @brief Lee el archivo CSV de configuración de la SD y llena la estructura SystemConfigRaw.
 * @param _sd Puntero al manejador de la tarjeta SD.
 * @return Estructura SystemConfigRaw con las cadenas extraídas.
 */
SystemConfigRaw SDgetSystemConfig(SDManager* _sd);