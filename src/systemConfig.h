#include <Arduino.h>
#include <E2PROM.h>

#define CONFIG_MAGIC_KEY 0x4D425331 // "MBS1" - Clave para verificar si la EEPROM está inicializada

// Estructura limpia y alineada para almacenar en EEPROM
struct SystemConfig {
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
    uint16_t rtuClientConfig;

    // --- INTERNAL SLAVE ---
    uint8_t internal_slave_id;
};