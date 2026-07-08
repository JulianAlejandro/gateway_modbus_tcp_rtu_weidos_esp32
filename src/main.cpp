#include <Arduino.h>

#include "FuncInternalClientOLED.h" // La cabecera gestiona el 'extern' de slaves

#include "ModbusRtuLock.h"
#include "ModbusRTUClient.h"
#include "ModbusRTUClientManager.h"

#include "ModbusInternalClient.h"
#include "InternalModbusSlave.h"

#include "ModbusTCPBridge.h"

byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
IPAddress ip(192, 168, 1, 150); 
uint16_t modbusPort = 502;    

uint32_t _baudrate = 9600; 

// --- INSTANCIAS GLOBALES ÚNICAS (Sin doble constructor) ---
// Inicialmente arranca con el DummyLock interno por defecto
ModbusInternalClient internalClient(&internalSlaveID10); 
ModbusRtuClientManager mbRtuManager(&ModbusRTUClient);
ModbusTcpBridge modbusTcpBridge(modbusPort, &mbRtuManager); 

TaskHandle_t ModbusGatewayTaskHandle = NULL;
SemaphoreHandle_t xModbusDataMutex = NULL;  
SemaphoreHandle_t xModbusRTUMutex = NULL; 

// Puntero para usar el wrapper del Lock tanto en el main como en el puente
ModbusRtuLock rtuThreadLock;

// --- ARRAY CON CONFIGURACIÓN DE ATRIBUTOS ---
ModbusSlaveData slaves[] = {
    { "CL2",  "ppm",  3,        1,      0,      2,       0x03,       {0, 0},      0.0,         false,        0,         false,    0},
    { "COND", "us",   1,        2,      0,      2,       0x03,       {0, 0},      0.0,         false,        0,         false,    0},
    { "REDOX","mV",   1,        3,      0,      2,       0x03,       {0, 0},      0.0,         false,        0,         false,    0},
    { "TURB", "NTU",  3,        4,      0,      2,       0x03,       {0, 0},      0.0,         false,        0,         false,    0},
    { "PH",   "pH",   2,        5,      0,      2,       0x03,       {0, 0},      0.0,         false,        0,         false,    0}
};

const uint8_t NUM_SLAVES = sizeof(slaves) / sizeof(slaves[0]);

void checkSlaveFlagsAndTimeouts();
void updateSlave(ModbusSlaveData* slave);
bool reqSlaveInternalClient(ModbusSlaveData* slave); 

void checkTCPReqCallback(const modbusStruct& req) { // todo, quiza modificar este callback para que reciba directametne el puntero, y no necesariamente llamar a la funcion. 
    if(req.slaveID == 10){
        modbusTcpBridge.setModbusClient(&internalClient);
    }else{
        modbusTcpBridge.setModbusClient(&mbRtuManager); 
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

    // 1. Crear Semáforos primero
    xModbusDataMutex = xSemaphoreCreateMutex(); 
    xModbusRTUMutex = xSemaphoreCreateMutex(); 

    if(xModbusDataMutex == NULL || xModbusRTUMutex == NULL) while(1);

    rtuThreadLock.init(xModbusRTUMutex); 

    // 2. Instanciar el Lock pasándole el Semáforo de FreeRTOS real
    //rtuThreadLock = new FreeRtosModbusLock(xModbusRTUMutex); // TODO , no me gusta en memoria dinamica

    RS485.setPins(RS485_TX, RS485_DE, RS485_RE);
    ModbusRTUClient.begin(_baudrate, (uint16_t)SERIAL_8N1);

    internalSlaveID10.begin(); // inicializamos el mapa, quiza esto deberia ir en otro sitio. 
   

    // 3. Vincular dinámicamente el Lock y el Interceptor al objeto global estable
    modbusTcpBridge.setThreadLock(&rtuThreadLock); 
    modbusTcpBridge.setInterceptor(checkTCPDataCallback);
    modbusTcpBridge.setTCPReqCallback(checkTCPReqCallback);
    modbusTcpBridge.begin(mac, ip);

    xTaskCreatePinnedToCore(modbusGatewayTask, "ModbusGatewayTask", 4096, NULL, 3, &ModbusGatewayTaskHandle, 0);

    initOLED(); 

    for(int i = 0; i < NUM_SLAVES; i++ ){ 
        reqSlaveInternalClient(&slaves[i]); 
    }

    delay(1000); 
}

void loop() {
    checkSlaveFlagsAndTimeouts();
    updateOLED(); 
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

//----------------------------------PRUEBAS SOBRE EL GATEWAY ORIGINAL------------------------------

/*
#include <Arduino.h>
#include "ModbusInternalClient.h"
#include "InternalModbusSlave.h"
#include "ModbusTCPBridge.h"

//#define BAUDRATE 9600

byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
IPAddress ip(192, 168, 1, 150); 
uint16_t modbusPort = 502;

uint32_t _baudrate = 9600; 

//ModbusRTUClientManager slaveRtu(BAUDRATE);
ModbusInternalClient internalClient(&internalSlaveID10); 
ModbusTcpBridge tcpBridge(modbusPort, &internalClient); // es un modbus TCP bridge con multihilo y callbacks. 

TaskHandle_t ModbusGatewayTaskHandle = NULL;

void checkTCPReqCallback(const modbusStruct& req) { // todo, quiza modificar este callback para que reciba directametne el puntero, y no necesariamente llamar a la funcion. 
    if(req.slaveID == 10){
        tcpBridge.setModbusClient(&internalClient);
    }else{
        tcpBridge.setModbusClient(nullptr); 
    }
}

void modbusGatewayTask(void * pvParameters) {
    for(;;) {
        tcpBridge.process();
        //esclavo10.updatePhysicalIO(); 
        vTaskDelay(pdMS_TO_TICKS(5)); 
    }
}

void setup() {
  Serial.begin(115200);
  while(!Serial){}

  // configuracion inicial de RTU , RS485 , baudrate y 
  //RS485.setPins(RS485_TX, RS485_DE, RS485_RE);
  //ModbusRTUClient.begin(_baudrate, (uint16_t)SERIAL_8N1);

  // inicializacion de Tcp bridge con mac e ip 
  if (internalSlaveID10.begin()) {
      ESP_LOGI("MAIN", "Esclavo interno Modbus inicializado correctamente.");
  } else {
      ESP_LOGE("MAIN", "Error crítico al inicializar el esclavo interno.");
  }

  tcpBridge.setTCPReqCallback(checkTCPReqCallback); 
  tcpBridge.begin(mac, ip);

  xTaskCreatePinnedToCore(modbusGatewayTask, "ModbusGatewayTask", 4096, NULL, 3, &ModbusGatewayTaskHandle, 0);

  delay(1000); 
}

void loop() {
  delay(100); 
}
*/
