#include <Arduino.h>

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
    { "CL2",    "ppm",   3,     1,     0,     MAX_REGISTER_QUANTITY,    0x03,     {0, 0},     0.0,      false,      0,      false,    0},
    { "COND",   "us",    1,     2,     0,     MAX_REGISTER_QUANTITY,    0x03,     {0, 0},     0.0,      false,      0,      false,    0},
    { "REDOX",  "mV",    1,     3,     0,     MAX_REGISTER_QUANTITY,    0x03,     {0, 0},     0.0,      false,      0,      false,    0},
    { "TURB",   "NTU",   3,     4,     0,     MAX_REGISTER_QUANTITY,    0x03,     {0, 0},     0.0,      false,      0,      false,    0},
    { "PH",     "pH",    2,     5,     0,     MAX_REGISTER_QUANTITY,    0x03,     {0, 0},     0.0,      false,      0,      false,    0}
};

const uint8_t NUM_SLAVES = sizeof(slaves) / sizeof(slaves[0]);

void checkSlaveFlagsAndTimeouts();
void updateSlave(ModbusSlaveData* slave);
bool reqSlaveInternalClient(ModbusSlaveData* slave);

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
        if (req.slaveID == slaves[i].slaveID && req.address == slaves[i].address && req.quantity_value >= slaves[i].quantity && req.functionCode == slaves[i].functionCode) {
            if (index < MAX_REGISTER_QUANTITY) {
                if (xSemaphoreTake(xModbusDataMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
                    slaves[i].rawBuffer[index] = value; 
                    if (index == slaves[i].quantity - 1 ) { 
                        slaves[i].flagUpdate = true; 
                    }
                    xSemaphoreGive(xModbusDataMutex);
                } else {
                    ESP_LOGW(TAG, "Data mutex timeout for slave %d index %d", req.slaveID, index);
                }
            }
            break; 
        }
    }
}

void modbusGatewayTask(void * pvParameters) {
    uint32_t loopCount = 0;
    for(;;) {
        modbusTcpBridge.process();

        //if (++loopCount % 30000 == 0) {
        //    UBaseType_t highWater = uxTaskGetStackHighWaterMark(NULL);
        //    ESP_LOGI(TAG, "Gateway stack high water: %d bytes free", highWater * 4);
        //}

        vTaskDelay(pdMS_TO_TICKS(1)); 
    }
}

void setup() {
    Serial.begin(115200);
    while(!Serial){}

    ConfigSource configSource = loadSystemConfig(&sdManager, sysConfig, DEFAULT_SYS_CONFIG);
    Serial.printf("Configuration loaded from: %s\r\n", 
             configSource == CONFIG_FROM_SD ? "SD" : 
             configSource == CONFIG_FROM_EEPROM ? "EEPROM" : "DEFAULT");
    
    printConfig(sysConfig);

    IPAddress ip(sysConfig.ip);
    IPAddress gateway(sysConfig.gateway);
    IPAddress subnet(sysConfig.subnet);
    IPAddress dns(sysConfig.dns);

    // 1. Crear Semáforos primero
    xModbusDataMutex = xSemaphoreCreateMutex(); 
    xModbusRTUMutex = xSemaphoreCreateMutex(); 

    if(xModbusDataMutex == NULL || xModbusRTUMutex == NULL) {
        ESP_LOGE(TAG, "CRITICAL: Failed to create mutexes!");
        delay(1000);
        esp_restart();
    }

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
    Serial.println(); 
    Serial.println("Gateway runing...");
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
    // C1: tryLock elimina la race condition entre isTcpTransferActive() y lock()
    if (!rtuThreadLock.tryLock(100)) {
        return false;  // TCP request en progreso o mutex ocupado
    }
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
