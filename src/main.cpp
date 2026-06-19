#include <Arduino.h>
#include "ModbusRTUModule.h"
#include "ModbusTCPModule.h"

#define BAUDRATE 9600

byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
IPAddress ip(192, 168, 1, 150); 
uint16_t modbusPort = 502;    

ModbusRTUModule slaveRtuModule(BAUDRATE);
ModbusTCPModule MasterTcpModule(modbusPort, &slaveRtuModule);

void checkTCPReqCallback(const modbusTCPStruct& req) { // este callback se ejecuta solo cuando se ha recibido una peticion modbus TCP y queremos informacion 
    if (req.slaveID == 1 && req.address == 0 && req.quantity == 2) { // por ejemplo
        Serial.println("efectuamos una accion");  
    }
}

void setup() {
  Serial.begin(115200);
  while(!Serial){}
  Serial.println("\n--- PASARELA MODBUS TCP A RTU MODULAR ---");

  if(!slaveRtuModule.begin()){   
    Serial.println("Error RTU");    
  } else {
    Serial.println("RTU listo.");
  }

  MasterTcpModule.setInterceptor(checkTCPReqCallback); 

  MasterTcpModule.begin(mac, ip);
  Serial.println("TCP listo."); 
}

void loop() {
  MasterTcpModule.process();
}
