
/*
#include <Arduino.h>
#include <SPI.h>       
#include <Ethernet.h>  
#include <ModbusRTU.h> // Usamos solo la parte RTU de la librería

// 1. CONFIGURACIÓN DE RED (Tuya probada)
byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
IPAddress ip(192, 168, 1, 150); 
uint16_t modbusPort = 502;      

// 2. CONFIGURACIÓN DE HARDWARE (UART para RS485 de tu Weidmüller)
#define RX_PIN 16     
#define TX_PIN 17     
#define RTS_PIN 4     // Pin DE/RE de control de flujo

// 3. INSTANCIAS DE HARDWARE Y MODBUS
class WeidosModbusServer : public EthernetServer {
public:
    WeidosModbusServer(uint16_t port) : EthernetServer(port) {}
    virtual void begin(uint16_t port) override {
        EthernetServer::begin(); 
    }
};

WeidosModbusServer modbusServer(modbusPort);
ModbusRTU rtu; // Única instancia de Modbus encargada de la línea serie

// Variables para coordinar la respuesta asíncrona del RS485
EthernetClient clienteActivo;
bool esperandoRespuestaRTU = false;

// Callback cuando el esclavo físico RS485 responde por el cable
Modbus::ResultCode cbRtuRaw(uint8_t* data, uint8_t len, void* custom) {
  auto src = (Modbus::frame_arg_t*) custom;
  
  // Si tenemos un cliente SCADA conectado por Ethernet esperando, le enviamos los datos
  if (clienteActivo && clienteActivo.connected()) {
    Serial.printf("[RTU] Esclavo %d respondió (%d bytes). Devolviendo al SCADA...\n", src->slaveId, len);
    
    // Modbus TCP requiere una cabecera MBAP antes de los datos RTU.
    // Como simplificación directa, devolvemos los datos crudos en formato RTU si tu SCADA lo acepta, 
    // o reconstruimos el MBAP básico:
    // Para esta prueba rápida, le inyectamos la respuesta serie directamente al socket TCP
    clienteActivo.write(data, len);
  }
  
  esperandoRespuestaRTU = false;
  return Modbus::EX_PASSTHROUGH;
}

void setup() {
  Serial.begin(115200);
  while (!Serial) { ; } 
  Serial.println("\n--- GATEWAY HÍBRIDO SEGURO ETHERNET TO RTU ---");

  // 4. INICIALIZAR ETHERNET (Tu método probado sin Crasheos)
  Ethernet.init(ETHERNET_CS);
  Ethernet.begin(mac, ip);
  modbusServer.begin(modbusPort);

  Serial.print("Servidor listo en IP: ");
  Serial.print(Ethernet.localIP());
  Serial.print(" | Puerto: ");
  Serial.println(modbusPort);

  // 5. INICIALIZAR MODBUS RTU FISICO
  Serial2.begin(9600, SERIAL_8N1, RX_PIN, TX_PIN);
  rtu.begin(&Serial2, RTS_PIN);  
  rtu.master(); 
  rtu.onRaw(cbRtuRaw); // Enlazamos la captura de respuestas del bus serie
  
  Serial.println("Esperando peticiones del SCADA...");
}

void loop() {
  // Mantener viva la tarea serie para procesar las respuestas del cable RS485
  rtu.task();

  // Escuchar si llega algún cliente TCP (SCADA)
  if (!esperandoRespuestaRTU) {
    EthernetClient client = modbusServer.available();

    if (client) {
      clienteActivo = client;
      byte tramaBuffer[260]; 
      int index = 0;

      // Leer la trama Modbus TCP que viene de la red
      while (client.connected() && client.available() && index < sizeof(tramaBuffer)) {
        tramaBuffer[index++] = client.read();
      }

      // Si recibimos una trama Modbus TCP válida (mínimo 7 bytes de cabecera MBAP)
      if (index >= 7) {
        // En Modbus TCP: el byte 6 es el Unit ID (ID del esclavo)
        uint8_t unitId = tramaBuffer[6];
        
        // Los datos útiles (PDU) de Modbus empiezan en el byte 7 (Código de función)
        // Calculamos cuántos bytes de PDU hay realmente
        int pduLength = index - 7;

        Serial.printf("\n[SCADA] Petición Ethernet para RTU ID: %d | PDU Len: %d\n", unitId, pduLength);

        // Activamos el flag para bloquear nuevas lecturas hasta que el RS485 conteste
        esperandoRespuestaRTU = true;

        // Enviamos la petición de forma idéntica al bus físico usando los datos recortados
        rtu.rawRequest(unitId, &tramaBuffer[7], pduLength);
        
        // Damos un pequeño margen de tiempo para que se procese en el loop
        uint32_t timeout = millis();
        while (esperandoRespuestaRTU && (millis() - timeout < 1500)) {
          rtu.task();
          delay(1);
        }
        
        if (esperandoRespuestaRTU) {
          Serial.printf("[TIMEOUT] El dispositivo RTU ID %d no respondió.\n", unitId);
          esperandoRespuestaRTU = false;
        }
      }
      
      // Liberamos el socket del cliente para la próxima consulta
      client.stop();
    }
  }
  
  yield();
}
*/

/*
  ModbusRTU ESP8266/ESP32
  Read multiple coils from slave device example

  (c)2019 Alexander Emelianov (a.m.emelianov@gmail.com)
  https://github.com/emelianov/modbus-esp8266

  modified 13 May 2020
  by brainelectronics

  This code is licensed under the BSD New License. See LICENSE.txt for more info.
*/
/*

#include "ModbusRTU.h"

ModbusRTU mb;

uint16_t holdingRegs[20];

bool cbRead(Modbus::ResultCode event, uint16_t transactionId, void* data) {
  if (event == Modbus::EX_SUCCESS) {
    Serial.println("--- Lectura Exitosa ---");
    Serial.printf("Registro 64512 (Alto): %d\n", holdingRegs[0]);
    Serial.printf("Registro 64513 (Bajo): %d\n", holdingRegs[1]);
    
    // Si tus dos registros forman un único valor de 32 bits (Dint / Long):
    uint32_t valorCompleto = ((uint32_t)holdingRegs[0] << 16) | holdingRegs[1];
    Serial.printf("Valor completo (32 bits): %u\n", valorCompleto);
    Serial.println("-----------------------");
  } else {
    Serial.printf("Error en la petición Modbus: 0x%02X\n", event);
  }
  return true;
}


void setup() {
  Serial.begin(115200);
  
  // Inicializamos el puerto Serial2 con los pines del ESP32
  Serial1.begin(9600, SERIAL_8N1, B_RS485, A_RS485);
  
  mb.begin(&Serial1);
  mb.master(); // Configuramos el ESP32 como Maestro
}

void loop() {
  // Aseguramos que no haya transacciones pendientes antes de enviar otra
  if (!mb.slave()) {
    
      //Parámetros de mb.readHreg:
      //1. ID del esclavo = 1
      //2. Dirección de inicio = 64512
      //3. Array de destino = holdingRegs
      //4. Cantidad de registros a leer = 2
      //5. Función callback = cbRead
    
    mb.readHreg(1, 64512, holdingRegs, 2, cbRead);
  }
  
  mb.task();
  yield();
  
  // Añadimos un pequeño retraso para no saturar el bus RS485 (ej. cada 2 segundos)
  delay(2000); 
}
*/
//------------------------------------------------------------------------------------------------------
/*
#include <ArduinoRS485.h>
#include <ModbusRTUClient.h>

// --- CONFIGURACIÓN ESPECÍFICA DE TU PETICIÓN ---
#define SLAVE_ADDRESS         1         // ID del dispositivo esclavo
#define TARGET_ADDRESS    64512         // Dirección del registro inicial (addr)
#define NUM_REGISTERS         2         // Cantidad de registros a leer (2 registros uint16_t)

#define BAUDRATE          9600         // Baudios (Ajusta este valor si tu esclavo usa otra velocidad)

void setup() {
  Serial.begin(115200);
  while(!Serial){}
  
  // Configuración de los pines RS485 nativos de la placa Weidmüller
  RS485.setPins(RS485_TX, RS485_DE, RS485_RE);
  
  // Inicializamos el cliente Modbus RTU
  // (Nota: Si tu dispositivo no usa paridad par, cambia SERIAL_8E1 por SERIAL_8N1)
  if(!ModbusRTUClient.begin(BAUDRATE, SERIAL_8N1)){   
    Serial.println("¡Error! No se pudo inicializar el cliente Modbus RTU.");    
  }
  else {
    Serial.println("Cliente Modbus RTU inicializado correctamente.");
    Serial.printf("Configurado para leer ID: %d | Addr: %d | Cantidad: %d\n\n", SLAVE_ADDRESS, TARGET_ADDRESS, NUM_REGISTERS);
  }
}

void loop() {
  Serial.println("--- Iniciando solicitud de lectura ---");
  
  // Lanzamos la petición al esclavo para leer Holding Registers (Función 03)
  if (ModbusRTUClient.requestFrom(SLAVE_ADDRESS, HOLDING_REGISTERS, TARGET_ADDRESS, NUM_REGISTERS)) {
    Serial.println("[ÉXITO] Datos recibidos del esclavo:");
    
    // Leemos los dos registros de forma consecutiva
    for(int i = 0; i < NUM_REGISTERS; i++){
      // ModbusRTUClient.read() devuelve un tipo 'long', lo casteamos a uint16_t para tu formato
      uint16_t valorRegistro = (uint16_t)ModbusRTUClient.read();
      
      Serial.printf("  -> Registro [%d]: %u (en HEX: 0x%04X)\n", TARGET_ADDRESS + i, valorRegistro, valorRegistro);
    }
  } 
  else {
    // Si la lectura falla (por timeout, cableado o configuración de baudios/paridad errónea)
    Serial.print("[ERROR] Falló la lectura. Código de error: ");
    Serial.println(ModbusRTUClient.lastError());
  }

  Serial.println("---------------------------------------\n");
  
  // Pausa de 3 segundos antes de volver a realizar la solicitud en el loop
  delay(3000);
}
  */