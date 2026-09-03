#include <unity.h>

// Stubs MUST come before project headers
#include "test_stubs.h"

// Now include the source under test directly
// This gives us access to all functions defined in SystemConfig.cpp
#include "../../src/systemConfig.h"

// Include implementation to get function definitions
// (SystemConfig.cpp will be compiled as part of this test file)
#include "../../src/SystemConfig.cpp"

// ============================================================
// TEST: calculateCRC16
// ============================================================

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

// ============================================================
// TEST: verifyConfigCRC
// ============================================================

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

// ============================================================
// TEST: validateCSVConfig
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

// ============================================================
// TEST: rawToSystemConfig
// ============================================================

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

// ============================================================
// TEST: parseIP (static, tested via validateCSVConfig indirectly)
// We test it through the public interface
// ============================================================

void test_parseIP_valid_addresses(void) {
    CSVSystemConfig cfg = createValidCSVConfig();
    strcpy(cfg.ip, "10.0.0.1");
    strcpy(cfg.gateway, "10.0.0.254");
    strcpy(cfg.subnet, "255.0.0.0");
    strcpy(cfg.dns, "8.8.8.8");
    TEST_ASSERT_TRUE(validateCSVConfig(cfg));
    EEPROMSystemConfig eeprom = rawToSystemConfig(cfg);
    TEST_ASSERT_EQUAL_UINT8(10, eeprom.ip[0]);
    TEST_ASSERT_EQUAL_UINT8(0, eeprom.ip[1]);
    TEST_ASSERT_EQUAL_UINT8(0, eeprom.ip[2]);
    TEST_ASSERT_EQUAL_UINT8(1, eeprom.ip[3]);
}

void test_parseIP_boundary_values(void) {
    CSVSystemConfig cfg = createValidCSVConfig();
    strcpy(cfg.ip, "255.255.255.255");
    strcpy(cfg.gateway, "0.0.0.0");
    strcpy(cfg.subnet, "1.2.3.4");
    strcpy(cfg.dns, "11.22.33.44");
    TEST_ASSERT_TRUE(validateCSVConfig(cfg));
}

// ============================================================
// TEST: Edge cases
// ============================================================

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

// ============================================================
// TEST: parseSerialConfig (tested through rawToSystemConfig)
// ============================================================

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
        TEST_ASSERT_TRUE(validateCSVConfig(cfg));
    }
}

// ============================================================
// MAIN
// ============================================================

int main(void) {
    UNITY_BEGIN();

    // calculateCRC16
    RUN_TEST(test_calculateCRC16_known_data);
    RUN_TEST(test_calculateCRC16_empty_data);
    RUN_TEST(test_calculateCRC16_single_byte);

    // verifyConfigCRC
    RUN_TEST(test_verifyConfigCRC_valid);
    RUN_TEST(test_verifyConfigCRC_corrupted);
    RUN_TEST(test_verifyConfigCRC_zero);

    // validateCSVConfig - invalid values
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

    // rawToSystemConfig
    RUN_TEST(test_rawToSystemConfig_crc_valid);
    RUN_TEST(test_rawToSystemConfig_magic_key);
    RUN_TEST(test_rawToSystemConfig_version);
    RUN_TEST(test_rawToSystemConfig_port);
    RUN_TEST(test_rawToSystemConfig_baudrate);
    RUN_TEST(test_rawToSystemConfig_ip);
    RUN_TEST(test_rawToSystemConfig_mac);
    RUN_TEST(test_rawToSystemConfig_rtu_config);
    RUN_TEST(test_rawToSystemConfig_attempts);

    // parseIP (indirect)
    RUN_TEST(test_parseIP_valid_addresses);
    RUN_TEST(test_parseIP_boundary_values);

    // Boundary values
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

    // parseSerialConfig (indirect)
    RUN_TEST(test_parseSerialConfig_all_valid);

    return UNITY_END();
}
