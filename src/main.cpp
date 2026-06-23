#include <Arduino.h>
#include "ModbusRTUModule.h"
#include "ModbusTCPModule.h"
#include "FuncInternalClientOLED.h" // La cabecera gestiona el 'extern' de slaves

#define BAUDRATE 9600

byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
IPAddress ip(192, 168, 1, 150); 
uint16_t modbusPort = 502;    

ModbusRTUModule slaveRtuModule(BAUDRATE);
ModbusTCPModule MasterTcpModule(modbusPort, &slaveRtuModule);

TaskHandle_t ModbusGatewayTaskHandle = NULL;
SemaphoreHandle_t xModbusDataMutex = NULL;  
SemaphoreHandle_t xModbusRTUMutex = NULL; 

const unsigned long INTERNAL_POLL_THRESHOLD = 30000; 

// --- ARRAY CON CONFIGURACIÓN DE ATRIBUTOS ---
ModbusSlaveData slaves[] = {
  // nombre  unidades  adress  ID dataRaw  
    { "CL2",  "ppm",  64512,   1, {0, 0}, false, 0.0, 0, false, false },
    { "COND", "C",    64512,   2, {0, 0}, false, 0.0, 0, false, false },
    { "REDOX","HR%",  64512,   3, {0, 0}, false, 0.0, 0, false, false },
    { "TURB", "Bar",  64512,   4, {0, 0}, false, 0.0, 0, false, false },
    { "PH",   "V",    64512,   5, {0, 0}, false, 0.0, 0, false, false }
};
const uint8_t NUM_SLAVES = sizeof(slaves) / sizeof(slaves[0]);

void checkSlaveFlagsAndTimeouts(); 
void loadSlaveValue(uint8_t slaveIndex, uint16_t slaveDataRawBuffer[2]); 

void checkTCPReqCallback(const modbusTCPStruct& req, uint16_t index, uint16_t value) {
    for (uint8_t i = 0; i < NUM_SLAVES; i++) {
        if (req.slaveID == slaves[i].slaveID && req.address == slaves[i].address && req.quantity == 2 && req.functionCode == 0x03) {
            if (index < 2) {
                // Escritura directa rápida sin Mutex para proteger el rendimiento del Core 0
                slaves[i].rawBuffer[index] = value; 
                if (index == 1) { 
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

  unsigned long currentMillis = millis();
  for(uint8_t i = 0; i < NUM_SLAVES; i++){
      slaves[i].lastUpload = currentMillis;
  }

  xTaskCreatePinnedToCore(modbusGatewayTask, "ModbusGatewayTask", 4096, NULL, 3, &ModbusGatewayTaskHandle, 0);

  initOLED(); // Inicialización modular
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
      if (xSemaphoreTake(xModbusDataMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        if (slaves[i].flagUpdate) {
          loadSlaveValue(i, slaves[i].rawBuffer); 
          slaves[i].flagUpdate = false; 
          slaves[i].isNew = true; // Activa el aviso de "flecha" en pantalla temporalmente
          slaves[i].disable = false; // Si responde por TCP, lógicamente está ONLINE
        }
        xSemaphoreGive(xModbusDataMutex);
      }
    }

    // 2. LECTURA FORZADA SI EL ESCLAVO NO REPORTA EN 30s
    if (!slaves[i].disable && (now - slaves[i].lastUpload > INTERNAL_POLL_THRESHOLD)) {
      modbusTCPStruct req; 
      req.slaveID = slaves[i].slaveID; 
      req.address = slaves[i].address; 
      req.functionCode = 0x03; 
      req.quantity = 2; 

      uint16_t aux_data[2] = {0, 0};
      bool lecturaExitosa = false;

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
          loadSlaveValue(i, aux_data);
          slaves[i].isNew = true; 
          slaves[i].disable = false;
          xSemaphoreGive(xModbusDataMutex);
        }
      } else {
        // Marcamos como offline para que el OLED muestre "OFFLINE"
        if (xSemaphoreTake(xModbusDataMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
          slaves[i].disable = true; 
          xSemaphoreGive(xModbusDataMutex);
        }
        slaves[i].lastUpload = now; // Evitamos reintentos en bucle agresivo
      }
    }
  }
}

void loadSlaveValue(uint8_t slaveIndex, uint16_t slaveDataRawBuffer[2]) {
  int32_t combinado = ((uint32_t)slaveDataRawBuffer[0] << 16) | slaveDataRawBuffer[1];
  slaves[slaveIndex].convertedData = combinado / 1000.0;
  slaves[slaveIndex].lastUpload = millis(); 
}