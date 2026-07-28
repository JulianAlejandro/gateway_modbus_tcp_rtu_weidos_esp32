#include "displayOLEDManager.h"

// Instanciamos el objeto físico u8g2 usando el constructor cooperativo de página (_1_)
//U8G2_SSD1306_128X32_UNIVISION_1_HW_I2C u8g2Instance(U8G2_R0, U8X8_PIN_NONE);
//U8G2 *u8g2 = &u8g2Instance;

/*
// Máquina de estados privada del archivo
enum DisplayState {
  OLED_IDLE,
  START_RENDER,
  RENDERING_PAGES
};
*/

//static DisplayState displayState = OLED_IDLE;
//static unsigned long lastChange = 0;
//static uint8_t currentIdx = 0;

// Caché de texto local para no usar strings pesadas dentro del bucle de pintado
//static char cachedLine1[20];
//static char cachedLine2[40];

displayOLEDManager::displayOLEDManager() 
  : _u8g2(U8G2_R0, U8X8_PIN_NONE), _displayState(OLED_IDLE) {
}

void displayOLEDManager::initOLED(ModbusSlaveData* slaves, uint8_t numSlaves, SemaphoreHandle_t dataMutex) {
    
    _slaves = slaves;
  _numSlaves = numSlaves;
  _dataMutex = dataMutex;

  _u8g2.setBusClock(400000); // 400 Khz alta velocidad I2C
  _u8g2.begin();

  // Bucle cooperativo para pintar el logo sin congelar la pantalla
  _u8g2.firstPage();
  do {
    showLogo2LinesLeft(_u8g2, emasesa_icon, "EMASESA", "Calidad del agua");
  } while (_u8g2.nextPage());

  delay(1500); 
}

// Función interna auxiliar para preparar los textos evaluando la telemetría
void displayOLEDManager::prepareTextData(uint8_t idx) {
  //unsigned long now = millis();
  if (_slaves == nullptr || _numSlaves == 0) return;

  float currentVal = 0.0;
  int decimals = 0; 
  unsigned long currentLastUpload = 0;
  //bool isDisabled = false;
  bool isNewData = false;
  uint16_t errCount = 0; 

  // Extracción ultrarrápida protegiendo la memoria con el Mutex global
  if (_dataMutex != NULL && xSemaphoreTake(_dataMutex, pdMS_TO_TICKS(5)) == pdTRUE) {
    currentVal = _slaves[idx].convertedData;
    decimals = _slaves[idx].decimals; 
    currentLastUpload = _slaves[idx].lastTimeReference;
    //isDisabled = slaves[idx].disable;
    isNewData = _slaves[idx].isNew;
    errCount = _slaves[idx].errCounter; 
    
    if (isNewData) {
      _slaves[idx].isNew = false; // Consumimos la bandera de dato fresco
    }
    xSemaphoreGive(_dataMutex);
  }

  // Formatear Línea 1 (Nombre del Dispositivo)
  strncpy(_cachedLine1, _slaves[idx].name, sizeof(_cachedLine1) - 1);
  _cachedLine1[sizeof(_cachedLine1) - 1] = '\0';
  
  // Formatear Línea 2 (Estado o Medición)
  if (errCount >= 5) {
    strcpy(_cachedLine2, "OFFLINE");
  } else if (currentLastUpload == 0) {
    strcpy(_cachedLine2, "unknown");
  } else {
    char valBuf[16];
    dtostrf(currentVal, 0, decimals, valBuf);
    
    if (1 < errCount && errCount < 5 ) {
      snprintf(_cachedLine2, sizeof(_cachedLine2), "Stale %s %s", valBuf, _slaves[idx].unit);
    } 
    else if (isNewData) {
      snprintf(_cachedLine2, sizeof(_cachedLine2), "->%s %s", valBuf, _slaves[idx].unit);
    } 
    else {
      snprintf(_cachedLine2, sizeof(_cachedLine2), "%s %s", valBuf, _slaves[idx].unit);
    }
  }
}

void displayOLEDManager::updateOLED() { 
if (_slaves == nullptr || _numSlaves == 0) return;
  unsigned long now = millis();
  
  switch (_displayState) {
    case OLED_IDLE:
      if (now - _lastChange >= DISPLAY_INTERVAL) {
        _lastChange = now;
        
        // Operación atómica de strings
        prepareTextData(_currentIdx);
        
        _displayState = START_RENDER;
      }
      break;
      
    case START_RENDER:
      _u8g2.firstPage();
      _displayState = RENDERING_PAGES;
      break;
      
    case RENDERING_PAGES:
      // Dibujamos usando la caché de texto precalculada
      _u8g2.setFont(u8g2_font_9x15_tr); 
      _u8g2.drawStr(0, 15, _cachedLine1);    

      _u8g2.setFont(u8g2_font_9x15_tr);
      _u8g2.drawStr(0, 31, _cachedLine2);    

      // Transfiere solo una franja de la pantalla a la vez (~1.5ms)
      if (_u8g2.nextPage() == 0) {
        _displayState = OLED_IDLE;
        
        // Rotamos al siguiente esclavo del carrusel
        _currentIdx = (_currentIdx + 1) % _numSlaves;
      }
      break;
  }
}