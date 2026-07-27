#include <Arduino.h>

//#include "FuncInternalClientOLED.h" // La cabecera gestiona el 'extern' de slaves
#include "displayOLEDManager.h"

#include "ModbusRtuLock.h"
#include "ModbusRTUClient.h"
#include "ModbusRTUClientWrapper.h"

#include "ModbusInternalClient.h"
#include "InternalModbusSlave.h"

#include "ModbusTCPBridge.h"

#include "SDManager.h"
#include "systemConfig.h"

static const char* TAG = "MAIN_APP"; 

SDManager sdManager; 

const EEPROMSystemConfig DEFAULT_SYS_CONFIG = {
    CONFIG_MAGIC_KEY,
    CONFIG_VERSION,
    {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED}, // MAC
    {192, 168, 1, 150},                  // IP
    {192, 168, 1, 1},                    // Gateway
    {255, 255, 255, 0},                  // Subnet
    {192, 168, 1, 1},                    // DNS
    502,                                 // Modbus TCP Port
    9600,                                // Baudrate
    17, 22, 23,                          // TX, DE, RE Pins
    SERIAL_8N2,                          // Serial Config
    250,                                 // Inter-frame delay
    5000,                                // Response Timeout
    3,                                   // Attempts
    10                                   // Internal Slave ID
};

EEPROMSystemConfig sysConfig = DEFAULT_SYS_CONFIG;  
 
// --- INSTANCIAS GLOBALES ÚNICAS (Sin doble constructor) ---
// Inicialmente arranca con el DummyLock interno por defecto
ModbusInternalClient internalClient(&internalSlaveID10); 
ModbusRtuClient mbRtu(&ModbusRTUClient);
ModbusTcpBridge modbusTcpBridge(&mbRtu); 

TaskHandle_t ModbusGatewayTaskHandle = NULL;
SemaphoreHandle_t xModbusDataMutex = NULL;  
SemaphoreHandle_t xModbusRTUMutex = NULL; 

// Puntero para usar el wrapper del Lock tanto en el main como en el puente
ModbusRtuLock rtuThreadLock;

displayOLEDManager disp; 

// --- ARRAY CON CONFIGURACIÓN DE ATRIBUTOS ---
ModbusSlaveData slaves[] = {
    { "CL2",    "ppm",   3,      1,      0,      2,      0x03,      {0, 0},      0.0,      false,      0,      false,    0},
    { "COND",   "us",    1,      2,      0,      2,      0x03,      {0, 0},      0.0,      false,      0,      false,    0},
    { "REDOX",  "mV",    1,      3,      0,      2,      0x03,      {0, 0},      0.0,      false,      0,      false,    0},
    { "TURB",   "NTU",   3,      4,      0,      2,      0x03,      {0, 0},      0.0,      false,      0,      false,    0},
    { "PH",     "pH",    2,      5,      0,      2,      0x03,      {0, 0},      0.0,      false,      0,      false,    0}
};

const uint8_t NUM_SLAVES = sizeof(slaves) / sizeof(slaves[0]);

void checkSlaveFlagsAndTimeouts();
void updateSlave(ModbusSlaveData* slave);
bool reqSlaveInternalClient(ModbusSlaveData* slave);
bool loadConfigurationFromEEPROM(EEPROMSystemConfig& cfg);   

// NOTE: Modifies _mbClient and _lock per-request. Safe because this callback
// and all usage run sequentially in modbusGatewayTask with no preemption.
void checkTCPReqCallback(const modbusStruct& req) {
    if (req.slaveID == sysConfig.internal_slave_id) {
        modbusTcpBridge.setModbusClient(&internalClient);
        modbusTcpBridge.setThreadLock(nullptr); // El puente usará _defaultLock (DummyLock) automáticamente
    } else {
        modbusTcpBridge.setModbusClient(&mbRtu); 
        modbusTcpBridge.setThreadLock(&rtuThreadLock); // Bloquea el HW real
    }
}

void checkTCPDataCallback(const modbusStruct& req, uint16_t index, uint16_t& value) {
    for (uint8_t i = 0; i < NUM_SLAVES; i++) {
        if (req.slaveID == slaves[i].slaveID && req.address == slaves[i].address && req.quantity_value == slaves[i].quantity && req.functionCode == slaves[i].functionCode) {
            if (index < slaves[i].quantity) {
                if (xSemaphoreTake(xModbusDataMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
                    slaves[i].rawBuffer[index] = value; 
                    if (index == slaves[i].quantity - 1 ) { 
                        slaves[i].flagUpdate = true; 
                    }
                    xSemaphoreGive(xModbusDataMutex);
                }
            }
            break; 
        }
    }
}

void modbusGatewayTask(void * pvParameters) {
    for(;;) {
        modbusTcpBridge.process();
        vTaskDelay(pdMS_TO_TICKS(1)); 
    }
}

void setup() {
    Serial.begin(115200);
    while(!Serial){}

    E2PROM.begin(); 

    bool loadedFromSD = false;
    if(sdManager.begin() == ESP_OK){ // TODO mejorar 
        if(sdManager.exists(PARAM_FILE)){
            ESP_LOGI(TAG, "[SD] Archivo de configuración encontrado. Cargando...");
            CSVSystemConfig configRaw;

            if (!SDgetSystemConfig(&sdManager, configRaw)) {
                ESP_LOGE(TAG, "[SD] Faltan parámetros en el CSV. Ignorando SD.");
                logErrorToSD(&sdManager, "Missing parameters in CSV");
            } else if (!validateCSVConfig(configRaw)) {
                ESP_LOGE(TAG, "[SD] Configuración CSV inválida. Ignorando SD.");
                logErrorToSD(&sdManager, "Invalid CSV configuration values");
            } else {
                EEPROMSystemConfig configFromSD = rawToSystemConfig(configRaw);

                E2PROM.put(0, configFromSD);
                sysConfig = configFromSD;
                loadedFromSD = true; 
                ESP_LOGI(TAG, "[SD -> EEPROM] Configuración guardada en EEPROM exitosamente.");
            }
            
        }else{
            ESP_LOGW(TAG, "[SD] Advertencia: La tarjeta SD está montada pero no contiene %s", PARAM_FILE);
            logErrorToSD(&sdManager, "Config file not found: " PARAM_FILE);
        }
    }else{
        ESP_LOGW(TAG, "[SD] Tarjeta SD no detectada o fallo al montar.");
    }

    if (!loadedFromSD) {
        ESP_LOGI(TAG, "[EEPROM] Intentando cargar configuración desde EEPROM...");

        if(!loadConfigurationFromEEPROM(sysConfig)){
            ESP_LOGE(TAG, "[CRÍTICO] Fallo de SD y EEPROM inválida. Cargando valores por defecto (FLASH)...");
            logErrorToSD(&sdManager, "EEPROM configuration invalid, loading defaults");
            sysConfig = DEFAULT_SYS_CONFIG;
            size_t dataLen = offsetof(EEPROMSystemConfig, crc);
            sysConfig.crc = calculateCRC16(reinterpret_cast<const uint8_t*>(&sysConfig), dataLen);
            E2PROM.put(0, sysConfig);
        } 
    }

    if (sdManager.isReady()) {
        sdManager.end();
    }
    
    printConfig(sysConfig);

    IPAddress ip(sysConfig.ip);
    IPAddress gateway(sysConfig.gateway);
    IPAddress subnet(sysConfig.subnet);
    IPAddress dns(sysConfig.dns);

    // 1. Crear Semáforos primero
    xModbusDataMutex = xSemaphoreCreateMutex(); 
    xModbusRTUMutex = xSemaphoreCreateMutex(); 

    if(xModbusDataMutex == NULL || xModbusRTUMutex == NULL) while(1);

    rtuThreadLock.init(xModbusRTUMutex); 

    // 2. Instanciar el Lock pasándole el Semáforo de FreeRTOS real
    //rtuThreadLock = new FreeRtosModbusLock(xModbusRTUMutex); // TODO , no me gusta en memoria dinamica

    RS485.setPins(RS485_TX, RS485_DE, RS485_RE);
    ModbusRTUClient.begin(sysConfig.baudrate, (uint32_t)sysConfig.rtuClientConfig);
    ModbusRTUClient.setTimeout(sysConfig.responseTimeout); // esto tiene que poder funcionar a 5000....MODIFICAR 
    modbusTcpBridge.setInterFrameDelay(sysConfig.interFrameDelay);

    internalSlaveID10.begin(); // inicializamos el mapa, quiza esto deberia ir en otro sitio. 
   

    // 3. Vincular dinámicamente el Lock y el Interceptor al objeto global estable
    modbusTcpBridge.setThreadLock(&rtuThreadLock); 
    modbusTcpBridge.setInterceptor(checkTCPDataCallback);
    modbusTcpBridge.setTCPReqCallback(checkTCPReqCallback);
    modbusTcpBridge.begin(sysConfig.modbusPort, sysConfig.mac, ip, dns, gateway, subnet);

    xTaskCreatePinnedToCore(modbusGatewayTask, "ModbusGatewayTask", 4096, NULL, 3, &ModbusGatewayTaskHandle, 0);

    //initOLED(); 
    disp.initOLED(slaves, NUM_SLAVES, xModbusDataMutex); 

    for(int i = 0; i < NUM_SLAVES; i++ ){ 
        reqSlaveInternalClient(&slaves[i]); 
    }

    delay(1000);
}

void loop() {
    checkSlaveFlagsAndTimeouts();
    //updateOLED();
    disp.updateOLED(); 
}

void checkSlaveFlagsAndTimeouts() {
    unsigned long now = millis(); 

    for (uint8_t i = 0; i < NUM_SLAVES; i++) {
        if (slaves[i].flagUpdate) {
            updateSlave(&slaves[i]); 
        }

        if (now - slaves[i].lastTimeReference > INTERNAL_POLL_THRESHOLD) {  
            if(!reqSlaveInternalClient(&slaves[i])){
                slaves[i].errCounter++; 
                slaves[i].lastTimeReference = now; 
                if(slaves[i].errCounter >= 5){ 
                    slaves[i].errCounter = 5;  
                }
            }
        }
    }
}

void updateSlave(ModbusSlaveData* slave){
    if (xSemaphoreTake(xModbusDataMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        if (slave->flagUpdate) {
            int32_t combinado = ((uint32_t)slave->rawBuffer[0] << 16) | slave->rawBuffer[1];
            slave->rawBuffer[0] = 0; 
            slave->rawBuffer[1] = 0; 
            slave->convertedData = combinado / 1000.0;
            slave->lastTimeReference = millis(); 
                  
            slave->flagUpdate = false; 
            slave->isNew = true; 
            slave->errCounter = 0; 
        }
        xSemaphoreGive(xModbusDataMutex);
    }
}

bool reqSlaveInternalClient(ModbusSlaveData* slave){ 
    uint16_t aux_data[2] = {0, 0};
    bool lecturaExitosa = false;

    // Sincronización directa usando el objeto (eliminado el check de nullptr)
    if (modbusTcpBridge.isTcpTransferActive()) {
        return false;  // TCP request en progreso, saltar este ciclo
    }
    rtuThreadLock.lock(); 
    delay(sysConfig.interFrameDelay);
    
    int dataType = ModbusTcpBridge::getModbusClientDataType(slave->functionCode);
    
    if(ModbusRTUClient.requestFrom(slave->slaveID, dataType, slave->address, slave->quantity)){
        for (int j = 0; j < slave->quantity; j++) {
            aux_data[j] = ModbusRTUClient.read();  
        }
        lecturaExitosa = true;
    }
    delay(sysConfig.interFrameDelay);
    rtuThreadLock.unlock(); 

    if (lecturaExitosa) {
        if (xSemaphoreTake(xModbusDataMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
            for (int i = 0; i < slave->quantity; i++) {
                slave->rawBuffer[i] = aux_data[i]; 
            }
            slave->flagUpdate = true;
            xSemaphoreGive(xModbusDataMutex);
        }
    } 
    return lecturaExitosa; 
}

bool loadConfigurationFromEEPROM(EEPROMSystemConfig& cfg) {
    E2PROM.begin();
    E2PROM.get(0, cfg);

    if (cfg.magic != CONFIG_MAGIC_KEY) {
        ESP_LOGE(TAG, "Magic Key no coincide. EEPROM no inicializada.");
        return false;
    }

    if (cfg.version != CONFIG_VERSION) {
        ESP_LOGW(TAG, "Version incompatible (EEPROM: %d, FW: %d). Re-inicializando.",
                 cfg.version, CONFIG_VERSION);
        return false;
    }

    if (!verifyConfigCRC(cfg)) {
        ESP_LOGE(TAG, "CRC invalido. Datos EEPROM corruptos.");
        return false;
    }

    ESP_LOGI(TAG, "Configuracion EEPROM validada (v%d, CRC ok).", cfg.version);
    return true;
}