/*
#include <Arduino.h>
#include "ModbusRTUClientManager.h"
#include "EmasesaModbusTCPBridge.h"
#include "FuncInternalClientOLED.h" // La cabecera gestiona el 'extern' de slaves

#define BAUDRATE 9600

byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
IPAddress ip(192, 168, 1, 150); 
uint16_t modbusPort = 502;    

ModbusRTUClientManager slaveRtu(BAUDRATE);
EmasesaModbusTcpBridge emasesaTcpBridge(modbusPort, &slaveRtu); // es un modbus TCP bridge con multihilo y callbacks. 

TaskHandle_t ModbusGatewayTaskHandle = NULL;
SemaphoreHandle_t xModbusDataMutex = NULL;  
SemaphoreHandle_t xModbusRTUMutex = NULL; 


// --- ARRAY CON CONFIGURACIÓN DE ATRIBUTOS ---
ModbusSlaveData slaves[] = {
  //                          constants                             |                         control variables 
  //   name   unit  decimals  salveId adress  quantity functionCode | dataRaw  convertedData  flagUpdate  lastTimeRef   isNew  errCounter 
    { "CL2",  "ppm",  3,        1,      0,      2,       0x03,       {0, 0},      0.0,         false,        0,         false,    0},
    { "COND", "us",   1,        2,      0,      2,       0x03,       {0, 0},      0.0,         false,        0,         false,    0},
    { "REDOX","mV",   1,        3,      0,      2,       0x03,       {0, 0},      0.0,         false,        0,         false,    0},
    { "TURB", "NTU",  3,        4,      0,      2,       0x03,       {0, 0},      0.0,         false,        0,         false,    0},
    { "PH",   "pH",   2,        5,      0,      2,       0x03,       {0, 0},      0.0,         false,        0,         false,    0}
};


const uint8_t NUM_SLAVES = sizeof(slaves) / sizeof(slaves[0]);
//unsigned long lastTimeReference = 0; 

void checkSlaveFlagsAndTimeouts();
void updateSlave(ModbusSlaveData* slave);
bool reqSlaveInternalClient(ModbusSlaveData* slave);  

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
        emasesaTcpBridge.process();
        vTaskDelay(pdMS_TO_TICKS(1)); 
    }
}

void setup() {
  Serial.begin(115200);
  while(!Serial){}

  xModbusDataMutex = xSemaphoreCreateMutex(); 
  xModbusRTUMutex = xSemaphoreCreateMutex(); 

  if(xModbusDataMutex == NULL || xModbusRTUMutex == NULL) while(1); 

  slaveRtu.begin();
  emasesaTcpBridge.setHardwareMutex(xModbusRTUMutex);
  emasesaTcpBridge.setInterceptor(checkTCPReqCallback); 
  emasesaTcpBridge.begin(mac, ip);

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

    // 2. LECTURA FORZADA SI EL ESCLAVO NO REPORTA EN 30s y cada 30 segundos
    if (now - slaves[i].lastTimeReference > INTERNAL_POLL_THRESHOLD) {  // ESTO ESTA MAL PORQUE SI 
      //lastTimeReference = now;   
      if(!reqSlaveInternalClient(&slaves[i])){
        slaves[i].errCounter++; 
        slaves[i].lastTimeReference = now; 
        if(slaves[i].errCounter >= 5){ // en 30 * 5 = en 2 minutos y medio no hay respuestas...disable.
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
      slave->rawBuffer[0] = 0; // inicilizado de nuevo. por si acaso, listo para ser sobrescrito
      slave->rawBuffer[1] = 0; 
      slave->convertedData = combinado / 1000.0;
      slave->lastTimeReference = millis(); 
              
      slave->flagUpdate = false; // se inicializa para que no este entrando en este bucle eternamente. 
      slave->isNew = true; // Activa el aviso de "flecha" en pantalla temporalmente
      //slave->disable = false; // Si responde por TCP, lógicamente está ONLINE
      slave->errCounter = 0; 
    }
    xSemaphoreGive(xModbusDataMutex);
  }
}

bool reqSlaveInternalClient(ModbusSlaveData* slave){ // todo quitar el now

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
    if (slaveRtu.readFromSlave(req)) {
      for (int j = 0; j < req.quantity; j++) {
          aux_data[j] = slaveRtu.readRegister(); q
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
  return lecturaExitosa; 
}
*/
//----------------------------------PRUEBAS SOBRE EL GATEWAY ORIGINAL------------------------------


#include <Arduino.h>
//#include "ModbusRTUClientManager.h"
#include "ModbusRTUClient.h"
#include "ModbusTCPBridge.h"

//#define BAUDRATE 9600

byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
IPAddress ip(192, 168, 1, 150); 
uint16_t modbusPort = 502;

uint32_t _baudrate = 9600; 

//ModbusRTUClientManager slaveRtu(BAUDRATE);
ModbusTcpBridge tcpBridge(modbusPort, &ModbusRTUClient); // es un modbus TCP bridge con multihilo y callbacks. 

TaskHandle_t ModbusGatewayTaskHandle = NULL;

void modbusGatewayTask(void * pvParameters) {
    for(;;) {
        tcpBridge.process();
        vTaskDelay(pdMS_TO_TICKS(5)); 
    }
}

void setup() {
  Serial.begin(115200);
  while(!Serial){}

  // configuracion inicial de RTU , RS485 , baudrate y 
  RS485.setPins(RS485_TX, RS485_DE, RS485_RE);
  ModbusRTUClient.begin(_baudrate, (uint16_t)SERIAL_8N1);

  // inicializacion de Tcp bridge con mac e ip 
  tcpBridge.begin(mac, ip);

  xTaskCreatePinnedToCore(modbusGatewayTask, "ModbusGatewayTask", 4096, NULL, 3, &ModbusGatewayTaskHandle, 0);

  delay(1000); 
}

void loop() {
  delay(100); 
}



/*
//----------------codigo para hacer pruebas modbus independiente de gateway---------

#include <Arduino.h>
#include <ArduinoRS485.h>  // Asegúrate de que esté incluida para los pines
#include <ModbusRTUClient.h>

uint32_t _baudrate = 9600; 
const int TARGET_SLAVE = 6;
const int COIL_ADDRESS = 0;

void setup() {
  Serial.begin(115200);
  while(!Serial){}

  Serial.println("--- MODBUS RTU CLIENT: TEST DE ESCRITURA SINGLE COIL (FC05) ---");

  // Configuración de pines de tu hardware RS485
  RS485.setPins(RS485_TX, RS485_DE, RS485_RE);
  
  // Inicializa el cliente Modbus RTU nativo
  if (!ModbusRTUClient.begin(_baudrate, (uint16_t)SERIAL_8N1)) {
    Serial.println("Error al inicializar el cliente Modbus RTU");
    while(1);
  }
  
  Serial.println("Cliente Modbus RTU iniciado correctamente.");
  delay(1000); 
}

void loop() {
  static bool coilState = true; // Estado que iremos alternando

  Serial.print("Intentando escribir en Esclavo ");
  Serial.print(TARGET_SLAVE);
  Serial.print(", Coil ");
  Serial.print(COIL_ADDRESS);
  Serial.print(" -> Valor: ");
  Serial.println(coilState ? "ENCENDIDO (1)" : "APAGADO (0)");

  // Ejecutamos la función coilWrite nativa (envía el FC 0x05)
  // El método de la librería espera: (id_esclavo, direccion, valor_0_o_1)
  int result = ModbusRTUClient.coilWrite(TARGET_SLAVE, COIL_ADDRESS, coilState ? 1 : 0);

  if (result == 1) {
    Serial.println("[ÉXITO] El esclavo respondió correctamente con el ECO.");
  } else {
    Serial.print("[FALLO] No se pudo escribir. Razón: ");
    // Si falla, leemos el errno formateado por la librería
    const char* errorMsg = ModbusRTUClient.lastError();
    if (errorMsg != NULL) {
      Serial.println(errorMsg);
    } else {
      Serial.println("Error desconocido o Timeout");
    }
  }

  Serial.println("----------------------------------------------");

  // Alternamos el estado para la siguiente iteración
  coilState = !coilState;

  // Esperamos 3 segundos antes del siguiente intento
  delay(15000);
}
*/