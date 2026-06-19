#include <Arduino.h>
#include "ModbusRTUModule.h"
#include "ModbusTCPModule.h"

#define BAUDRATE 9600

byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
IPAddress ip(192, 168, 1, 150); 
uint16_t modbusPort = 502;    

ModbusRTUModule rtuModule(BAUDRATE);
ModbusTCPModule tcpModule(modbusPort, &rtuModule);

void setup() {
  Serial.begin(115200);
  while(!Serial){}
  Serial.println("\n--- PASARELA MODBUS TCP A RTU MODULAR ---");

  if(!rtuModule.begin()){   
    Serial.println("Error RTU");    
  } else {
    Serial.println("RTU listo.");
  }

  tcpModule.begin(mac, ip);
  Serial.println("TCP listo."); 
}

void loop() {
  tcpModule.process();
}
