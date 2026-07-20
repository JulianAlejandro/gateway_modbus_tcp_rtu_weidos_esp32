

#include <Arduino.h>
#include "SDManager.h"
#include "SDSystemConfig.h"
#include "systemConfig.h"

// Objeto manejador de la tarjeta SD (ajusta si la inicialización en tu proyecto requiere parámetros)
SDManager sdManager; 

bool parseIP(const char* str, uint8_t out[4]);
bool parseMAC(const char* str, uint8_t out[6]);
uint16_t parseSerialConfig(const char* str);
int parsePin(const char* str, int defaultPin);
SystemConfig rawToSystemConfig(const SystemConfigRaw& raw); 
void printConfig(const SystemConfig& cfg); 

void setup() {
    Serial.begin(115200);
    while (!Serial) {} // Esperar puerto serie
    delay(2000); 

    // 1. Inicializar SIEMPRE la EEPROM al arrancar la aplicación
    E2PROM.begin();

    // 2. Intentar leer desde la SD
    if (sdManager.begin() == ESP_OK) {
        Serial.println("[SD] Tarjeta detectada. Leyendo configuración...");
        
        SystemConfigRaw configRaw = SDgetSystemConfig(&sdManager);
        
        // Asignación directa a la variable existente (sin volver a declarar SystemConfig)
        SystemConfig configFromSD = rawToSystemConfig(configRaw);
        
        // Escribir en la EEPROM
        E2PROM.put(0, configFromSD);
        Serial.println("[SD] Configuración copiada a la EEPROM.");
    } else {
        Serial.println("[SD] No se pudo montar la SD. Se usará la configuración existente en EEPROM.");
    }

    // 3. Leer de la EEPROM (ahora es 100% seguro porque E2PROM.begin() ya se ejecutó)
    SystemConfig readData; 
    E2PROM.get(0, readData); 

    // 4. Validar Magic Key
    if (readData.magic == CONFIG_MAGIC_KEY) {
        Serial.println("\n[ÉXITO] Configuración cargada y verificada desde la EEPROM.");
        printConfig(readData);
    } else {
        Serial.println("\n[ERROR] Configuración no válida en EEPROM (Magic Key incorrecta o memoria vacía).");
    }
}

void loop() {
    // No se requiere ejecutar código periódico
    delay(5000);
}


// Función para parsear texto "192.168.1.150" a uint8_t[4]
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

// Función para parsear MAC "DE:AD:BE:EF:FE:ED" a uint8_t[6]
bool parseMAC(const char* str, uint8_t out[6]) {
    int m[6];
    if (sscanf(str, "%x:%x:%x:%x:%x:%x", &m[0], &m[1], &m[2], &m[3], &m[4], &m[5]) == 6) {
        for(int i = 0; i < 6; i++) out[i] = (uint8_t)m[i];
        return true;
    }
    return false;
}

// Mapeo de constantes de configuración Serial
uint16_t parseSerialConfig(const char* str) {
    if (strcmp(str, "SERIAL_8N1") == 0) return (uint16_t)SERIAL_8N1;
    if (strcmp(str, "SERIAL_8E1") == 0) return (uint16_t)SERIAL_8E1;
    if (strcmp(str, "SERIAL_8O1") == 0) return (uint16_t)SERIAL_8O1;
    return (uint16_t)SERIAL_8N1; // Default
}

// Mapeo para pines (soporta tanto números como alias comunes)
int parsePin(const char* str, int defaultPin) {
    if (strcmp(str, "RS485_TX") == 0) return RS485_TX;
    if (strcmp(str, "RS485_DE") == 0) return RS485_DE;
    if (strcmp(str, "RS485_RE") == 0) return RS485_RE;
    
    // Si metieron un número como "17" o "22"
    int pin = atoi(str);
    return (pin != 0 || strcmp(str, "0") == 0) ? pin : defaultPin;
}


// CONVERSOR PRINCIPAL: SystemConfigRaw -> SystemConfig
SystemConfig rawToSystemConfig(const SystemConfigRaw& raw) {
    SystemConfig config;
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

    // 3. Slave Interno
    config.internal_slave_id = (uint8_t)atoi(raw.internalSlaveId);

    return config;
}

void printConfig(const SystemConfig& cfg) {
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
    Serial.printf("Internal Slave ID: %d\n", cfg.internal_slave_id);
    Serial.println("---------------------------------");
}

/*
#include <Arduino.h>

#include "displayOLEDManager.h"

#include "ModbusRtuLock.h"
#include "ModbusRTUClient.h"
#include "ModbusRTUClientWrapper.h"

#include "ModbusInternalClient.h"
#include "InternalModbusSlave.h"

#include "ModbusTCPBridge.h"

#include "systemConfig.h"

SystemConfig sysConfig;

// --- INSTANCIAS GLOBALES ÚNICAS (Sin doble constructor) ---
// Inicialmente arranca con el DummyLock interno por defecto
ModbusInternalClient internalClient(&internalSlaveID10); 
ModbusRtuClient mbRtu(&ModbusRTUClient);
ModbusTcpBridge modbusTcpBridge(502, &mbRtu); // TODO este dato se debe tomar de sysConfig

TaskHandle_t ModbusGatewayTaskHandle = NULL;
SemaphoreHandle_t xModbusDataMutex = NULL;  
SemaphoreHandle_t xModbusRTUMutex = NULL; 

// Puntero para usar el wrapper del Lock tanto en el main como en el puente
ModbusRtuLock rtuThreadLock;

displayOLEDManager disp; 

// --- ARRAY CON CONFIGURACIÓN DE ATRIBUTOS ---
ModbusSlaveData slaves[] = {
    { "CL2",       "ppm",    3,      1,      0,      2,      0x03,      {0, 0},      0.0,      false,      0,      false,    0},
    { "COND",      "us",     1,      2,      0,      2,      0x03,      {0, 0},      0.0,      false,      0,      false,    0},
    { "REDOX",     "mV",     1,      3,      0,      2,      0x03,      {0, 0},      0.0,      false,      0,      false,    0},
    { "TURB",      "NTU",    3,      4,      0,      2,      0x03,      {0, 0},      0.0,      false,      0,      false,    0},
    { "PH",        "pH",     2,      5,      0,      2,      0x03,      {0, 0},      0.0,      false,      0,      false,    0}
};

const uint8_t NUM_SLAVES = sizeof(slaves) / sizeof(slaves[0]);

void checkSlaveFlagsAndTimeouts();
void updateSlave(ModbusSlaveData* slave);
bool reqSlaveInternalClient(ModbusSlaveData* slave);
SystemConfig loadConfigurationFromEEPROM();  

void checkTCPReqCallback(const modbusStruct& req) { // TODO: pensar en como podemos mejorar el asunto de relacion entre HW y mutex 
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
                slaves[i].rawBuffer[index] = value; 
                if (index == slaves[i].quantity - 1 ) { 
                    slaves[i].flagUpdate = true; 
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

    //Cargar la configuracion desde la EEPRIM antes de iniciar los perifericos 
    sysConfig = loadConfigurationFromEEPROM(); 

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

    internalSlaveID10.begin(); // inicializamos el mapa, quiza esto deberia ir en otro sitio. 
   

    // 3. Vincular dinámicamente el Lock y el Interceptor al objeto global estable
    modbusTcpBridge.setThreadLock(&rtuThreadLock); 
    modbusTcpBridge.setInterceptor(checkTCPDataCallback);
    modbusTcpBridge.setTCPReqCallback(checkTCPReqCallback);
    modbusTcpBridge.begin(sysConfig.mac, ip, dns, gateway, subnet);

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
    rtuThreadLock.lock(); 
    
    int dataType = ModbusTcpBridge::getModbusClientDataType(slave->functionCode);
    
    if(ModbusRTUClient.requestFrom(slave->slaveID, dataType, slave->address, slave->quantity)){
        for (int j = 0; j < slave->quantity; j++) {
            aux_data[j] = ModbusRTUClient.read();  
        }
        lecturaExitosa = true;
    }
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

SystemConfig loadConfigurationFromEEPROM() {
    E2PROM.begin();
    
    SystemConfig config;
    E2PROM.get(0, config);

    // Validación mediante Magic Key
    if (config.magic != CONFIG_MAGIC_KEY) {
        Serial.println("[ERROR EEPROM] Configuración no encontrada o corrupta. Bucle de seguridad activado.");
        while(1) { delay(1000); } // Bloquea si la memoria no está correctamente inicializada
    }

    Serial.println("[EEPROM] Configuración cargada correctamente desde la EEPROM.");
    return config;
}

*/


/*

//----------------------------GATEWAY MODBUS TCP RT with internal Client id = 10------------------------------

#include <Arduino.h>
#include <esp_task_wdt.h> 

#include "ModbusRTUClient.h"
#include "ModbusRTUClientManager.h"

#include "InternalModbusSlave.h"
#include "ModbusInternalClient.h"

#include "ModbusTCPBridge.h"

//----------CONFIGURABLE GLOBAL VARS------------

//Mosbus TCP vars
byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
IPAddress ip(192, 168, 1, 150);
IPAddress gateway(192, 168, 1, 1);    
IPAddress subnet(255, 255, 255, 0);  
IPAddress dns(192, 168, 1, 1);

uint16_t modbusPort = 502;   

//modbus RTU vars
uint32_t baudrate = 9600;
int txPin = RS485_TX;  
int dePin = RS485_DE;  
int rePin = RS485_RE;
uint16_t rtuClientConfig = (uint16_t)SERIAL_8N1; 

//internal Client 
const uint8_t internal_slave_id = 10;  
//-------------------------------------------------

ModbusInternalClient internalClient(&internalSlaveID10); 
ModbusRtuClientManager mbRtuClientManager(&ModbusRTUClient);

ModbusTcpBridge modbusTcpBridge(modbusPort, &mbRtuClientManager); 

void checkTCPReqCallback(const modbusStruct& req) { 
    if (req.slaveID == internal_slave_id) {
        modbusTcpBridge.setModbusClient(&internalClient);
    } else {
        modbusTcpBridge.setModbusClient(&mbRtuClientManager); 
    }
}

void setup() {
  Serial.begin(115200);
  while(!Serial){}

  esp_err_t wdt_err = esp_task_wdt_init(4, true); 
  
  if (wdt_err == ESP_OK) {
      // Suscribimos la tarea actual (el loop principal de Arduino) al Watchdog
      esp_task_wdt_add(NULL); 
      ESP_LOGI("MAIN", "Watchdog inicializado correctamente (4s).");
  } else {
      ESP_LOGE("MAIN", "Fallo al inicializar el Watchdog.");
  }

  RS485.setPins(txPin, dePin, rePin);
 
  int intentosRTU = 0;
  const int maxIntentos = 5;
  bool rtuOk = false;

  while (intentosRTU < maxIntentos) {
      // Alimentamos al Watchdog durante el bucle de inicio para que no se dispare aquí
      esp_task_wdt_reset(); 

      if (ModbusRTUClient.begin(baudrate, (uint32_t)rtuClientConfig)) {
          rtuOk = true;
          ESP_LOGI("MAIN", "ModbusRTUClient inicializado con éxito.");
          break; // Salimos del bucle si arranca bien
      }

      intentosRTU++;
      ESP_LOGW("MAIN", "Error al inicializar ModbusRTUClient. Intento %d/%d...", intentosRTU, maxIntentos);
      delay(1000); // Esperamos 1 segundo antes de volver a intentar
  }

  // Si tras los intentos sigue fallando, aplicamos la acción drástica
  if (!rtuOk) {
      ESP_LOGE("MAIN", "¡HARDWARE CRÍTICO CAÍDO! ModbusRTUClient no responde de forma definitiva.");
      ESP_LOGE("MAIN", "Reiniciando el sistema en 3 segundos...");
      
      delay(3000); // Margen para que el LOG se envíe por Serial antes de morir
      ESP.restart(); // Reinicio forzado del ESP32
  }

  internalSlaveID10.begin();

  modbusTcpBridge.setTCPReqCallback(checkTCPReqCallback);
  modbusTcpBridge.begin(mac, ip, dns, gateway, subnet);

  delay(1000); 
}

void loop() {
    esp_task_wdt_reset();

    modbusTcpBridge.process();   
    vTaskDelay(pdMS_TO_TICKS(5)); 
}

*/