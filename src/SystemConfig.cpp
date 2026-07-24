#include "SystemConfig.h"
#include <CSV_Parser.h>
#include <cstring>
#include <cstddef>
#include "esp_log.h"

static const char* TAG = "SYS_CFG";

// Tabla CRC16 (polynomial 0x8005, CRC-16/Modbus)
static const uint16_t crc16_table[256] = {
    0x0000, 0xC0C1, 0xC181, 0x0140, 0xC301, 0x03C0, 0x0280, 0xC241,
    0xC601, 0x06C0, 0x0780, 0xC741, 0x0500, 0xC5C1, 0xC481, 0x0440,
    0xCC01, 0x0CC0, 0x0D80, 0xCD41, 0x0F00, 0xCFC1, 0xCE81, 0x0E40,
    0x0A00, 0xCAC1, 0xCB81, 0x0B40, 0xC901, 0x09C0, 0x0880, 0xC841,
    0xD801, 0x18C0, 0x1980, 0xD941, 0x1B00, 0xDBC1, 0xDA81, 0x1A40,
    0x1E00, 0xDEC1, 0xDF81, 0x1F40, 0xDD01, 0x1DC0, 0x1C80, 0xDC41,
    0x1400, 0xD4C1, 0xD581, 0x1540, 0xD701, 0x17C0, 0x1680, 0xD641,
    0xD201, 0x12C0, 0x1380, 0xD341, 0x1100, 0xD1C1, 0xD081, 0x1040,
    0xF001, 0x30C0, 0x3180, 0xF141, 0x3300, 0xF3C1, 0xF281, 0x3240,
    0x3600, 0xF6C1, 0xF781, 0x3740, 0xF501, 0x35C0, 0x3480, 0xF441,
    0x3C00, 0xFCC1, 0xFD81, 0x3D40, 0xFF01, 0x3FC0, 0x3E80, 0xFE41,
    0xFA01, 0x3AC0, 0x3B80, 0xFB41, 0x3900, 0xF9C1, 0xF881, 0x3840,
    0x2800, 0xE8C1, 0xE981, 0x2940, 0xEB01, 0x2BC0, 0x2A80, 0xEA41,
    0xEE01, 0x2EC0, 0x2F80, 0xEF41, 0x2D00, 0xEDC1, 0xEC81, 0x2C40,
    0xE401, 0x24C0, 0x2580, 0xE541, 0x2700, 0xE7C1, 0xE681, 0x2640,
    0x2200, 0xE2C1, 0xE381, 0x2340, 0xE101, 0x21C0, 0x2080, 0xE041,
    0xA001, 0x60C0, 0x6180, 0xA141, 0x6300, 0xA3C1, 0xA281, 0x6240,
    0x6600, 0xA6C1, 0xA781, 0x6740, 0xA501, 0x65C0, 0x6480, 0xA441,
    0x6C00, 0xACC1, 0xAD81, 0x6D40, 0xAF01, 0x6FC0, 0x6E80, 0xAE41,
    0xAA01, 0x6AC0, 0x6B80, 0xAB41, 0x6900, 0xA9C1, 0xA881, 0x6840,
    0x7800, 0xB8C1, 0xB981, 0x7940, 0xBB01, 0x7BC0, 0x7A80, 0xBA41,
    0xBE01, 0x7EC0, 0x7F80, 0xBF41, 0x7D00, 0xBDC1, 0xBC81, 0x7C40,
    0xB401, 0x74C0, 0x7580, 0xB541, 0x7700, 0xB7C1, 0xB681, 0x7640,
    0x7200, 0xB2C1, 0xB381, 0x7340, 0xB101, 0x71C0, 0x7080, 0xB041,
    0x5000, 0x90C1, 0x9181, 0x5140, 0x9301, 0x53C0, 0x5280, 0x9241,
    0x9601, 0x56C0, 0x5780, 0x9741, 0x5500, 0x95C1, 0x9481, 0x5440,
    0x9C01, 0x5CC0, 0x5D80, 0x9D41, 0x5F00, 0x9FC1, 0x9E81, 0x5E40,
    0x5A00, 0x9AC1, 0x9B81, 0x5B40, 0x9901, 0x59C0, 0x5880, 0x9841,
    0x8801, 0x48C0, 0x4980, 0x8941, 0x4B00, 0x8BC1, 0x8A81, 0x4A40,
    0x4E00, 0x8EC1, 0x8F81, 0x4F40, 0x8D01, 0x4DC0, 0x4C80, 0x8C41,
    0x4400, 0x84C1, 0x8581, 0x4540, 0x8701, 0x47C0, 0x4680, 0x8641,
    0x8201, 0x42C0, 0x4380, 0x8341, 0x4100, 0x81C1, 0x8081, 0x4040
};

uint16_t calculateCRC16(const uint8_t* data, size_t length) {
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < length; i++) {
        crc = (crc >> 8) ^ crc16_table[(crc ^ data[i]) & 0xFF];
    }
    return crc;
}

bool verifyConfigCRC(const EEPROMSystemConfig& cfg) {
    if (cfg.crc == 0) return false;  // CRC sin inicializar
    size_t dataLen = offsetof(EEPROMSystemConfig, crc);
    uint16_t computed = calculateCRC16(reinterpret_cast<const uint8_t*>(&cfg), dataLen);
    return computed == cfg.crc;
}

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
    CSV_Parser cp("sssss", true, ';');

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
            } else if (strcmp(key, "Response Timeout (ms)") == 0) {
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

static bool isValidIP(const char* str) {
    if (!str || str[0] == '\0') return false;
    int a, b, c, d;
    if (sscanf(str, "%d.%d.%d.%d", &a, &b, &c, &d) != 4) return false;
    if (a < 0 || a > 255 || b < 0 || b > 255 || c < 0 || c > 255 || d < 0 || d > 255) return false;
    return true;
}

static bool isValidMAC(const char* str) {
    if (!str || str[0] == '\0') return false;
    int m[6];
    if (sscanf(str, "%x:%x:%x:%x:%x:%x", &m[0], &m[1], &m[2], &m[3], &m[4], &m[5]) != 6) return false;
    for (int i = 0; i < 6; i++) {
        if (m[i] < 0 || m[i] > 255) return false;
    }
    return true;
}

static bool isValidSerialConfig(const char* str) {
    if (!str || str[0] == '\0') return false;
    return (strcmp(str, "SERIAL_5N1") == 0 || strcmp(str, "SERIAL_6N1") == 0 ||
            strcmp(str, "SERIAL_7N1") == 0 || strcmp(str, "SERIAL_8N1") == 0 ||
            strcmp(str, "SERIAL_5N2") == 0 || strcmp(str, "SERIAL_6N2") == 0 ||
            strcmp(str, "SERIAL_7N2") == 0 || strcmp(str, "SERIAL_8N2") == 0 ||
            strcmp(str, "SERIAL_5E1") == 0 || strcmp(str, "SERIAL_6E1") == 0 ||
            strcmp(str, "SERIAL_7E1") == 0 || strcmp(str, "SERIAL_8E1") == 0 ||
            strcmp(str, "SERIAL_5E2") == 0 || strcmp(str, "SERIAL_6E2") == 0 ||
            strcmp(str, "SERIAL_7E2") == 0 || strcmp(str, "SERIAL_8E2") == 0 ||
            strcmp(str, "SERIAL_5O1") == 0 || strcmp(str, "SERIAL_6O1") == 0 ||
            strcmp(str, "SERIAL_7O1") == 0 || strcmp(str, "SERIAL_8O1") == 0 ||
            strcmp(str, "SERIAL_5O2") == 0 || strcmp(str, "SERIAL_6O2") == 0 ||
            strcmp(str, "SERIAL_7O2") == 0 || strcmp(str, "SERIAL_8O2") == 0);
}

bool validateCSVConfig(const CSVSystemConfig& raw) {
    bool valid = true;

    // MAC address
    if (!isValidMAC(raw.mac)) {
        ESP_LOGE(TAG, "Invalid MAC address: '%s'", raw.mac);
        valid = false;
    }

    // IP addresses
    if (!isValidIP(raw.ip)) {
        ESP_LOGE(TAG, "Invalid IP address: '%s'", raw.ip);
        valid = false;
    }
    if (!isValidIP(raw.gateway)) {
        ESP_LOGE(TAG, "Invalid gateway: '%s'", raw.gateway);
        valid = false;
    }
    if (!isValidIP(raw.subnet)) {
        ESP_LOGE(TAG, "Invalid subnet: '%s'", raw.subnet);
        valid = false;
    }
    if (!isValidIP(raw.dns)) {
        ESP_LOGE(TAG, "Invalid DNS: '%s'", raw.dns);
        valid = false;
    }

    // Port: 1-65535
    int port = atoi(raw.port);
    if (port < 1 || port > 65535) {
        ESP_LOGE(TAG, "Invalid port: '%s' (must be 1-65535)", raw.port);
        valid = false;
    }

    // Baudrate: 300-115200
    int baudrate = atoi(raw.baudrate);
    if (baudrate < 300 || baudrate > 115200) {
        ESP_LOGE(TAG, "Invalid baudrate: '%s' (must be 300-115200)", raw.baudrate);
        valid = false;
    }

    // Serial config: must be valid
    if (!isValidSerialConfig(raw.rtuConfig)) {
        ESP_LOGE(TAG, "Invalid RTU config: '%s'", raw.rtuConfig);
        valid = false;
    }

    // Inter-frame delay: 2-250
    int interFrameDelay = atoi(raw.interFrameDelay);
    if (interFrameDelay < 2 || interFrameDelay > 250) {
        ESP_LOGE(TAG, "Invalid inter-frame delay: '%s' (must be 2-250 ms)", raw.interFrameDelay);
        valid = false;
    }

    // Response timeout: 50-5000
    int responseTimeout = atoi(raw.responseTimeout);
    if (responseTimeout < 50 || responseTimeout > 5000) {
        ESP_LOGE(TAG, "Invalid response timeout: '%s' (must be 50-5000 ms)", raw.responseTimeout);
        valid = false;
    }

    // Attempts: 1-5
    int attempts = atoi(raw.attempts);
    if (attempts < 1 || attempts > 5) {
        ESP_LOGE(TAG, "Invalid attempts: '%s' (must be 1-5)", raw.attempts);
        valid = false;
    }

    if (valid) {
        ESP_LOGI(TAG, "CSV configuration validated successfully");
    }

    return valid;
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
    config.version = CONFIG_VERSION;

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

    // Calcular CRC sobre todo el struct excepto el campo crc
    size_t dataLen = offsetof(EEPROMSystemConfig, crc);
    config.crc = calculateCRC16(reinterpret_cast<const uint8_t*>(&config), dataLen);

    return config;
}

void printConfig(const EEPROMSystemConfig& cfg) {
    Serial.println("--- DATOS LEÍDOS DE LA EEPROM ---");
    Serial.printf("Magic Key: 0x%X\n", cfg.magic);
    Serial.printf("Version: %d\n", cfg.version);
    Serial.printf("CRC16: 0x%04X\n", cfg.crc);
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