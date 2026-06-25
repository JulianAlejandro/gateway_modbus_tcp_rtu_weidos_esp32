#include <Arduino.h>
#include "ModbusRTUModule.h"
#include "ModbusTCPModule.h"
#include "FuncInternalClientOLED.h" // La cabecera gestiona el 'extern' de slaves

#define BAUDRATE 19200

byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
IPAddress ip(192, 168, 1, 150); 
uint16_t modbusPort = 502;    

ModbusRTUModule slaveRtuModule(BAUDRATE);
ModbusTCPModule MasterTcpModule(modbusPort, &slaveRtuModule);

TaskHandle_t ModbusGatewayTaskHandle = NULL;
SemaphoreHandle_t xModbusDataMutex = NULL;  
SemaphoreHandle_t xModbusRTUMutex = NULL; 


// --- ARRAY CON CONFIGURACIÓN DE ATRIBUTOS ---
ModbusSlaveData slaves[] = {
  //                          constants                             |                         control variables 
  // name     unit  decimals  salveId adress  quantity functionCode | dataRaw  convertedData  flagUpdate  lastUpload   disable  isNew  
    { "CL2",  "ppm",  3,        1,      0,      2,       0x03,       {0, 0},      0.0,         false,        0,        false,  false},
    { "COND", "us",   1,        2,      0,      2,       0x03,       {0, 0},      0.0,         false,        0,        false,  false},
    { "REDOX","mV",   1,        3,      0,      2,       0x03,       {0, 0},      0.0,         false,        0,        false,  false},
    { "TURB", "NTU",  3,        4,      0,      2,       0x03,       {0, 0},      0.0,         false,        0,        false,  false},
    { "PH",   "pH",   2,        5,      0,      2,       0x03,       {0, 0},      0.0,         false,        0,        false,  false}
};


const uint8_t NUM_SLAVES = sizeof(slaves) / sizeof(slaves[0]);

void checkSlaveFlagsAndTimeouts();
void updateSlave(ModbusSlaveData* slave);
void reqSlaveInternalClient(ModbusSlaveData* slave);  

void checkTCPReqCallback(const modbusTCPStruct& req, uint16_t index, uint16_t value) {
    for (uint8_t i = 0; i < NUM_SLAVES; i++) {
        if (req.slaveID == slaves[i].slaveID && req.address == slaves[i].address && req.quantity == slaves[i].quantity && req.functionCode == slaves[i].functionCode) {
            if (index < slaves[i].quantity) {
                // Escritura directa rápida sin Mutex para proteger el rendimiento del Core 0
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
        MasterTcpModule.process();
        vTaskDelay(pdMS_TO_TICKS(1)); 
    }
}

void setup() {
  Serial.begin(115200);
  while(!Serial){}

  xModbusDataMutex = xSemaphoreCreateMutex(); 
  xModbusRTUMutex = xSemaphoreCreateMutex(); 

  if(xModbusDataMutex == NULL || xModbusRTUMutex == NULL) while(1); 

  slaveRtuModule.begin();
  MasterTcpModule.setHardwareMutex(xModbusRTUMutex);
  MasterTcpModule.setInterceptor(checkTCPReqCallback); 
  MasterTcpModule.begin(mac, ip);
/*
  unsigned long currentMillis = millis();
  for(uint8_t i = 0; i < NUM_SLAVES; i++){
      slaves[i].lastUpload = currentMillis;
  }
*/
  xTaskCreatePinnedToCore(modbusGatewayTask, "ModbusGatewayTask", 4096, NULL, 3, &ModbusGatewayTaskHandle, 0);

  initOLED(); // Inicialización modular

  for(int i = 0; i < NUM_SLAVES; i++ ){ // iniciamos teniendo algun dato de los esclavos que queremos mostrar por pantalla
    reqSlaveInternalClient(&slaves[i]); 
  }

  delay(1000); 
}

void loop() {
  checkSlaveFlagsAndTimeouts();
  updateOLED(); // Llamada no bloqueante que refresca el display por fragmentos
}

void checkSlaveFlagsAndTimeouts() {
  unsigned long now = millis(); 

  for (uint8_t i = 0; i < NUM_SLAVES; i++) {
    
    // 1. PROCESADO DE FLAG DESDE EL INTERCEPTOR TCP
    if (slaves[i].flagUpdate) {
      updateSlave(&slaves[i]); 
    }

    // 2. LECTURA FORZADA SI EL ESCLAVO NO REPORTA EN 30s
    if (!slaves[i].disable && (now - slaves[i].lastUpload > INTERNAL_POLL_THRESHOLD)) { // SI ESTA DISABLE NO ENTRA? MAL
      reqSlaveInternalClient(&slaves[i]);
    }
  }
}

void updateSlave(ModbusSlaveData* slave){
  //updateSlave(ModbusSlaveData* slave); 
  if (xSemaphoreTake(xModbusDataMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
    if (slave->flagUpdate) {

      int32_t combinado = ((uint32_t)slave->rawBuffer[0] << 16) | slave->rawBuffer[1];
      slave->rawBuffer[0] = 0; // inicilizado de nuevo. por si acaso, listo para ser sobrescrito
      slave->rawBuffer[1] = 0; 
      slave->convertedData = combinado / 1000.0;
      slave->lastUpload = millis(); 
              
      slave->flagUpdate = false; // se inicializa para que no este entrando en este bucle eternamente. 
      slave->isNew = true; // Activa el aviso de "flecha" en pantalla temporalmente
      slave->disable = false; // Si responde por TCP, lógicamente está ONLINE
    }
    xSemaphoreGive(xModbusDataMutex);
  }
}


void reqSlaveInternalClient(ModbusSlaveData* slave){ // todo quitar el now

  // se construye una request
  modbusTCPStruct req; 
  req.slaveID = slave->slaveID; 
  req.address = slave->address; 
  req.functionCode = slave->functionCode; 
  req.quantity = slave->quantity; 

  uint16_t aux_data[2] = {0, 0};
  bool lecturaExitosa = false;

  // se usa el modbus RTU para obtener el dato 
  if (xSemaphoreTake(xModbusRTUMutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
    if (slaveRtuModule.readFromSlave(req)) {
      for (int j = 0; j < req.quantity; j++) {
          aux_data[j] = slaveRtuModule.readRegister(); 
      }
      lecturaExitosa = true;
    }
    xSemaphoreGive(xModbusRTUMutex);
  }

  if (lecturaExitosa) {
    if (xSemaphoreTake(xModbusDataMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
          
      for (int i = 0; i < req.quantity; i++) {
          slave->rawBuffer[i] = aux_data[i]; 
      }
      slave->flagUpdate = true;
      
      xSemaphoreGive(xModbusDataMutex);
    }
  } 
}