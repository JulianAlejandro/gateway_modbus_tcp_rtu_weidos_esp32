#include <Arduino.h>
#include "ModbusRTUModule.h"
#include "ModbusTCPModule.h"
#include "FuncInternalClientOLED.h"

#define BAUDRATE 9600

byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
IPAddress ip(192, 168, 1, 150); 
uint16_t modbusPort = 502;    

ModbusRTUModule slaveRtuModule(BAUDRATE);
ModbusTCPModule MasterTcpModule(modbusPort, &slaveRtuModule);

// Definición del Handler de la Tarea
TaskHandle_t ModbusGatewayTaskHandle = NULL;
SemaphoreHandle_t xModbusDataMutex = NULL;  // Semaforo para las variables globales compartidas. 
SemaphoreHandle_t xModbusRTUMutex = NULL; 

uint16_t slaveDataRawBuffer[2];
volatile bool flagUpdateSlaveData = false;  // variable volatile ya que cambia desde dos hilos distintos

float slaveData = 0.0;
unsigned long slaveUploadReference = 0; 

const unsigned long INTERNAL_POLL_THRESHOLD = 30000; // 30 Segundos 

void checkSlaveFlag(); 
void loadSlaveValue(uint16_t slaveDataRawBuffer[2]); 

unsigned long lastOledUpload = 0; 

// esta funcion callback es ejecutada por el gateway, nos permite capturar el dato. 
void checkTCPReqCallback(const modbusTCPStruct& req, uint16_t index, uint16_t value) {
    if (req.slaveID == 1 && req.address == 64512 && req.quantity == 2 && req.functionCode == 0x03) {
        if(index < 2){
            if(xSemaphoreTake(xModbusDataMutex, 0) == pdTRUE){
            slaveDataRawBuffer[index] = value; 
                if(index == 1){ 
                    flagUpdateSlaveData = true; 
                }

                xSemaphoreGive(xModbusDataMutex); 
            }
        }        
    }
}

// Función que ejecutará el hilo dedicado al Gateway
void modbusGatewayTask(void * pvParameters) {
    Serial.println("Hilo Modbus Gateway iniciado...");
    
    // El bucle infinito del hilo
    for(;;) {
        MasterTcpModule.process();
        
        // Liberar tiempo de CPU para que el IDLE task alimente al Watchdog del Core 0
        vTaskDelay(pdMS_TO_TICKS(1)); //TODO analizar si bloquea mucho la ejecucion
    }
}


void setup() {
  Serial.begin(115200);
  while(!Serial){}
  Serial.println("\n--- PASARELA MODBUS MULTIHILO ---");

  xModbusDataMutex = xSemaphoreCreateMutex(); 
  xModbusRTUMutex = xSemaphoreCreateMutex(); 

  if(xModbusDataMutex == NULL || xModbusRTUMutex == NULL){
    Serial.println("Error al crear el Mutex"); 
    while(1); 
  }

  if(!slaveRtuModule.begin()){   
    Serial.println("Error RTU");    
  } else {
    Serial.println("RTU listo.");
  }

  MasterTcpModule.setHardwareMutex(xModbusRTUMutex);
  MasterTcpModule.setInterceptor(checkTCPReqCallback); 
  MasterTcpModule.begin(mac, ip);
  Serial.println("TCP listo."); 

  slaveUploadReference = millis();

  // Creación del hilo (Tarea de FreeRTOS)
  xTaskCreatePinnedToCore(
    modbusGatewayTask,        // Función de la tarea
    "ModbusGatewayTask",      // Nombre descriptivo
    4096,                     // Tamaño de la pila (Stack size en bytes/words)
    NULL,                     // Parámetros de entrada
    1,                        // Prioridad de la tarea (1 es baja/media), TODO quiza aumentar la prioridad. 
    &ModbusGatewayTaskHandle, // Handler de la tarea
    0                         // Core donde se ejecutará (Core 0 para dejar el Core 1 al loop)
  );

  initOLED();
  delay(3000); 
}

void loop() {

  checkSlaveFlag();
  //delay(10); // como es un hilo que no esta constantemente ejecutandose se puede hacer un delay
}

void checkSlaveFlag(){

  unsigned long now = millis(); 

  if(flagUpdateSlaveData){

    // intentamos tomar el Mutex. Aqui podemos esperar hasta 10ms si el core 0 esta escribiendo 
    if(xSemaphoreTake(xModbusDataMutex, pdMS_TO_TICKS(10)) == pdTRUE){

      if(flagUpdateSlaveData){

        loadSlaveValue(slaveDataRawBuffer); 
        flagUpdateSlaveData = false; 

      }
      
      xSemaphoreGive(xModbusDataMutex);

      Serial.println("Aqui ha entrado y el valor es: ");
      Serial.println(slaveData); 
    }
  }

// 2. LECTURA DE RESPALDO SI PASAN 30 SEGUNDOS (Filtro de tiempo corregido)
  if (now - slaveUploadReference > INTERNAL_POLL_THRESHOLD) {
    Serial.println("[Timeout 30s] Forzando lectura directa del esclavo RTU...");

    modbusTCPStruct req; 
    req.slaveID = 1; 
    req.address = 64512; // Modificado a 64512 para que coincida con tu registro
    req.functionCode = 0x03; 
    req.quantity = 2; 

    uint16_t aux_data[2] = {0, 0};
    bool lecturaExitosa = false;

    // Tomamos el control del HARDWARE RS485. Si el Core 0 está usando el bus, 
    // esperamos hasta 1000ms (lo que tarda un timeout estándar de Modbus)
    if (xSemaphoreTake(xModbusRTUMutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
      
      if (slaveRtuModule.readFromSlave(req)) {
        for (int i = 0; i < req.quantity; i++) {
            aux_data[i] = slaveRtuModule.readRegister(); 
        }
        lecturaExitosa = true;
      }
      
      // Soltamos inmediatamente el hardware para que el Gateway pueda seguir operando
      xSemaphoreGive(xModbusRTUMutex);
    }

    if (lecturaExitosa) {
      loadSlaveValue(aux_data);
      Serial.print("[Interno] Dato forzado con éxito: ");
      Serial.println(slaveData);
    } else {
      Serial.println("[Error] No se pudo forzar la lectura. Esclavo offline.");
      // Reseteamos el tiempo para no saturar el bus reintentando agresivamente en cada vuelta del loop
      //slaveUploadReference = millis(); 
    }
  }

  if (now - lastOledUpload > 5000){ // cada 5 seg
    lastOledUpload = now;
    
    float dataToDisplay = 0.0; 
    bool dataReady = false; 

    if (xSemaphoreTake(xModbusDataMutex, pdMS_TO_TICKS(5)) == pdTRUE) {
      dataToDisplay = slaveData;
      dataReady = true;
      xSemaphoreGive(xModbusDataMutex);
    }

    if (dataReady) {
      drawOLEAD("slave", dataToDisplay, "ppm");
    }
    
  }

}

void loadSlaveValue(uint16_t slaveDataRawBuffer[2]){

  int32_t combinado = ((uint32_t)slaveDataRawBuffer[0] << 16) | slaveDataRawBuffer[1];

  slaveData = combinado / 1000.0;
  slaveUploadReference = millis(); 

}