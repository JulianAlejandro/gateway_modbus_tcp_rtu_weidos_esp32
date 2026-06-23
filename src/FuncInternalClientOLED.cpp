#include "FuncInternalClientOLED.h"

U8G2 *u8g2;



void initOLED() {

  /*
  // Mantenemos el constructor de alta velocidad para la pasarela
  u8g2 = new U8G2_SSD1306_128X32_UNIVISION_1_HW_I2C(U8G2_R0, U8X8_PIN_NONE);
  u8g2->setBusClock(400000); 
  u8g2->begin();

  //Wire.setTimeout(2); 

  // ──> CORRECCIÓN CRÍTICA DEL LOGO:
  // Envolvemos el dibujo en un bucle 'do-while' de páginas.
  // Esto obliga a la pantalla a procesar las 4 franjas de 8px del logo.
  u8g2->firstPage();
  do {
    // Llamamos a tu función de librería, pero ahora se ejecutará cooperativamente 
    // por cada franja de la pantalla, completando el logotipo de EMASESA.
    showLogo2LinesLeft(*u8g2, emasesa_icon, "EMASESA", "Calidad del agua");
  } while (u8g2->nextPage());

  delay(1500); 
  
  */

    // Crear el objeto U8G2 según tu hardware (ajusta si es necesario)
  u8g2 = new U8G2_SSD1306_128X32_UNIVISION_F_HW_I2C(U8G2_R0, U8X8_PIN_NONE);
  u8g2->begin();
  
  // Pantalla de inicio con logo
  showLogo2LinesLeft(*u8g2, emasesa_icon, "EMASESA", "Calidad del agua");
  delay(1500);
}



void drawOLEAD(const char* name, float value, const char* unit) {

  char line1[20]; 
  char line2[40]; 

  strncpy(line1, name, sizeof(line1) - 1);
  line1[sizeof(line1) - 1] = '\0';

  String valStr = String(value, 2);
  snprintf(line2, sizeof(line2), " %s %s", valStr.c_str(), unit);

  // Mostrar en la pantalla física
  show2LinesFull(*u8g2, line1, line2);
}

/*
// ---------------------------------------------------------
// Dibuja pantalla actual (llamada por updateOLEAD)
// ---------------------------------------------------------
void drawOLEAD(byte idx) {
  char line1[20];
  char line2[40];  
  unsigned long now = millis();
  
  // Línea 1: Nombre del esclavo
  strcpy(line1, slaveConfig[idx].name);
  
  // Línea 2: Evaluación del estado real del dato
  if (slaveDisabled[idx]) {
    strcpy(line2, "OFFLINE");
  } else if (slaveLastUpdate[idx] == 0) {
    strcpy(line2, "unknown");
  } else {
    // Convertimos el valor flotante a string según sus decimales configurados
    String valStr = String(slaveData[idx], slaveConfig[idx].n_dec);  
    
    if ((now - slaveLastUpdate[idx]) > STALE_TIMEOUT) {
      // ESTADO 1: El tiempo ha expirado sin actualizaciones -> STALE [Dato] [Unidad]
      snprintf(line2, sizeof(line2), "Stale %s %s", valStr.c_str(), slaveConfig[idx].units);
    } 
    else if (slaveIsNew[idx]) {
      // ESTADO 2: El dato acaba de entrar en este ciclo -> NEW [Dato] [Unidad]
      snprintf(line2, sizeof(line2), "new %s %s", valStr.c_str(), slaveConfig[idx].units);
      
      // CRÍTICO: Una vez mostrado una vez en pantalla, deja de ser "nuevo"
      slaveIsNew[idx] = false; 
    } 
    else {
      // ESTADO 3: El dato es válido pero es el mismo de antes -> [Dato] [Unidad] (Sin prefijos)
      snprintf(line2, sizeof(line2), "%s %s", valStr.c_str(), slaveConfig[idx].units);
    }
  }
  
  // Mostrar en la pantalla física
  show2LinesFull(*u8g2, line1, line2);
}
*/