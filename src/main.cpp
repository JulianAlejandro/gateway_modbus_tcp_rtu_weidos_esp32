

#include <ArduinoRS485.h>
#include <ModbusRTUClient.h>
#include "JmodbusBridge.h"

#include <SPI.h>       
#include <Ethernet.h> 

#define BAUDRATE  9600

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
  
  // Configuración de los pines RS485 nativos de la placa Weidmüller
  RS485.setPins(RS485_TX, RS485_DE, RS485_RE);
  
  // Inicializamos el cliente Modbus RTU

  if(!ModbusRTUClient.begin(BAUDRATE, SERIAL_8N1)){   
    Serial.println("¡Error! No se pudo inicializar el cliente Modbus RTU.");    
  }else {
    Serial.println("Cliente Modbus RTU inicializado correctamente.");
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
        
        loadModbusRTUReqBuf(modbusTCPRequestBuffer, modbusRTURequestBuffer);

        //showInfoModbusTCPReq(modbusTCPRequestBuffer); 
        //showInfoModbusRTUReq(modbusRTURequestBuffer); 

        getStructModbusRTUReq(modbusRTURequestBuffer, &mb_rtu_struct);
 
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

  }

}

