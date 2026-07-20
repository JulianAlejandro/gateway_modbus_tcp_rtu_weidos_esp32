



/*
#include <Arduino.h>
#include <E2PROM.h>

#define CONFIG_MAGIC_KEY 0x4D425331 // "MBS1" - Identificador único de validez

// Estructura de configuración alineada
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

// Configuración por defecto a escribir
SystemConfig defaultConfig = {
    .magic = CONFIG_MAGIC_KEY,
    .mac = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED },
    .ip = { 192, 168, 1, 150 },
    .gateway = { 192, 168, 1, 1 },
    .subnet = { 255, 255, 255, 0 },
    .dns = { 192, 168, 1, 1 },
    .modbusPort = 502,
    
    .baudrate = 9600,
    .txPin = RS485_TX,  // Define las constantes de pines si no están cargadas por tu framework
    .dePin = RS485_DE,
    .rePin = RS485_RE,
    .rtuClientConfig = (uint16_t)SERIAL_8N1,
    
    .internal_slave_id = 10
};

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

void setup() {
    Serial.begin(115200);
    while (!Serial) {}
    delay(2000);

    Serial.println("\n[EEPROM WRITER] Inicializando EEPROM...");
    E2PROM.begin();

    // 1. Escribir la estructura completa
    Serial.println("[EEPROM WRITER] Escribiendo configuración en la dirección 0...");
    E2PROM.put(0, defaultConfig);

    // 2. Leer los datos inmediatamente para verificar la grabación
    SystemConfig readData;
    E2PROM.get(0, readData);

    // 3. Comprobar resultado
    if (readData.magic == CONFIG_MAGIC_KEY) {
        Serial.println("\n[ÉXITO] Configuración guardada y verificada correctamente.");
        printConfig(readData);
    } else {
        Serial.println("\n[ERROR] Hubo un problema al escribir o leer la memoria.");
    }
}

void loop() {
    // El trabajo ya se completó en el setup
    delay(5000);
    Serial.println("[EEPROM WRITER] Datos guardados permanentemente. Ya puedes cargar el firmware principal.");
}
*/


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

//Instancias de IP dinamicas
//IPAddress ip;
//IPAddress gateway;    
//IPAddress subnet;  
//IPAddress dns;

// --- INSTANCIAS GLOBALES ÚNICAS (Sin doble constructor) ---
// Inicialmente arranca con el DummyLock interno por defecto
ModbusInternalClient internalClient(&internalSlaveID10); 
ModbusRtuClient mbRtu(&ModbusRTUClient);
ModbusTcpBridge modbusTcpBridge(502, &mbRtu); 

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