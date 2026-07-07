/*
#include <Arduino.h>
#include "InternalModbusSlave.h"
#include "ModbusInternalClient.h"

// Instanciamos el cliente interno pasando la referencia de la instancia global 'esclavo10'
ModbusInternalClient clienteInterno(&esclavo10);


void setup() {
    Serial.begin(115200);
    while (!Serial) { delay(10); } // Esperar a la consola serial en plataformas nativas USB
    
    Serial.println("--- Iniciando Pruebas de ModbusInternalClient ---");

    // 1. Inicializar el mapa de memoria del esclavo (4 coils, 4 discrete, 1 holding, 4 inputs)
    if (esclavo10.begin()) {
        Serial.println("[OK] Esclavo interno inicializado con éxito.");
    } else {
        Serial.println("[ERROR] No se pudo inicializar la memoria libmodbus.");
        while (1); // Bloquear ejecución si falla
    }

    // Configuración manual del pin DI_4 como INPUT para testear Discrete Inputs físicamente si deseas
    // (Aunque esclavo10.begin ya configura internamente los vectores estáticos correspondientes)
}

void loop() {
    Serial.println("\n=== Ejecutando ciclo de prueba ===");

    // =========================================================================
    // TEST 1: Verificar Lectura de Coils (Salidas Digitales) modificadas por Software
    // =========================================================================
    Serial.println("\n[Test 1] Forzando Coil 0 a TRUE vía Software...");

    esclavo10.writeSingleRegister(0, 512);
    esclavo10.updatePhysicalIO(); 
    int valorLeido = esclavo10.readHoldingRegister(0); 

    Serial.printf("Resultado Test 1 -> Coil 0 leído a través del cliente: %d (Esperado: 1)\n", valorLeido);

    delay(2000);

    Serial.println("\n[Test 2] Forzando Coil 0 a FALSE vía Software...");

    esclavo10.writeSingleRegister(0, 111);
    esclavo10.updatePhysicalIO(); 
    valorLeido = esclavo10.readHoldingRegister(0); 

    Serial.printf("Resultado Test 2 -> Coil 0 leído a través del cliente: %d (Esperado: 0)\n", valorLeido);

    delay(2000);

    Serial.println("\n[Test 3] Forzando Coil 0 a TRUE vía hw...");

    Serial.printf("Resultado Test 4 -> Coil 0 leído a través del cliente: %d (Esperado: 0)\n", valorLeido);


    Serial.println("\n=================================");
    delay(4000); // Esperar 4 segundos antes de repetir el bucle
}
*/


#include <Arduino.h>
#include "ModbusRTUClient.h"
#include "ModbusTCPBridge.h"
#include "FuncInternalClientOLED.h" // La cabecera gestiona el 'extern' de slaves
#include "ModbusRtuLock.h"
#include "ModbusRTUClientManager.h"

byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
IPAddress ip(192, 168, 1, 150); 
uint16_t modbusPort = 502;    

uint32_t _baudrate = 9600; 


// --- INSTANCIAS GLOBALES ÚNICAS (Sin doble constructor) ---
// Inicialmente arranca con el DummyLock interno por defecto
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

void checkTCPReqCallback(const modbusStruct& req, uint16_t index, uint16_t& value) {
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

    // 3. Vincular dinámicamente el Lock y el Interceptor al objeto global estable
    modbusTcpBridge.setThreadLock(&rtuThreadLock); 
    modbusTcpBridge.setInterceptor(checkTCPReqCallback);
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
    rtuThreadLock.lock(); // <--- Cambiado -> por .
    
    int dataType = ModbusTcpBridge::getModbusClientDataType(slave->functionCode);
    
    if(ModbusRTUClient.requestFrom(slave->slaveID, dataType, slave->address, slave->quantity)){
        for (int j = 0; j < slave->quantity; j++) {
            aux_data[j] = ModbusRTUClient.read();  
        }
        lecturaExitosa = true;
    }
    rtuThreadLock.unlock(); // <--- Cambiado -> por .

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
*/
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