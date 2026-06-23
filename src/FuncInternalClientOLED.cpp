#include "FuncInternalClientOLED.h"

// Instanciamos el objeto físico u8g2 usando el constructor cooperativo de página (_1_)
U8G2_SSD1306_128X32_UNIVISION_1_HW_I2C u8g2Instance(U8G2_R0, U8X8_PIN_NONE);
U8G2 *u8g2 = &u8g2Instance;

// Máquina de estados privada del archivo
enum DisplayState {
  OLED_IDLE,
  START_RENDER,
  RENDERING_PAGES
};

static DisplayState displayState = OLED_IDLE;
static unsigned long lastChange = 0;
static uint8_t currentIdx = 0;

// Caché de texto local para no usar strings pesadas dentro del bucle de pintado
static char cachedLine1[20];
static char cachedLine2[40];

void initOLED() {
  u8g2->setBusClock(400000); // 400 Khz alta velocidad I2C
  u8g2->begin();

  // Bucle cooperativo para pintar el logo sin congelar la pantalla
  u8g2->firstPage();
  do {
    showLogo2LinesLeft(*u8g2, emasesa_icon, "EMASESA", "Calidad del agua");
  } while (u8g2->nextPage());

  delay(1500); 
}

// Función interna auxiliar para preparar los textos evaluando la telemetría
static void prepareTextData(uint8_t idx) {
  unsigned long now = millis();
  
  float currentVal = 0.0;
  unsigned long currentLastUpload = 0;
  bool isDisabled = false;
  bool isNewData = false;

  // Extracción ultrarrápida protegiendo la memoria con el Mutex global
  if (xModbusDataMutex != NULL && xSemaphoreTake(xModbusDataMutex, pdMS_TO_TICKS(5)) == pdTRUE) {
    currentVal = slaves[idx].convertedData;
    currentLastUpload = slaves[idx].lastUpload;
    isDisabled = slaves[idx].disable;
    isNewData = slaves[idx].isNew;
    
    if (isNewData) {
      slaves[idx].isNew = false; // Consumimos la bandera de dato fresco
    }
    xSemaphoreGive(xModbusDataMutex);
  }

  // Formatear Línea 1 (Nombre del Dispositivo)
  strncpy(cachedLine1, slaves[idx].name, sizeof(cachedLine1) - 1);
  cachedLine1[sizeof(cachedLine1) - 1] = '\0';
  
  // Formatear Línea 2 (Estado o Medición)
  if (isDisabled) {
    strcpy(cachedLine2, "OFFLINE");
  } else if (currentLastUpload == 0) {
    strcpy(cachedLine2, "unknown");
  } else {
    String valStr = String(currentVal, 2); 
    
    if ((now - currentLastUpload) > STALE_TIMEOUT) {
      snprintf(cachedLine2, sizeof(cachedLine2), "Stale %s %s", valStr.c_str(), slaves[idx].unit);
    } 
    else if (isNewData) {
      snprintf(cachedLine2, sizeof(cachedLine2), "->%s %s", valStr.c_str(), slaves[idx].unit);
    } 
    else {
      snprintf(cachedLine2, sizeof(cachedLine2), "%s %s", valStr.c_str(), slaves[idx].unit);
    }
  }
}

void updateOLED() { 
  unsigned long now = millis();
  
  switch (displayState) {
    case OLED_IDLE:
      if (now - lastChange >= DISPLAY_INTERVAL) {
        lastChange = now;
        
        // Operación atómica de strings
        prepareTextData(currentIdx);
        
        displayState = START_RENDER;
      }
      break;
      
    case START_RENDER:
      u8g2->firstPage();
      displayState = RENDERING_PAGES;
      break;
      
    case RENDERING_PAGES:
      // Dibujamos usando la caché de texto precalculada
      u8g2->setFont(u8g2_font_9x15_tr); 
      u8g2->drawStr(0, 15, cachedLine1);    

      u8g2->setFont(u8g2_font_9x15_tr);
      u8g2->drawStr(0, 31, cachedLine2);    

      // Transfiere solo una franja de la pantalla a la vez (~1.5ms)
      if (u8g2->nextPage() == 0) {
        displayState = OLED_IDLE;
        
        // Rotamos al siguiente esclavo del carrusel
        currentIdx = (currentIdx + 1) % NUM_SLAVES;
      }
      break;
  }
}