#ifndef FUNCTION_INTERNAL_CLIENT_H
#define FUNCTION_INTERNAL_CLIENT_H

#include <U8g2lib.h>
#include <WPriv_OLED128X32emasesa.h>  // Tu librería con funciones de display

void drawOLEAD(const char* name, float value, const char* unit); 
void initOLED(); 

#endif


/*
#define NUM_SLAVES 5
#define NUM_REGS_READ 2; 

const byte slaveIDs[NUM_SLAVES] = {1, 2, 3, 4, 5};
unsigned long slaveLastUpdate[NUM_SLAVES];

const unsigned long INTERNAL_POLL_THRESHOLD = 30000;
const unsigned long INTERNAL_CHECK_INTERVAL = 5000;

struct PollConfig {
    byte func;        // 0x03 = holding registers, 0x04 = input registers
    uint16_t address; // Dirección inicial del primer registro
};


const PollConfig pollConfig[NUM_SLAVES] = {
  {0x03, 0xFC00}, // ID 1: Cloro   (input register 0)
  {0x03, 0xFC00}, // ID 2: pH       (input register 2)
  {0x03, 0xFC00}, // ID 3: Densidad (input register 4)
  {0x03, 0xFC00}, // ID 4: Turbidez (input register 6)
  {0x03, 0x000A}, // ID 5: Caudal   (input register 8)
  //{0x03, 0x0000}, // ID 10: Temperatura (input register 10)
};


struct slave_Disp_data {
  const char* name; 
  const char* units;
  const int n_dec;
  //const float max_value; quiza en un futuro...
};


const slave_Disp_data slaveConfig[] = {
  {"Cl2", "ppm", 3},          // ID 1
  {"COND", "uS", 1},         // ID 2
  {"REDOX", "mV", 1},        // ID 3
  {"TURB", "NTU", 3},        // ID 4
  {"PH", "pH", 2}           // ID 5
  //{"Temperature", "C", 1}   // ID 10 (Cambiado "degrees" por "°C" para que quede mejor en pantalla)
};
*/
