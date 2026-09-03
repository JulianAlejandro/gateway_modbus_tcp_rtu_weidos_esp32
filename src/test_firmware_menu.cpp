#include <Arduino.h>
#include "systemConfig.h"
#include "SDManager.h"

// ============================================================
// Minimal embedded test runner (no Unity dependency)
// ============================================================

static int _test_count = 0;
static int _test_pass = 0;
static int _test_fail = 0;

#define TEST_ASSERT_TRUE(cond) do { \
    if (!(cond)) { \
        Serial.printf("  FAIL: %s (line %d)\n", #cond, __LINE__); \
        _test_fail++; \
    } else { _test_pass++; } \
    _test_count++; \
} while(0)

#define TEST_ASSERT_FALSE(cond) do { \
    if ((cond)) { \
        Serial.printf("  FAIL: !%s (line %d)\n", #cond, __LINE__); \
        _test_fail++; \
    } else { _test_pass++; } \
    _test_count++; \
} while(0)

#define TEST_ASSERT_EQUAL_UINT16(expected, actual) do { \
    if ((expected) != (actual)) { \
        Serial.printf("  FAIL: expected 0x%04X, got 0x%04X (line %d)\n", \
                      (uint16_t)(expected), (uint16_t)(actual), __LINE__); \
        _test_fail++; \
    } else { _test_pass++; } \
    _test_count++; \
} while(0)

#define TEST_ASSERT_EQUAL_UINT32(expected, actual) do { \
    if ((expected) != (actual)) { \
        Serial.printf("  FAIL: expected 0x%08X, got 0x%08X (line %d)\n", \
                      (uint32_t)(expected), (uint32_t)(actual), __LINE__); \
        _test_fail++; \
    } else { _test_pass++; } \
    _test_count++; \
} while(0)

#define TEST_ASSERT_EQUAL_UINT8(expected, actual) do { \
    if ((expected) != (actual)) { \
        Serial.printf("  FAIL: expected %d, got %d (line %d)\n", \
                      (uint8_t)(expected), (uint8_t)(actual), __LINE__); \
        _test_fail++; \
    } else { _test_pass++; } \
    _test_count++; \
} while(0)

#define TEST_ASSERT_NOT_EQUAL(expected, actual) do { \
    if ((expected) == (actual)) { \
        Serial.printf("  FAIL: expected != 0x%04X (line %d)\n", \
                      (uint16_t)(expected), __LINE__); \
        _test_fail++; \
    } else { _test_pass++; } \
    _test_count++; \
} while(0)

#define TEST_ASSERT_EQUAL_STRING(expected, actual) do { \
    if (strcmp((expected), (actual)) != 0) { \
        Serial.printf("  FAIL: expected \"%s\", got \"%s\" (line %d)\n", \
                      (expected), (actual), __LINE__); \
        _test_fail++; \
    } else { _test_pass++; } \
    _test_count++; \
} while(0)

#define TEST_ASSERT_TRUE_MESSAGE(cond, msg) do { \
    if (!(cond)) { \
        Serial.printf("  FAIL [%s]: %s (line %d)\n", (msg), #cond, __LINE__); \
        _test_fail++; \
    } else { _test_pass++; } \
    _test_count++; \
} while(0)

#define RUN_TEST(fn) do { \
    Serial.printf("  RUN  %s...", #fn); \
    fn(); \
    Serial.println(" ok"); \
} while(0)

#define TEST_REPORT() do { \
    Serial.println(); \
    Serial.println("========================================"); \
    Serial.printf("  TOTAL: %d | PASS: %d | FAIL: %d\n", \
                  _test_count, _test_pass, _test_fail); \
    Serial.println("========================================"); \
    if (_test_fail == 0) Serial.println("  ALL TESTS PASSED"); \
    else                 Serial.println("  SOME TESTS FAILED"); \
    Serial.println("========================================"); \
} while(0)

// ============================================================
// Unit tests
// ============================================================

static CSVSystemConfig createValidCSVConfig(void) {
    CSVSystemConfig cfg;
    memset(&cfg, 0, sizeof(cfg));
    strcpy(cfg.mac, "DE:AD:BE:EF:FE:ED");
    strcpy(cfg.ip, "192.168.1.150");
    strcpy(cfg.gateway, "192.168.1.1");
    strcpy(cfg.subnet, "255.255.255.0");
    strcpy(cfg.dns, "192.168.1.1");
    strcpy(cfg.port, "502");
    strcpy(cfg.baudrate, "9600");
    strcpy(cfg.txPin, "17");
    strcpy(cfg.dePin, "22");
    strcpy(cfg.rePin, "23");
    strcpy(cfg.rtuConfig, "SERIAL_8N2");
    strcpy(cfg.interFrameDelay, "250");
    strcpy(cfg.responseTimeout, "5000");
    strcpy(cfg.attempts, "3");
    strcpy(cfg.internalSlaveId, "10");
    return cfg;
}

// --- calculateCRC16 ---
void test_calculateCRC16_known_data(void) {
    const uint8_t data[] = "123456789";
    uint16_t crc = calculateCRC16(data, 9);
    TEST_ASSERT_EQUAL_UINT16(0x4B37, crc);
}

void test_calculateCRC16_empty_data(void) {
    uint16_t crc = calculateCRC16(nullptr, 0);
    TEST_ASSERT_EQUAL_UINT16(0xFFFF, crc);
}

void test_calculateCRC16_single_byte(void) {
    const uint8_t data[] = {0x00};
    uint16_t crc = calculateCRC16(data, 1);
    TEST_ASSERT_NOT_EQUAL(0xFFFF, crc);
}

// --- verifyConfigCRC ---
void test_verifyConfigCRC_valid(void) {
    EEPROMSystemConfig cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.magic = CONFIG_MAGIC_KEY;
    cfg.version = CONFIG_VERSION;
    cfg.modbusPort = 502;
    cfg.baudrate = 9600;
    size_t dataLen = offsetof(EEPROMSystemConfig, crc);
    cfg.crc = calculateCRC16(reinterpret_cast<const uint8_t*>(&cfg), dataLen);
    TEST_ASSERT_TRUE(verifyConfigCRC(cfg));
}

void test_verifyConfigCRC_corrupted(void) {
    EEPROMSystemConfig cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.magic = CONFIG_MAGIC_KEY;
    cfg.version = CONFIG_VERSION;
    size_t dataLen = offsetof(EEPROMSystemConfig, crc);
    cfg.crc = calculateCRC16(reinterpret_cast<const uint8_t*>(&cfg), dataLen);
    cfg.crc ^= 0xFFFF;
    TEST_ASSERT_FALSE(verifyConfigCRC(cfg));
}

void test_verifyConfigCRC_zero(void) {
    EEPROMSystemConfig cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.crc = 0;
    TEST_ASSERT_FALSE(verifyConfigCRC(cfg));
}

// --- validateCSVConfig ---
void test_validateCSVConfig_all_valid(void) {
    CSVSystemConfig cfg = createValidCSVConfig();
    TEST_ASSERT_TRUE(validateCSVConfig(cfg));
}

void test_validateCSVConfig_invalid_mac(void) {
    CSVSystemConfig cfg = createValidCSVConfig();
    strcpy(cfg.mac, "INVALID_MAC");
    TEST_ASSERT_FALSE(validateCSVConfig(cfg));
}

void test_validateCSVConfig_invalid_ip(void) {
    CSVSystemConfig cfg = createValidCSVConfig();
    strcpy(cfg.ip, "999.999.999.999");
    TEST_ASSERT_FALSE(validateCSVConfig(cfg));
}

void test_validateCSVConfig_invalid_gateway(void) {
    CSVSystemConfig cfg = createValidCSVConfig();
    strcpy(cfg.gateway, "not_an_ip");
    TEST_ASSERT_FALSE(validateCSVConfig(cfg));
}

void test_validateCSVConfig_invalid_port_zero(void) {
    CSVSystemConfig cfg = createValidCSVConfig();
    strcpy(cfg.port, "0");
    TEST_ASSERT_FALSE(validateCSVConfig(cfg));
}

void test_validateCSVConfig_invalid_port_too_high(void) {
    CSVSystemConfig cfg = createValidCSVConfig();
    strcpy(cfg.port, "70000");
    TEST_ASSERT_FALSE(validateCSVConfig(cfg));
}

void test_validateCSVConfig_invalid_baudrate_low(void) {
    CSVSystemConfig cfg = createValidCSVConfig();
    strcpy(cfg.baudrate, "100");
    TEST_ASSERT_FALSE(validateCSVConfig(cfg));
}

void test_validateCSVConfig_invalid_baudrate_high(void) {
    CSVSystemConfig cfg = createValidCSVConfig();
    strcpy(cfg.baudrate, "200000");
    TEST_ASSERT_FALSE(validateCSVConfig(cfg));
}

void test_validateCSVConfig_invalid_rtu_config(void) {
    CSVSystemConfig cfg = createValidCSVConfig();
    strcpy(cfg.rtuConfig, "INVALID_CONFIG");
    TEST_ASSERT_FALSE(validateCSVConfig(cfg));
}

void test_validateCSVConfig_invalid_interframe_delay(void) {
    CSVSystemConfig cfg = createValidCSVConfig();
    strcpy(cfg.interFrameDelay, "0");
    TEST_ASSERT_FALSE(validateCSVConfig(cfg));
}

void test_validateCSVConfig_invalid_interframe_delay_high(void) {
    CSVSystemConfig cfg = createValidCSVConfig();
    strcpy(cfg.interFrameDelay, "500");
    TEST_ASSERT_FALSE(validateCSVConfig(cfg));
}

void test_validateCSVConfig_invalid_response_timeout(void) {
    CSVSystemConfig cfg = createValidCSVConfig();
    strcpy(cfg.responseTimeout, "10");
    TEST_ASSERT_FALSE(validateCSVConfig(cfg));
}

void test_validateCSVConfig_invalid_attempts_zero(void) {
    CSVSystemConfig cfg = createValidCSVConfig();
    strcpy(cfg.attempts, "0");
    TEST_ASSERT_FALSE(validateCSVConfig(cfg));
}

void test_validateCSVConfig_invalid_attempts_high(void) {
    CSVSystemConfig cfg = createValidCSVConfig();
    strcpy(cfg.attempts, "10");
    TEST_ASSERT_FALSE(validateCSVConfig(cfg));
}

void test_validateCSVConfig_missing_field(void) {
    CSVSystemConfig cfg = createValidCSVConfig();
    cfg.ip[0] = '\0';
    TEST_ASSERT_FALSE(validateCSVConfig(cfg));
}

// --- rawToSystemConfig ---
void test_rawToSystemConfig_crc_valid(void) {
    CSVSystemConfig csv = createValidCSVConfig();
    EEPROMSystemConfig eeprom = rawToSystemConfig(csv);
    TEST_ASSERT_TRUE(verifyConfigCRC(eeprom));
}

void test_rawToSystemConfig_magic_key(void) {
    CSVSystemConfig csv = createValidCSVConfig();
    EEPROMSystemConfig eeprom = rawToSystemConfig(csv);
    TEST_ASSERT_EQUAL_UINT32(CONFIG_MAGIC_KEY, eeprom.magic);
}

void test_rawToSystemConfig_version(void) {
    CSVSystemConfig csv = createValidCSVConfig();
    EEPROMSystemConfig eeprom = rawToSystemConfig(csv);
    TEST_ASSERT_EQUAL_UINT8(CONFIG_VERSION, eeprom.version);
}

void test_rawToSystemConfig_port(void) {
    CSVSystemConfig csv = createValidCSVConfig();
    EEPROMSystemConfig eeprom = rawToSystemConfig(csv);
    TEST_ASSERT_EQUAL_UINT16(502, eeprom.modbusPort);
}

void test_rawToSystemConfig_baudrate(void) {
    CSVSystemConfig csv = createValidCSVConfig();
    EEPROMSystemConfig eeprom = rawToSystemConfig(csv);
    TEST_ASSERT_EQUAL_UINT32(9600, eeprom.baudrate);
}

void test_rawToSystemConfig_ip(void) {
    CSVSystemConfig csv = createValidCSVConfig();
    EEPROMSystemConfig eeprom = rawToSystemConfig(csv);
    TEST_ASSERT_EQUAL_UINT8(192, eeprom.ip[0]);
    TEST_ASSERT_EQUAL_UINT8(168, eeprom.ip[1]);
    TEST_ASSERT_EQUAL_UINT8(1, eeprom.ip[2]);
    TEST_ASSERT_EQUAL_UINT8(150, eeprom.ip[3]);
}

void test_rawToSystemConfig_mac(void) {
    CSVSystemConfig csv = createValidCSVConfig();
    EEPROMSystemConfig eeprom = rawToSystemConfig(csv);
    TEST_ASSERT_EQUAL_UINT8(0xDE, eeprom.mac[0]);
    TEST_ASSERT_EQUAL_UINT8(0xED, eeprom.mac[5]);
}

void test_rawToSystemConfig_rtu_config(void) {
    CSVSystemConfig csv = createValidCSVConfig();
    EEPROMSystemConfig eeprom = rawToSystemConfig(csv);
    TEST_ASSERT_EQUAL_UINT32(SERIAL_8N2, eeprom.rtuClientConfig);
}

void test_rawToSystemConfig_attempts(void) {
    CSVSystemConfig csv = createValidCSVConfig();
    EEPROMSystemConfig eeprom = rawToSystemConfig(csv);
    TEST_ASSERT_EQUAL_UINT8(3, eeprom.attempts);
}

// --- Boundary values ---
void test_validateCSVConfig_boundary_port_1(void) {
    CSVSystemConfig cfg = createValidCSVConfig();
    strcpy(cfg.port, "1");
    TEST_ASSERT_TRUE(validateCSVConfig(cfg));
}

void test_validateCSVConfig_boundary_port_65535(void) {
    CSVSystemConfig cfg = createValidCSVConfig();
    strcpy(cfg.port, "65535");
    TEST_ASSERT_TRUE(validateCSVConfig(cfg));
}

void test_validateCSVConfig_boundary_baudrate_300(void) {
    CSVSystemConfig cfg = createValidCSVConfig();
    strcpy(cfg.baudrate, "300");
    TEST_ASSERT_TRUE(validateCSVConfig(cfg));
}

void test_validateCSVConfig_boundary_baudrate_115200(void) {
    CSVSystemConfig cfg = createValidCSVConfig();
    strcpy(cfg.baudrate, "115200");
    TEST_ASSERT_TRUE(validateCSVConfig(cfg));
}

void test_validateCSVConfig_boundary_interframe_2(void) {
    CSVSystemConfig cfg = createValidCSVConfig();
    strcpy(cfg.interFrameDelay, "2");
    TEST_ASSERT_TRUE(validateCSVConfig(cfg));
}

void test_validateCSVConfig_boundary_interframe_250(void) {
    CSVSystemConfig cfg = createValidCSVConfig();
    strcpy(cfg.interFrameDelay, "250");
    TEST_ASSERT_TRUE(validateCSVConfig(cfg));
}

void test_validateCSVConfig_boundary_timeout_50(void) {
    CSVSystemConfig cfg = createValidCSVConfig();
    strcpy(cfg.responseTimeout, "50");
    TEST_ASSERT_TRUE(validateCSVConfig(cfg));
}

void test_validateCSVConfig_boundary_timeout_5000(void) {
    CSVSystemConfig cfg = createValidCSVConfig();
    strcpy(cfg.responseTimeout, "5000");
    TEST_ASSERT_TRUE(validateCSVConfig(cfg));
}

void test_validateCSVConfig_boundary_attempts_1(void) {
    CSVSystemConfig cfg = createValidCSVConfig();
    strcpy(cfg.attempts, "1");
    TEST_ASSERT_TRUE(validateCSVConfig(cfg));
}

void test_validateCSVConfig_boundary_attempts_5(void) {
    CSVSystemConfig cfg = createValidCSVConfig();
    strcpy(cfg.attempts, "5");
    TEST_ASSERT_TRUE(validateCSVConfig(cfg));
}

// --- parseSerialConfig (indirect) ---
void test_parseSerialConfig_all_valid(void) {
    const char* configs[] = {
        "SERIAL_5N1", "SERIAL_6N1", "SERIAL_7N1", "SERIAL_8N1",
        "SERIAL_5N2", "SERIAL_6N2", "SERIAL_7N2", "SERIAL_8N2",
        "SERIAL_5E1", "SERIAL_6E1", "SERIAL_7E1", "SERIAL_8E1",
        "SERIAL_5E2", "SERIAL_6E2", "SERIAL_7E2", "SERIAL_8E2",
        "SERIAL_5O1", "SERIAL_6O1", "SERIAL_7O1", "SERIAL_8O1",
        "SERIAL_5O2", "SERIAL_6O2", "SERIAL_7O2", "SERIAL_8O2"
    };
    for (int i = 0; i < 24; i++) {
        CSVSystemConfig cfg = createValidCSVConfig();
        strcpy(cfg.rtuConfig, configs[i]);
        TEST_ASSERT_TRUE_MESSAGE(validateCSVConfig(cfg), configs[i]);
    }
}

// ============================================================
// Integration test: config loading logic (simulates setup())
// ============================================================

static const EEPROMSystemConfig TEST_DEFAULT_CONFIG = {
    CONFIG_MAGIC_KEY,
    CONFIG_VERSION,
    {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED},
    {192, 168, 1, 150},
    {192, 168, 1, 1},
    {255, 255, 255, 0},
    {192, 168, 1, 1},
    502,
    9600,
    17, 22, 23,
    SERIAL_8N2,
    250,
    5000,
    3,
    10
};

SDManager testSdManager;

static bool loadConfigurationFromEEPROM(EEPROMSystemConfig& cfg) {
    E2PROM.begin();
    E2PROM.get(0, cfg);

    if (cfg.magic != CONFIG_MAGIC_KEY) {
        Serial.println("  Magic Key no coincide.");
        return false;
    }
    if (cfg.version != CONFIG_VERSION) {
        Serial.printf("  Version incompatible (%d != %d).\n", cfg.version, CONFIG_VERSION);
        return false;
    }
    if (!verifyConfigCRC(cfg)) {
        Serial.println("  CRC invalido.");
        return false;
    }
    Serial.println("  EEPROM validada OK.");
    return true;
}

const char* runConfigLoading(void) {
    EEPROMSystemConfig testCfg = TEST_DEFAULT_CONFIG;

    E2PROM.begin();

    bool loadedFromSD = false;
    esp_err_t sdResult = testSdManager.begin();
    Serial.printf("  [DEBUG] SD begin: %s\n", sdResult == ESP_OK ? "OK" : "FAIL");

    if (sdResult == ESP_OK) {
        bool fileExists = testSdManager.exists(PARAM_FILE);
        Serial.printf("  [DEBUG] File '%s' exists: %s\n", PARAM_FILE, fileExists ? "YES" : "NO");

        if (fileExists) {
            CSVSystemConfig configRaw;
            memset(&configRaw, 0, sizeof(configRaw));

            bool readOk = SDgetSystemConfig(&testSdManager, configRaw);
            Serial.printf("  [DEBUG] SDgetSystemConfig: %s\n", readOk ? "OK" : "FAIL");

            if (readOk) {
                bool validOk = validateCSVConfig(configRaw);
                Serial.printf("  [DEBUG] validateCSVConfig: %s\n", validOk ? "OK" : "FAIL");

                if (validOk) {
                    EEPROMSystemConfig configFromSD = rawToSystemConfig(configRaw);
                    E2PROM.put(0, configFromSD);
                    testCfg = configFromSD;
                    loadedFromSD = true;
                }
            }
        }
    }

    if (!loadedFromSD) {
        if (!loadConfigurationFromEEPROM(testCfg)) {
            testCfg = TEST_DEFAULT_CONFIG;
            size_t dataLen = offsetof(EEPROMSystemConfig, crc);
            testCfg.crc = calculateCRC16(
                reinterpret_cast<const uint8_t*>(&testCfg), dataLen);
            E2PROM.put(0, testCfg);
            if (testSdManager.isReady()) testSdManager.end();
            return "DEFAULT";
        } else {
            if (testSdManager.isReady()) testSdManager.end();
            return "EEPROM";
        }
    }

    if (testSdManager.isReady()) testSdManager.end();
    return "SD";
}

void test_integration_eeprom_wipe_loads_default(void) {
    EEPROMSystemConfig blank;
    memset(&blank, 0xFF, sizeof(blank));
    E2PROM.begin();
    E2PROM.put(0, blank);
    const char* source = runConfigLoading();
    TEST_ASSERT_EQUAL_STRING("DEFAULT", source);
}

void test_integration_eeprom_bad_crc_loads_default(void) {
    EEPROMSystemConfig cfg = TEST_DEFAULT_CONFIG;
    size_t dataLen = offsetof(EEPROMSystemConfig, crc);
    cfg.crc = calculateCRC16(reinterpret_cast<const uint8_t*>(&cfg), dataLen);
    cfg.crc ^= 0xFFFF;
    E2PROM.begin();
    E2PROM.put(0, cfg);
    const char* source = runConfigLoading();
    TEST_ASSERT_EQUAL_STRING("DEFAULT", source);
}

void test_integration_eeprom_bad_magic_loads_default(void) {
    EEPROMSystemConfig cfg = TEST_DEFAULT_CONFIG;
    cfg.magic = 0x00000000;
    size_t dataLen = offsetof(EEPROMSystemConfig, crc);
    cfg.crc = calculateCRC16(reinterpret_cast<const uint8_t*>(&cfg), dataLen);
    E2PROM.begin();
    E2PROM.put(0, cfg);
    const char* source = runConfigLoading();
    TEST_ASSERT_EQUAL_STRING("DEFAULT", source);
}

void test_integration_eeprom_valid_loads_from_eeprom(void) {
    EEPROMSystemConfig cfg = TEST_DEFAULT_CONFIG;
    size_t dataLen = offsetof(EEPROMSystemConfig, crc);
    cfg.crc = calculateCRC16(reinterpret_cast<const uint8_t*>(&cfg), dataLen);
    E2PROM.begin();
    E2PROM.put(0, cfg);
    const char* source = runConfigLoading();
    TEST_ASSERT_EQUAL_STRING("EEPROM", source);
}

// ============================================================
// Arduino setup: runs all unit tests + integration tests
// ============================================================

void setup() {
    delay(2000);
    Serial.begin(115200);
    while (!Serial) {}

    Serial.println("\n========================================");
    Serial.println("   GATEWAY CONFIG TESTS (ESP32)");
    Serial.println("========================================\n");

    _test_count = 0;
    _test_pass = 0;
    _test_fail = 0;

    Serial.println("[calculateCRC16]");
    RUN_TEST(test_calculateCRC16_known_data);
    RUN_TEST(test_calculateCRC16_empty_data);
    RUN_TEST(test_calculateCRC16_single_byte);

    Serial.println("\n[verifyConfigCRC]");
    RUN_TEST(test_verifyConfigCRC_valid);
    RUN_TEST(test_verifyConfigCRC_corrupted);
    RUN_TEST(test_verifyConfigCRC_zero);

    Serial.println("\n[validateCSVConfig]");
    RUN_TEST(test_validateCSVConfig_all_valid);
    RUN_TEST(test_validateCSVConfig_invalid_mac);
    RUN_TEST(test_validateCSVConfig_invalid_ip);
    RUN_TEST(test_validateCSVConfig_invalid_gateway);
    RUN_TEST(test_validateCSVConfig_invalid_port_zero);
    RUN_TEST(test_validateCSVConfig_invalid_port_too_high);
    RUN_TEST(test_validateCSVConfig_invalid_baudrate_low);
    RUN_TEST(test_validateCSVConfig_invalid_baudrate_high);
    RUN_TEST(test_validateCSVConfig_invalid_rtu_config);
    RUN_TEST(test_validateCSVConfig_invalid_interframe_delay);
    RUN_TEST(test_validateCSVConfig_invalid_interframe_delay_high);
    RUN_TEST(test_validateCSVConfig_invalid_response_timeout);
    RUN_TEST(test_validateCSVConfig_invalid_attempts_zero);
    RUN_TEST(test_validateCSVConfig_invalid_attempts_high);
    RUN_TEST(test_validateCSVConfig_missing_field);

    Serial.println("\n[rawToSystemConfig]");
    RUN_TEST(test_rawToSystemConfig_crc_valid);
    RUN_TEST(test_rawToSystemConfig_magic_key);
    RUN_TEST(test_rawToSystemConfig_version);
    RUN_TEST(test_rawToSystemConfig_port);
    RUN_TEST(test_rawToSystemConfig_baudrate);
    RUN_TEST(test_rawToSystemConfig_ip);
    RUN_TEST(test_rawToSystemConfig_mac);
    RUN_TEST(test_rawToSystemConfig_rtu_config);
    RUN_TEST(test_rawToSystemConfig_attempts);

    Serial.println("\n[Boundary values]");
    RUN_TEST(test_validateCSVConfig_boundary_port_1);
    RUN_TEST(test_validateCSVConfig_boundary_port_65535);
    RUN_TEST(test_validateCSVConfig_boundary_baudrate_300);
    RUN_TEST(test_validateCSVConfig_boundary_baudrate_115200);
    RUN_TEST(test_validateCSVConfig_boundary_interframe_2);
    RUN_TEST(test_validateCSVConfig_boundary_interframe_250);
    RUN_TEST(test_validateCSVConfig_boundary_timeout_50);
    RUN_TEST(test_validateCSVConfig_boundary_timeout_5000);
    RUN_TEST(test_validateCSVConfig_boundary_attempts_1);
    RUN_TEST(test_validateCSVConfig_boundary_attempts_5);

    Serial.println("\n[parseSerialConfig]");
    RUN_TEST(test_parseSerialConfig_all_valid);

    Serial.println("\n[Integration: config loading]");
    RUN_TEST(test_integration_eeprom_wipe_loads_default);
    RUN_TEST(test_integration_eeprom_bad_crc_loads_default);
    RUN_TEST(test_integration_eeprom_bad_magic_loads_default);
    RUN_TEST(test_integration_eeprom_valid_loads_from_eeprom);

    TEST_REPORT();

    Serial.println("\nIniciando menu interactivo en 3 segundos...");
    delay(3000);
}

// ============================================================
// Interactive menu for manual hardware testing
// ============================================================

void loop() {
    Serial.println();
    Serial.println("========================================");
    Serial.println("   GATEWAY INIT TEST - Menu");
    Serial.println("========================================");
    Serial.println("  1. [EEPROM] Borrar EEPROM (wipe)");
    Serial.println("  2. [EEPROM] Escribir config VALIDA");
    Serial.println("  3. [EEPROM] Escribir config con CRC INCORRECTA");
    Serial.println("  4. [EEPROM] Escribir config con magic INVALIDO");
    Serial.println("  5. [SD]     Preparar SD con CSV VALIDO");
    Serial.println("  6. [SD]     Preparar SD con CSV INVALIDO");
    Serial.println("  7. [SD]     Borrar sysconf.csv de SD");
    Serial.println("  8. [RUN]    Ejecutar config loading (simular setup)");
    Serial.println("  9. [VIEW]   Ver estado EEPROM");
    Serial.println("  0. [VIEW]   Ver config DEFAULT");
    Serial.println("  T. [TEST]   Re-ejecutar unit tests");
    Serial.println("  R. [REBOOT] Reiniciar ESP32");
    Serial.println("========================================");
    Serial.print("Opcion: ");

    while (!Serial.available()) {}
    char c = Serial.read();
    while (Serial.available()) Serial.read();

    switch (c) {
        case '1': {
            Serial.println("\n--- EEPROM WIPE ---");
            EEPROMSystemConfig blank;
            memset(&blank, 0xFF, sizeof(blank));
            E2PROM.begin();
            E2PROM.put(0, blank);
            Serial.println("EEPROM borrada (0xFF). Magic/CRC invalidos.");
            break;
        }
        case '2': {
            Serial.println("\n--- EEPROM: ESCRIBIR CONFIG VALIDA ---");
            EEPROMSystemConfig cfg = TEST_DEFAULT_CONFIG;
            size_t dataLen = offsetof(EEPROMSystemConfig, crc);
            cfg.crc = calculateCRC16(reinterpret_cast<const uint8_t*>(&cfg), dataLen);
            E2PROM.begin();
            E2PROM.put(0, cfg);
            Serial.printf("EEPROM escrita. Magic=0x%08X CRC=0x%04X (OK)\n", cfg.magic, cfg.crc);
            break;
        }
        case '3': {
            Serial.println("\n--- EEPROM: CRC INCORRECTA ---");
            EEPROMSystemConfig cfg = TEST_DEFAULT_CONFIG;
            size_t dataLen = offsetof(EEPROMSystemConfig, crc);
            cfg.crc = calculateCRC16(reinterpret_cast<const uint8_t*>(&cfg), dataLen);
            cfg.crc ^= 0xFFFF;
            E2PROM.begin();
            E2PROM.put(0, cfg);
            Serial.printf("EEPROM escrita. CRC=0x%04X (CORRUPTO)\n", cfg.crc);
            break;
        }
        case '4': {
            Serial.println("\n--- EEPROM: MAGIC INVALIDO ---");
            EEPROMSystemConfig cfg = TEST_DEFAULT_CONFIG;
            cfg.magic = 0x00000000;
            size_t dataLen = offsetof(EEPROMSystemConfig, crc);
            cfg.crc = calculateCRC16(reinterpret_cast<const uint8_t*>(&cfg), dataLen);
            E2PROM.begin();
            E2PROM.put(0, cfg);
            Serial.printf("EEPROM escrita. Magic=0x%08X (INVALIDO)\n", cfg.magic);
            break;
        }
        case '5': {
            Serial.println("\n--- SD: CSV VALIDO ---");
            if (testSdManager.begin() != ESP_OK) {
                Serial.println("ERROR: SD no disponible.");
                break;
            }
            File f = SD.open(PARAM_FILE, FILE_WRITE);
            if (!f) { Serial.println("ERROR al crear archivo."); testSdManager.end(); break; }
            f.println("Name;Value;Editable;coment;format");
            f.println(";;;;");
            f.println("Modbus TCP ;;;;");
            f.println("mac address;DE:AD:BE:EF:FE:ED;yes;;Formato XX:XX:XX:XX:XX:XX, octetos 0-255");
            f.println("IP address;192.168.1.150;yes;;Formato N.N.N.N, octetos 0-255");
            f.println("gateway;192.168.1.1;yes;;Formato N.N.N.N, octetos 0-255");
            f.println("subnet;255.255.255.0;yes;;Formato N.N.N.N, octetos 0-255");
            f.println("dns;192.168.1.1;yes;;Formato N.N.N.N, octetos 0-255");
            f.println("port;502;yes;modbus port, 502;1-65535");
            f.println(";;;;");
            f.println("Modbus RTU ;;;;");
            f.println("baudrate;9600;yes;;300-115200");
            f.println("txPin;RS485_TX;no;;");
            f.println("dePin;RS485_DE;no;;");
            f.println("rePin;RS485_RE;no;;");
            f.println("RTU Config;SERIAL_8N2;yes;8 bits, None parity, 2 stop bit;SERIAL_5N1, SERIAL_6N1, SERIAL_7N1, SERIAL_8N1, SERIAL_5N2, SERIAL_6N2, SERIAL_7N2, SERIAL_8N2");
            f.println("Inter-frame delay (ms);2;yes;;2-250 ms");
            f.println("Response Timeout (ms);1000;yes;min ;50-5000 ms");
            f.println("Attempts;1;no;Por defecto 1 intento con timeout;");
            f.println(";;;;");
            f.println("Internal Modbus slave;;;;");
            f.println("Slave ID;10;yes;virtual internal Weidos salve with id = 10;");
            f.close();
            Serial.println("CSV VALIDO escrito en SD.");
            break;
        }
        case '6': {
            Serial.println("\n--- SD: CSV INVALIDO ---");
            if (testSdManager.begin() != ESP_OK) {
                Serial.println("ERROR: SD no disponible.");
                break;
            }
            File f = SD.open(PARAM_FILE, FILE_WRITE);
            if (!f) { Serial.println("ERROR al crear archivo."); testSdManager.end(); break; }
            f.println("Name;Value;Editable;coment;format");
            f.println(";;;;");
            f.println("Modbus TCP ;;;;;");
            f.println("mac address;INVALID;yes;Bad MAC;Formato XX:XX:XX:XX:XX:XX");
            f.println("IP address;999.0.0.1;yes;Bad IP;Formato N.N.N.N");
            f.println("gateway;192.168.1.1;yes;GW;Formato N.N.N.N");
            f.println("subnet;255.255.255.0;yes;Sub;Formato N.N.N.N");
            f.println("dns;192.168.1.1;yes;DNS;Formato N.N.N.N");
            f.println("port;0;yes;Bad port;1-65535");
            f.println(";;;;");
            f.println("Modbus RTU ;;;;;");
            f.println("baudrate;9600;yes;Baud;300-115200");
            f.println("txPin;17;yes;TX;;");
            f.println("dePin;22;yes;DE;;");
            f.println("rePin;23;yes;RE;;");
            f.println("RTU Config;SERIAL_8N2;yes;RTU;SERIAL_8N2");
            f.println("Inter-frame delay (ms);250;yes;IFD;2-250 ms");
            f.println("Response Timeout (ms);5000;yes;Tmo;50-5000 ms");
            f.println("Attempts;3;yes;Att;;");
            f.println(";;;;");
            f.println("Internal Modbus slave;;;;");
            f.println("Slave ID;10;yes;SID;;");
            f.close();
            Serial.println("CSV INVALIDO escrito en SD (MAC, IP, puerto).");
            break;
        }
        case '7': {
            Serial.println("\n--- SD: BORRAR sysconf.csv ---");
            if (testSdManager.begin() != ESP_OK) {
                Serial.println("ERROR: SD no disponible.");
                break;
            }
            if (testSdManager.exists(PARAM_FILE)) {
                testSdManager.deleteFile(PARAM_FILE);
                Serial.println("sysconf.csv eliminado.");
            } else {
                Serial.println("sysconf.csv no existia.");
            }
            break;
        }
        case '8': {
            Serial.println("\n--- EJECUTANDO CONFIG LOADING ---");
            const char* source = runConfigLoading();
            Serial.println();
            Serial.println("========================================");
            Serial.printf("  RESULTADO: Config cargada desde: %s\n", source);
            Serial.println("========================================");

            E2PROM.begin();
            EEPROMSystemConfig verifyCfg;
            E2PROM.get(0, verifyCfg);
            bool eepromValid = (verifyCfg.magic == CONFIG_MAGIC_KEY) &&
                               (verifyCfg.version == CONFIG_VERSION) &&
                               verifyConfigCRC(verifyCfg);
            bool sdPresent = testSdManager.begin() == ESP_OK &&
                             testSdManager.exists(PARAM_FILE);
            if (testSdManager.isReady()) testSdManager.end();

            if (sdPresent) {
                TEST_ASSERT_EQUAL_STRING("SD", source);
            } else if (eepromValid) {
                TEST_ASSERT_EQUAL_STRING("EEPROM", source);
            } else {
                TEST_ASSERT_EQUAL_STRING("DEFAULT", source);
            }
            Serial.println("  Verificacion de source: PASSED");
            break;
        }
        case '9': {
            Serial.println("\n--- ESTADO EEPROM ---");
            E2PROM.begin();
            EEPROMSystemConfig cfg;
            E2PROM.get(0, cfg);
            Serial.printf("  Magic:   0x%08X %s\n", cfg.magic,
                          cfg.magic == CONFIG_MAGIC_KEY ? "(OK)" : "(INVALIDO)");
            Serial.printf("  Version: %d %s\n", cfg.version,
                          cfg.version == CONFIG_VERSION ? "(OK)" : "(INVALIDA)");
            Serial.printf("  CRC:     0x%04X %s\n", cfg.crc,
                          verifyConfigCRC(cfg) ? "(VALIDO)" : "(INVALIDO)");
            Serial.printf("  MAC:     %02X:%02X:%02X:%02X:%02X:%02X\n",
                          cfg.mac[0], cfg.mac[1], cfg.mac[2], cfg.mac[3], cfg.mac[4], cfg.mac[5]);
            Serial.printf("  IP:      %d.%d.%d.%d\n", cfg.ip[0], cfg.ip[1], cfg.ip[2], cfg.ip[3]);
            Serial.printf("  Gateway: %d.%d.%d.%d\n", cfg.gateway[0], cfg.gateway[1], cfg.gateway[2], cfg.gateway[3]);
            Serial.printf("  Subnet:  %d.%d.%d.%d\n", cfg.subnet[0], cfg.subnet[1], cfg.subnet[2], cfg.subnet[3]);
            Serial.printf("  DNS:     %d.%d.%d.%d\n", cfg.dns[0], cfg.dns[1], cfg.dns[2], cfg.dns[3]);
            Serial.printf("  Port:    %d\n", cfg.modbusPort);
            Serial.printf("  Baud:    %d\n", cfg.baudrate);
            Serial.printf("  Pins:    TX=%d DE=%d RE=%d\n", cfg.txPin, cfg.dePin, cfg.rePin);
            Serial.printf("  RTUCfg:  0x%08X\n", cfg.rtuClientConfig);
            Serial.printf("  IFDelay: %d ms\n", cfg.interFrameDelay);
            Serial.printf("  Timeout: %d ms\n", cfg.responseTimeout);
            Serial.printf("  Attempts:%d\n", cfg.attempts);
            Serial.printf("  SlaveID: %d\n", cfg.internal_slave_id);
            break;
        }
        case '0': {
            Serial.println("\n--- CONFIG DEFAULT DEFINIDA ---");
            Serial.printf("  Magic:   0x%08X\n", TEST_DEFAULT_CONFIG.magic);
            Serial.printf("  Version: %d\n", TEST_DEFAULT_CONFIG.version);
            Serial.printf("  MAC:     %02X:%02X:%02X:%02X:%02X:%02X\n",
                          TEST_DEFAULT_CONFIG.mac[0], TEST_DEFAULT_CONFIG.mac[1],
                          TEST_DEFAULT_CONFIG.mac[2], TEST_DEFAULT_CONFIG.mac[3],
                          TEST_DEFAULT_CONFIG.mac[4], TEST_DEFAULT_CONFIG.mac[5]);
            Serial.printf("  IP:      %d.%d.%d.%d\n",
                          TEST_DEFAULT_CONFIG.ip[0], TEST_DEFAULT_CONFIG.ip[1],
                          TEST_DEFAULT_CONFIG.ip[2], TEST_DEFAULT_CONFIG.ip[3]);
            Serial.printf("  Gateway: %d.%d.%d.%d\n",
                          TEST_DEFAULT_CONFIG.gateway[0], TEST_DEFAULT_CONFIG.gateway[1],
                          TEST_DEFAULT_CONFIG.gateway[2], TEST_DEFAULT_CONFIG.gateway[3]);
            Serial.printf("  Port:    %d\n", TEST_DEFAULT_CONFIG.modbusPort);
            Serial.printf("  Baud:    %d\n", TEST_DEFAULT_CONFIG.baudrate);
            break;
        }
        case 't':
        case 'T': {
            Serial.println("Re-ejecutando tests...");
            delay(500);
            ESP.restart();
            break;
        }
        case 'r':
        case 'R':
            Serial.println("Reiniciando...");
            delay(500);
            ESP.restart();
            break;
        default:
            Serial.println("\nOpcion no valida.");
            break;
    }

    Serial.println("\nPresione ENTER para continuar...");
    while (!Serial.available()) {}
    while (Serial.available()) Serial.read();
}
