#include "SystemConfig.h"
#include <CSV_Parser.h>
#include <cstring>

CSVSystemConfig SDgetSystemConfig(SDManager* _sd) { 
    CSVSystemConfig res; 
    
    // Inicializar todas las cadenas con terminador nulo
    res.mac[0] = '\0';
    res.ip[0] = '\0';
    res.gateway[0] = '\0';
    res.subnet[0] = '\0';
    res.dns[0] = '\0';
    res.port[0] = '\0';
    res.baudrate[0] = '\0';
    res.txPin[0] = '\0';
    res.dePin[0] = '\0';
    res.rePin[0] = '\0';
    res.rtuConfig[0] = '\0';
    res.interFrameDelay[0] = '\0';
    res.responseTimeout[0] = '\0';
    res.attempts[0] = '\0';
    res.internalSlaveId[0] = '\0';

    if (!_sd->isReady()) {
        return res;
    }

    // "ssss" -> 4 columnas tipo string (Name; Value; Editable; coment)
    CSV_Parser cp("ssss", true, ';');

    // Leemos el archivo pasándole las líneas al parser
    _sd->withFile(PARAM_FILE, [](Stream& file, void* arg) {
        CSV_Parser* parser = (CSV_Parser*)arg;
        while (file.available()) {
            String line = file.readStringUntil('\n');
            line.trim(); 
            if (line.length() > 0) {
                line += "\n";
                *parser << line.c_str();
            }
        }
    }, &cp);

    char **names  = (char**)cp[0]; // Columna 0: Name
    char **values = (char**)cp[1]; // Columna 1: Value

    int rowCount = cp.getRowsCount();

    if (names != nullptr && values != nullptr) {
        for (int i = 0; i < rowCount; i++) {
            if (!names[i] || !values[i]) continue;

            String keyStr = String(names[i]);
            keyStr.trim();
            const char* key = keyStr.c_str();

            // Comparar y asignar a la variable correspondiente
            if (strcmp(key, "mac address") == 0) {
                strncpy(res.mac, values[i], MAX_TEXT_SIZE - 1);
            } else if (strcmp(key, "IP address") == 0) {
                strncpy(res.ip, values[i], MAX_TEXT_SIZE - 1);
            } else if (strcmp(key, "gateway") == 0) {
                strncpy(res.gateway, values[i], MAX_TEXT_SIZE - 1);
            } else if (strcmp(key, "subnet") == 0) {
                strncpy(res.subnet, values[i], MAX_TEXT_SIZE - 1);
            } else if (strcmp(key, "dns") == 0) {
                strncpy(res.dns, values[i], MAX_TEXT_SIZE - 1);
            } else if (strcmp(key, "port") == 0) {
                strncpy(res.port, values[i], MAX_TEXT_SIZE - 1);
            } else if (strcmp(key, "baudrate") == 0) {
                strncpy(res.baudrate, values[i], MAX_TEXT_SIZE - 1);
            } else if (strcmp(key, "txPin") == 0) {
                strncpy(res.txPin, values[i], MAX_TEXT_SIZE - 1);
            } else if (strcmp(key, "dePin") == 0) {
                strncpy(res.dePin, values[i], MAX_TEXT_SIZE - 1);
            } else if (strcmp(key, "rePin") == 0) {
                strncpy(res.rePin, values[i], MAX_TEXT_SIZE - 1);
            } else if (strcmp(key, "RTU Config") == 0) {
                strncpy(res.rtuConfig, values[i], MAX_TEXT_SIZE - 1);
            } else if (strcmp(key, "Inter-frame delay (ms)") == 0) {
                strncpy(res.interFrameDelay, values[i], MAX_TEXT_SIZE - 1);
            } else if (strcmp(key, "Response Timeout") == 0) {
                strncpy(res.responseTimeout, values[i], MAX_TEXT_SIZE - 1);
            } else if (strcmp(key, "Attemp") == 0) {
                strncpy(res.attempts, values[i], MAX_TEXT_SIZE - 1);
            } else if (strcmp(key, "id") == 0) {
                strncpy(res.internalSlaveId, values[i], MAX_TEXT_SIZE - 1);
            }
        }
    }

    return res; 
}

bool parseIP(const char* str, uint8_t out[4]) {
    int a, b, c, d;
    if (sscanf(str, "%d.%d.%d.%d", &a, &b, &c, &d) == 4) {
        out[0] = (uint8_t)a;
        out[1] = (uint8_t)b;
        out[2] = (uint8_t)c;
        out[3] = (uint8_t)d;
        return true;
    }
    return false;
}

bool parseMAC(const char* str, uint8_t out[6]) {
    int m[6];
    if (sscanf(str, "%x:%x:%x:%x:%x:%x", &m[0], &m[1], &m[2], &m[3], &m[4], &m[5]) == 6) {
        for(int i = 0; i < 6; i++) out[i] = (uint8_t)m[i];
        return true;
    }
    return false;
}

uint32_t parseSerialConfig(const char* str) {
    if (strcmp(str, "SERIAL_5N1") == 0) return SERIAL_5N1;
    if (strcmp(str, "SERIAL_6N1") == 0) return SERIAL_6N1;
    if (strcmp(str, "SERIAL_7N1") == 0) return SERIAL_7N1;
    if (strcmp(str, "SERIAL_8N1") == 0) return SERIAL_8N1;
    
    if (strcmp(str, "SERIAL_5N2") == 0) return SERIAL_5N2;
    if (strcmp(str, "SERIAL_6N2") == 0) return SERIAL_6N2;
    if (strcmp(str, "SERIAL_7N2") == 0) return SERIAL_7N2;
    if (strcmp(str, "SERIAL_8N2") == 0) return SERIAL_8N2;

    if (strcmp(str, "SERIAL_5E1") == 0) return SERIAL_5E1;
    if (strcmp(str, "SERIAL_6E1") == 0) return SERIAL_6E1;
    if (strcmp(str, "SERIAL_7E1") == 0) return SERIAL_7E1;
    if (strcmp(str, "SERIAL_8E1") == 0) return SERIAL_8E1;
    
    if (strcmp(str, "SERIAL_5E2") == 0) return SERIAL_5E2;
    if (strcmp(str, "SERIAL_6E2") == 0) return SERIAL_6E2;
    if (strcmp(str, "SERIAL_7E2") == 0) return SERIAL_7E2;
    if (strcmp(str, "SERIAL_8E2") == 0) return SERIAL_8E2;

    if (strcmp(str, "SERIAL_5O1") == 0) return SERIAL_5O1;
    if (strcmp(str, "SERIAL_6O1") == 0) return SERIAL_6O1;
    if (strcmp(str, "SERIAL_7O1") == 0) return SERIAL_7O1;
    if (strcmp(str, "SERIAL_8O1") == 0) return SERIAL_8O1;
    
    if (strcmp(str, "SERIAL_5O2") == 0) return SERIAL_5O2;
    if (strcmp(str, "SERIAL_6O2") == 0) return SERIAL_6O2;
    if (strcmp(str, "SERIAL_7O2") == 0) return SERIAL_7O2;
    if (strcmp(str, "SERIAL_8O2") == 0) return SERIAL_8O2;

    return SERIAL_8N1; 
}

int parsePin(const char* str, int defaultPin) {
    if (strcmp(str, "RS485_TX") == 0) return RS485_TX;
    if (strcmp(str, "RS485_DE") == 0) return RS485_DE;
    if (strcmp(str, "RS485_RE") == 0) return RS485_RE;
    
    int pin = atoi(str);
    return (pin != 0 || strcmp(str, "0") == 0) ? pin : defaultPin;
}

// CONVERSOR PRINCIPAL: CSVSystemConfig -> EEPROMSystemConfig
EEPROMSystemConfig rawToSystemConfig(const CSVSystemConfig& raw) {
    EEPROMSystemConfig config;
    config.magic = CONFIG_MAGIC_KEY;

    // 1. Modbus TCP
    parseMAC(raw.mac, config.mac);
    parseIP(raw.ip, config.ip);
    parseIP(raw.gateway, config.gateway);
    parseIP(raw.subnet, config.subnet);
    parseIP(raw.dns, config.dns);
    config.modbusPort = (uint16_t)atoi(raw.port);

    // 2. Modbus RTU
    config.baudrate = (uint32_t)strtoul(raw.baudrate, NULL, 10);

    
    config.txPin = parsePin(raw.txPin, RS485_TX);
    config.dePin = parsePin(raw.dePin, RS485_DE);
    config.rePin = parsePin(raw.rePin, RS485_RE);

    config.rtuClientConfig = parseSerialConfig(raw.rtuConfig);
    
    // Parseo de los nuevos parámetros RTU
    config.interFrameDelay = (uint32_t)strtoul(raw.interFrameDelay, NULL, 10);
    config.responseTimeout = (uint32_t)strtoul(raw.responseTimeout, NULL, 10);
    config.attempts = (uint8_t)atoi(raw.attempts);

    // 3. Slave Interno
    config.internal_slave_id = (uint8_t)atoi(raw.internalSlaveId);

    return config;
}

void printConfig(const EEPROMSystemConfig& cfg) {
    Serial.println("--- DATOS LEÍDOS DE LA EEPROM ---");
    Serial.printf("Magic Key: 0x%X\n", cfg.magic);
    Serial.printf("MAC: %02X:%02X:%02X:%02X:%02X:%02X\n", 
                  cfg.mac[0], cfg.mac[1], cfg.mac[2], cfg.mac[3], cfg.mac[4], cfg.mac[5]);
    Serial.printf("IP: %d.%d.%d.%d\n", cfg.ip[0], cfg.ip[1], cfg.ip[2], cfg.ip[3]);
    Serial.printf("Gateway: %d.%d.%d.%d\n", cfg.gateway[0], cfg.gateway[1], cfg.gateway[2], cfg.gateway[3]);
    Serial.printf("Subnet: %d.%d.%d.%d\n", cfg.subnet[0], cfg.subnet[1], cfg.subnet[2], cfg.subnet[3]);
    Serial.printf("DNS: %d.%d.%d.%d\n", cfg.dns[0], cfg.dns[1], cfg.dns[2], cfg.dns[3]);
    Serial.printf("Puerto Modbus TCP: %d\n", cfg.modbusPort);
    Serial.printf("Baudrate RTU: %d\n", cfg.baudrate);
    Serial.printf("Pines RTU (TX/DE/RE): %d / %d / %d\n", cfg.txPin, cfg.dePin, cfg.rePin);
    Serial.printf("Inter-frame delay: %d ms\n", cfg.interFrameDelay);
    Serial.printf("Response Timeout: %d ms\n", cfg.responseTimeout);
    Serial.printf("Attempts: %d\n", cfg.attempts);
    Serial.printf("Internal Slave ID: %d\n", cfg.internal_slave_id);
    Serial.println("---------------------------------");

}