

#include <ArduinoRS485.h>
#include <ModbusRTUClient.h>
#include "JmodbusBridge.h"
#include "config_mb_rtu.h"

#include <SPI.h>       
#include <Ethernet.h> 

#define BAUDRATE          9600         // Baudios (Ajusta este valor si tu esclavo usa otra velocidad)

// --- CONFIGURACIÓN ESPECÍFICA DE TU PETICIÓN ---
//#define SLAVE_ADDRESS         1         // ID del dispositivo esclavo
//#define TARGET_ADDRESS    64512         // Dirección del registro inicial (addr)
//#define NUM_REGISTERS         2         // Cantidad de registros a leer (2 registros uint16_t)

// 1. CONFIGURACIÓN DE RED (Tuya probada)
byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
IPAddress ip(192, 168, 1, 150); 
uint16_t modbusPort = 502;    

// 2. INSTANCIA DEL SERVIDOR SEGURO
class WeidosModbusServer : public EthernetServer {
public:
    WeidosModbusServer(uint16_t port) : EthernetServer(port) {}
    virtual void begin(uint16_t port) override {
        EthernetServer::begin(); 
    }
};

WeidosModbusServer modbusServer(modbusPort);

byte modbusTCPRequestBuffer[SIZE_MB_TCP_REQUEST] = {0};
byte modbusRTURequestBuffer[SIZE_MB_RTU_REQUEST] = {0};

modbusTCPStruct mb_tcp_struct; 
modbusRTUStruct mb_rtu_struct; 

//pienso que esta variable puede llegar a ser util
//uint16_t n_bytes_tcp_resp = 7 + 1 + 1; // 7(MBAP) + 1(Fn) + 1 (Conteo) + ¿data(calcular)? 

void setup() {
  Serial.begin(115200);
  while(!Serial){}

    Serial.println("\n--- SIMULADOR RESPONDEDOR MODBUS TCP ---");

  // Inicializar Ethernet sin crasheos
  Ethernet.init(ETHERNET_CS);
  Ethernet.begin(mac, ip);
  modbusServer.begin(modbusPort);

  //Serial.print("Servidor listo en IP: ");
  //Serial.print(Ethernet.localIP());
  //Serial.print(" | Puerto: ");
  //Serial.println(modbusPort);
  //Serial.println("Esperando peticiones de lectura del SCADA...");
  
  //loadModbusRTUReqBuf(modbusTCPRequestBuffer, modbusRTURequestBuffer); 
  //showInfoModbusRTUReq(modbusRTURequestBuffer);

  //getStructModbusRTUReq(modbusRTURequestBuffer, &mb_rtu_struct);
  // Configuración de los pines RS485 nativos de la placa Weidmüller
  RS485.setPins(RS485_TX, RS485_DE, RS485_RE);
  
  // Inicializamos el cliente Modbus RTU
  // (Nota: Si tu dispositivo no usa paridad par, cambia SERIAL_8E1 por SERIAL_8N1)
  if(!ModbusRTUClient.begin(BAUDRATE, SERIAL_8N1)){   
    Serial.println("¡Error! No se pudo inicializar el cliente Modbus RTU.");    
  }else {
    Serial.println("Cliente Modbus RTU inicializado correctamente.");
    Serial.printf("Configurado para leer ID: %d | Addr: %d | Cantidad: %d\n\n", mb_rtu_struct.slaveID, mb_rtu_struct.address, mb_rtu_struct.quantity);
 
    //Serial.printf("Configurado para leer ID: %d | Addr: %d | Cantidad: %d\n\n", SLAVE_ADDRESS, TARGET_ADDRESS, NUM_REGISTERS);
  }

}

void loop() {
  // 1. Escuchar si llega algún cliente TCP (Modbus Poll)
  
  EthernetClient client = modbusServer.available();
  if (client) {
    Serial.println("\n[Modbus Poll] ¡Cliente conectado y socket abierto!");

    // 2. Mantenernos dentro de este bucle mientras Modbus Poll siga conectado
    while (client.connected()) {
      
      // Si hay datos disponibles en el canal, procesamos la petición
      if (client.available()) {
        //byte tramaBuffer[260];  
        int index = 0;

        // Leer los bytes de la petición actual
        while (client.available() && index < sizeof(modbusTCPRequestBuffer)) {
          modbusTCPRequestBuffer[index++] = client.read();
          delayMicroseconds(100); // Pequeña pausa para asegurar que entran todos los bytes
        }
        //getStructModbusTCPReq(modbusTCPRequestBuffer, &mb_tcp_struct); 
        loadModbusRTUReqBuf(modbusTCPRequestBuffer, modbusRTURequestBuffer);

        //showInfoModbusTCPReq(modbusTCPRequestBuffer); 
        //showInfoModbusRTUReq(modbusRTURequestBuffer); 

        getStructModbusRTUReq(modbusRTURequestBuffer, &mb_rtu_struct);

        //una vez obtenido el request de modbus , tenemos informacion del tamaño del dato que volverá
        //n_bytes_tcp_resp = n_bytes_tcp_resp + mb_rtu_struct.quantity; // en bytes

if (ModbusRTUClient.requestFrom(mb_rtu_struct.slaveID, getModbusClientDataType(mb_rtu_struct.functionCode), mb_rtu_struct.address, mb_rtu_struct.quantity)) {
          Serial.println("[ÉXITO] Datos recibidos del esclavo por RS485. Respondiendo al SCADA...");
          
          // 1. Calcular el tamaño de los datos puros en bytes
          uint8_t byteCount = mb_rtu_struct.quantity * 2; 

          // 2. Calcular el campo 'Length' de la cabecera MBAP
          // El campo Length cuenta todo lo que va después de sí mismo: UnitID(1) + Fn(1) + Conteo(1) + Datos(byteCount)
          uint16_t tcpLength = 3 + byteCount;

          // 3. ENVIAR CABECERA MBAP DIRECTAMENTE AL CLIENTE TCP
          // Transaction ID (2 bytes) - Se clona exactamente de la petición original
          client.write(modbusTCPRequestBuffer[0]);
          client.write(modbusTCPRequestBuffer[1]);

          // Protocol ID (2 bytes) - Siempre 0x0000
          client.write((uint8_t)0);
          client.write((uint8_t)0);

          // Length (2 bytes) - Big Endian
          client.write(highByte(tcpLength));
          client.write(lowByte(tcpLength));

          // Unit ID / Slave ID (1 byte)
          client.write(mb_rtu_struct.slaveID);

          // 4. ENVIAR PARTE FIJA DE LA PDU MODBUS
          // Function Code (1 byte)
          client.write(mb_rtu_struct.functionCode);

          // Byte Count (1 byte)
          client.write(byteCount);

          // 5. LEER DEL BUS SERIE Y TRANSMITIR AL VUELO POR TCP
          for(int i = 0; i < mb_rtu_struct.quantity; i++){
            uint16_t valorRegistro = (uint16_t)ModbusRTUClient.read();
            
            // Enviamos el registro de 16 bits dividido en 2 bytes (Big Endian: Alto primero, Bajo después)
            client.write(highByte(valorRegistro));
            client.write(lowByte(valorRegistro));
            
            // Serial de depuración opcional por si quieres monitorizarlo
            // Serial.printf("  -> Transmitido Reg [%d]: 0x%04X\n", mb_rtu_struct.address + i, valorRegistro);
          }

          //Serial.println("[TCP] Respuesta enviada de forma limpia y directa al SCADA.");

        } else {
          Serial.print("[ERROR] Falló la lectura en el bus serie. Código: ");
          Serial.println(ModbusRTUClient.lastError());
          // Aquí opcionalmente podrías enviar una excepción Modbus por TCP si lo deseas más adelante
        }
     
      }
      
      yield(); // Liberar CPU para tareas internas del ESP32
    }

    // 5. Si el programa llega aquí, es porque Modbus Poll se desconectó voluntariamente
    //Serial.println("[Modbus Poll] Cliente desconectado. Socket liberado de forma limpia.");
  }


  //delay(3000);
}


/*                             hasta aqui todo iba bien 
#include <ArduinoRS485.h>
#include <ModbusRTUClient.h>
#include "JmodbusBridge.h"

// --- CONFIGURACIÓN ESPECÍFICA DE TU PETICIÓN ---
//#define SLAVE_ADDRESS         1         // ID del dispositivo esclavo
//#define TARGET_ADDRESS    64512         // Dirección del registro inicial (addr)
//#define NUM_REGISTERS         2         // Cantidad de registros a leer (2 registros uint16_t)

#define BAUDRATE          9600         // Baudios (Ajusta este valor si tu esclavo usa otra velocidad)

//inicializame simulando por modbus TCP una peticion a ID = 1 , HOLDING REGISTER, adress 0 , y tamaño 2 
byte modbusTCPRequestBuffer[SIZE_MB_TCP_REQUEST] = {
  0x00, 0x01, // Transaction ID (2 bytes)
  0x00, 0x00, // Protocol ID (2 bytes, siempre cero)
  0x00, 0x06, // Length (2 bytes, cuenta los bytes que faltan)
  0x01,       // Unit ID / Slave ID (1 byte)
  0x03,       // Function Code (1 byte, 0x03 = Read Holding Registers)
  0x00, 0x00, // Starting Address (2 bytes, Dirección 0)
  0x00, 0x02  // Quantity of Registers (2 bytes, solicita 2 registros)
};

byte modbusRTURequestBuffer[SIZE_MB_RTU_REQUEST] = {0};

modbusRTUStruct mb_rtu_struct; 

void setup() {
  Serial.begin(115200);
  while(!Serial){}
  
    loadModbusRTUReqBuf(modbusTCPRequestBuffer, modbusRTURequestBuffer); 
  showInfoModbusRTUReq(modbusRTURequestBuffer);

  getStructModbusRTUReq(modbusRTURequestBuffer, &mb_rtu_struct);
  // Configuración de los pines RS485 nativos de la placa Weidmüller
  RS485.setPins(RS485_TX, RS485_DE, RS485_RE);
  
  // Inicializamos el cliente Modbus RTU
  // (Nota: Si tu dispositivo no usa paridad par, cambia SERIAL_8E1 por SERIAL_8N1)
  if(!ModbusRTUClient.begin(BAUDRATE, SERIAL_8N1)){   
    Serial.println("¡Error! No se pudo inicializar el cliente Modbus RTU.");    
  }else {
    Serial.println("Cliente Modbus RTU inicializado correctamente.");
    Serial.printf("Configurado para leer ID: %d | Addr: %d | Cantidad: %d\n\n", mb_rtu_struct.slaveID, mb_rtu_struct.address, mb_rtu_struct.quantity);
 
    //Serial.printf("Configurado para leer ID: %d | Addr: %d | Cantidad: %d\n\n", SLAVE_ADDRESS, TARGET_ADDRESS, NUM_REGISTERS);
  }

}

void loop() {
  Serial.println("--- Iniciando solicitud de lectura ---");
  
  // Lanzamos la petición al esclavo para leer Holding Registers (Función 03)
int getModbusClientDataType(uint8_t functionCode);
  if (ModbusRTUClient.requestFrom(mb_rtu_struct.slaveID, getModbusClientDataType(mb_rtu_struct.functionCode), mb_rtu_struct.address, mb_rtu_struct.quantity)) {
  //if (ModbusRTUClient.requestFrom(SLAVE_ADDRESS, HOLDING_REGISTERS, TARGET_ADDRESS, NUM_REGISTERS)) {
    Serial.println("[ÉXITO] Datos recibidos del esclavo:");
    
    // Leemos los dos registros de forma consecutiva
    for(int i = 0; i < mb_rtu_struct.quantity; i++){
      // ModbusRTUClient.read() devuelve un tipo 'long', lo casteamos a uint16_t para tu formato
      uint16_t valorRegistro = (uint16_t)ModbusRTUClient.read();
      
      Serial.printf("  -> Registro [%d]: %u (en HEX: 0x%04X)\n", mb_rtu_struct.address + i, valorRegistro, valorRegistro);
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
}*/

//------------------------------------ESTE CODIGO ES CLAVE------------------------------------------

/*
#include <Arduino.h>
#include <SPI.h>       
#include <Ethernet.h> 

#include "JmodbusBridge.h"

// 1. CONFIGURACIÓN DE RED (Tuya probada)
byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
IPAddress ip(192, 168, 1, 150); 
uint16_t modbusPort = 502;      

// 2. INSTANCIA DEL SERVIDOR SEGURO
class WeidosModbusServer : public EthernetServer {
public:
    WeidosModbusServer(uint16_t port) : EthernetServer(port) {}
    virtual void begin(uint16_t port) override {
        EthernetServer::begin(); 
    }
};

WeidosModbusServer modbusServer(modbusPort);

byte modbusTCPRequestBuffer[SIZE_MB_TCP_REQUEST] = {0};
byte modbusRTURequestBuffer[SIZE_MB_RTU_REQUEST] = {0};

void setup() {
  Serial.begin(115200);
  while (!Serial) { ; } 
  Serial.println("\n--- SIMULADOR RESPONDEDOR MODBUS TCP ---");

  // Inicializar Ethernet sin crasheos
  Ethernet.init(ETHERNET_CS);
  Ethernet.begin(mac, ip);
  modbusServer.begin(modbusPort);

  Serial.print("Servidor listo en IP: ");
  Serial.print(Ethernet.localIP());
  Serial.print(" | Puerto: ");
  Serial.println(modbusPort);
  Serial.println("Esperando peticiones de lectura del SCADA...");
}

void loop() {
  // 1. Escuchar si llega algún cliente TCP (Modbus Poll)
  EthernetClient client = modbusServer.available();

  if (client) {
    Serial.println("\n[Modbus Poll] ¡Cliente conectado y socket abierto!");

    // 2. Mantenernos dentro de este bucle mientras Modbus Poll siga conectado
    while (client.connected()) {
      
      // Si hay datos disponibles en el canal, procesamos la petición
      if (client.available()) {
        //byte tramaBuffer[260];  
        int index = 0;

        // Leer los bytes de la petición actual
        while (client.available() && index < sizeof(modbusTCPRequestBuffer)) {
          modbusTCPRequestBuffer[index++] = client.read();
          delayMicroseconds(100); // Pequeña pausa para asegurar que entran todos los bytes
        }

        loadModbusRTUReqBuf(modbusTCPRequestBuffer, modbusRTURequestBuffer);

        showInfoModbusTCPReq(modbusTCPRequestBuffer); 
        showInfoModbusRTUReq(modbusRTURequestBuffer); 

        // hasta aqui correcto..... 

        //// 3. Si la trama tiene el tamaño mínimo de la cabecera MBAP
        //if (index >= 7) {
        //  uint16_t transactionId = (modbusTCPRequestBuffer[0] << 8) | modbusTCPRequestBuffer[1];
        //  uint8_t unitId = modbusTCPRequestBuffer[6];
        //  uint8_t functionCode = modbusTCPRequestBuffer[7];
//
        //  Serial.printf("[SCADA] Petición -> TxID: %d, SlaveID: %d, Fn: 0x%02X\n", transactionId, unitId, functionCode);
//
        //  // 4. Responder únicamente a la Función 0x03 (Read Holding Registers)
        //  if (functionCode == 0x03) {
        //    uint16_t valorSimulado = 1234; // El dato de prueba que va a ver Modbus Poll
        //    byte respuesta[11];
        //    
        //    // Replicamos el ID de Transacción exacto que nos envió Modbus Poll
        //    respuesta[0] = modbusTCPRequestBuffer[0];
        //    respuesta[1] = modbusTCPRequestBuffer[1];
        //    
        //    // ID de Protocolo (Siempre 0x0000)
        //    respuesta[2] = 0x00;
        //    respuesta[3] = 0x00;
        //    
        //    // Longitud del resto del paquete (5 bytes hacia adelante)
        //    respuesta[4] = 0x00;
        //    respuesta[5] = 0x05; 
        //    
        //    // Unit ID (ID del esclavo solicitado)
        //    respuesta[6] = unitId;
        //    
        //    // Código de Función (0x03)
        //    respuesta[7] = 0x03;
        //    
        //    // Conteo de bytes de datos (1 registro = 2 bytes)
        //    respuesta[8] = 0x02;
        //    
        //    // Desglosamos el valor 1234 (0x04D2) en dos bytes
        //    respuesta[9] = (valorSimulado >> 8) & 0xFF;  // Byte alto (0x04)
        //    respuesta[10] = valorSimulado & 0xFF;         // Byte bajo (0xD2)
//
        //    // Enviamos la respuesta manteniendo el socket abierto
        //    client.write(respuesta, 11);
        //    
        //    Serial.printf("[ESP32] Respondido valor: %d\n", valorSimulado);
        //  }
        //}
        
      }
      
      yield(); // Liberar CPU para tareas internas del ESP32
    }

    // 5. Si el programa llega aquí, es porque Modbus Poll se desconectó voluntariamente
    Serial.println("[Modbus Poll] Cliente desconectado. Socket liberado de forma limpia.");
  }
}
*/
//---------------------------------------------------------------------------------------------------------------------------


/*
#include <SPI.h>
#include <Ethernet.h>

// Configuración de red (Ajustada a los valores de tu pasarela)
byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
IPAddress ip(192, 168, 1, 150); // IP fija para el WEIDOS
uint16_t modbusPort = 502;      // Puerto Modbus TCP estándar

// --- CLASE PARCHE PARA SOLUCIONAR EL CONFLICTO DE WEIDMÜLLER ---
class WeidosModbusServer : public EthernetServer {
public:
    // Pasamos el puerto al constructor de la librería original
    WeidosModbusServer(uint16_t port) : EthernetServer(port) {}

    // Implementamos la función virtual pura tal y como la exige Server.h de Weidmüller
    virtual void begin(uint16_t port) override {
        // Ignoramos el parámetro 'port' y llamamos al begin original sin argumentos
        EthernetServer::begin(); 
    }
};

// Instanciamos nuestro servidor corregido usando la nueva clase limpia
WeidosModbusServer modbusServer(modbusPort);

void mostrarTramaHex(byte* buffer, int longitud);

void setup() {
  Serial.begin(9600);
  while (!Serial) { ; } 

  Serial.println("--- Extractor de Tramas Modbus TCP ---");

  // El pin ETHERNET_CS ya viene configurado automáticamente en pins_weidos.h
  Ethernet.init(ETHERNET_CS);

  // Iniciar la interfaz física Ethernet
  Ethernet.begin(mac, ip);
  
  // Iniciamos el servidor pasando el puerto para saciar al compilador
  modbusServer.begin(modbusPort);

  Serial.print("Servidor Ethernet Modbus TCP listo en IP: ");
  Serial.print(Ethernet.localIP());
  Serial.print(" | Puerto: ");
  Serial.println(modbusPort);
}

void loop() {
  // Escuchar si llega algún cliente TCP (Modscan, PLC, etc.)
  EthernetClient client = modbusServer.available();

  if (client) {
    Serial.println("\n[NUEVA PETICIÓN] Cliente conectado.");
    
    byte tramaBuffer[260]; 
    int index = 0;

    // Mientras el cliente esté conectado y queden bytes en el chip de red
    while (client.connected() && client.available()) {
      byte c = client.read();
      
      if (index < sizeof(tramaBuffer)) {
        tramaBuffer[index] = c;
        index++;
      }
    }

    // Si hemos leído algo del canal, lo imprimimos por serial
    if (index > 0) {
      mostrarTramaHex(tramaBuffer, index);
    }

    // Forzamos el cierre para liberar el socket en el chip de red del WEIDOS
    client.stop();
    Serial.println("[SOCKET] Cliente desconectado y socket liberado.");
  }
}

// Función auxiliar para imprimir la trama en formato HEX limpio
void mostrarTramaHex(byte* buffer, int longitud) {
  Serial.print("-> Trama recibida (Longitud: ");
  Serial.print(longitud);
  Serial.println(" bytes):");
  
  Serial.print("HEX: ");
  for (int i = 0; i < longitud; i++) {
    if (buffer[i] < 0x10) {
      Serial.print("0"); // Mantiene el formato de dos dígitos (ej: 0A en vez de A)
    }
    Serial.print(buffer[i], HEX);
    Serial.print(" ");
  }
  Serial.println();

  // Desglose básico de la cabecera Modbus TCP (MBAP)
  if (longitud >= 7) {
    uint16_t transactionId = (buffer[0] << 8) | buffer[1];
    uint16_t protocolId = (buffer[2] << 8) | buffer[3];
    uint16_t lenField = (buffer[4] << 8) | buffer[5];
    byte unitId = buffer[6];
    byte functionCode = (longitud > 7) ? buffer[7] : 0;

    Serial.println("--- Desglose MBAP ---");
    Serial.print("  [ID Transacción]: "); Serial.println(transactionId);
    Serial.print("  [ID Protocolo]:    "); Serial.println(protocolId);
    Serial.print("  [Longitud PDU]:   "); Serial.println(lenField);
    Serial.print("  [ID Unidad]:       "); Serial.println(unitId);
    if(longitud > 7) {
      Serial.print("  [Código Función]:  0x"); Serial.println(functionCode, HEX);
    }
  }
  Serial.println("------------------------------------");
}

*/
/*

#include <SPI.h>
#include <Ethernet.h>

// Configuración de red
byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
IPAddress ip(192, 168, 1, 150); 
uint16_t modbusPort = 502;      

// --- SOLUCIÓN PARA EL ERROR ABSTRACT TYPE ---
// Creamos una subclase que hereda de EthernetServer y completa la función virtual pura
class WeidosModbusServer : public EthernetServer {
public:
    // Pasamos el puerto al constructor base
    WeidosModbusServer(uint16_t port) : EthernetServer(port) {}

    // Implementamos la función que le falta a la librería de Weidmüller
    virtual void begin(uint16_t port) override {
        // Simplemente llama al begin() original de la librería Ethernet
        EthernetServer::begin(); 
    }
};

// Instanciamos nuestro servidor corregido
WeidosModbusServer modbusServer(modbusPort);

void mostrarTramaHex(byte* buffer, int longitud);

void setup() {
  Serial.begin(115200);
  while (!Serial) { ; } 

  Serial.println("--- Extractor de Tramas Modbus TCP ---");

  // Inicializar chip Ethernet usando el CS por defecto (Pin 2 internamente)
  Ethernet.init(ETHERNET_CS);

  // Iniciar la conexión Ethernet
  Ethernet.begin(mac, ip);
  
  // Iniciamos el servidor llamando a nuestra función corregida
  modbusServer.begin(modbusPort);

  Serial.print("Servidor Ethernet Modbus TCP listo en IP: ");
  Serial.print(Ethernet.localIP());
  Serial.print(" | Puerto: ");
  Serial.println(modbusPort);
}

void loop() {
  // Escuchar si llega algún cliente TCP
  EthernetClient client = modbusServer.available();

  if (client) {
    Serial.println("\n[NUEVA PETICIÓN] Cliente conectado.");
    
    byte tramaBuffer[260]; 
    int index = 0;

    // Mientras el cliente esté conectado y tenga bytes listos para leer
    while (client.connected() && client.available()) {
      byte c = client.read();
      
      if (index < sizeof(tramaBuffer)) {
        tramaBuffer[index] = c;
        index++;
      }
    }

    // Si capturamos bytes, procedemos a mostrarlos en pantalla (Serial)
    if (index > 0) {
      mostrarTramaHex(tramaBuffer, index);
    }

    // Cerramos la conexión con el cliente para liberar el socket
    client.stop();
    Serial.println("[SOCKET] Cliente desconectado y socket liberado.");
  }
}

// Función auxiliar para imprimir la trama en formato HEX limpio
void mostrarTramaHex(byte* buffer, int longitud) {
  Serial.print("-> Trama recibida (Longitud: ");
  Serial.print(longitud);
  Serial.println(" bytes):");
  
  Serial.print("HEX: ");
  for (int i = 0; i < longitud; i++) {
    if (buffer[i] < 0x10) {
      Serial.print("0"); 
    }
    Serial.print(buffer[i], HEX);
    Serial.print(" ");
  }
  Serial.println();

  // Desglose básico de la cabecera Modbus TCP (MBAP)
  if (longitud >= 7) {
    uint16_t transactionId = (buffer[0] << 8) | buffer[1];
    uint16_t protocolId = (buffer[2] << 8) | buffer[3];
    uint16_t lenField = (buffer[4] << 8) | buffer[5];
    byte unitId = buffer[6];
    byte functionCode = (longitud > 7) ? buffer[7] : 0;

    Serial.println("--- Desglose MBAP ---");
    Serial.print("  [ID Transacción]: "); Serial.println(transactionId);
    Serial.print("  [ID Protocolo]:    "); Serial.println(protocolId);
    Serial.print("  [Longitud PDU]:   "); Serial.println(lenField);
    Serial.print("  [ID Unidad]:       "); Serial.println(unitId);
    if(longitud > 7) {
      Serial.print("  [Código Función]:  0x"); Serial.println(functionCode, HEX);
    }
  }
  Serial.println("------------------------------------");
}

*/