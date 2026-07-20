#include "SDSystemConfig.h"
#include <CSV_Parser.h>
#include <cstring>

SystemConfigRaw SDgetSystemConfig(SDManager* _sd) { 
    SystemConfigRaw res; 
    
    // Inicializar todas las cadenas con terminador nulo para seguridad
    res.mac[0] = '\0';
    res.ip[0] = '\0';
    res.gateway[0] = '\0';
    res.subnet[0] = '\0';
    res.dns[0] = '\0';
    res.port[0] = '\0';
    res.baudrate[0] = '\0';
    res.txPin[0] = '\0';
    res.dePin[0] = '\0';
    res.rePin[0] = '\0';
    res.rtuConfig[0] = '\0';
    res.internalSlaveId[0] = '\0';

    if (!_sd->isReady()) {
        return res;
    }

    Serial.println("HASTA AQUI SABEMOS QUE SI LLEGO Y EL FALLO ES OTRO"); 

    // "sss" -> 3 columnas tipo string (Name; Value; Editable)
    // Header 'true' ignora la primera fila "Name;Value;Editable"
    CSV_Parser cp("sss", true, ';');

    // Leemos todo el archivo pasándole las líneas al parser
    _sd->withFile(PARAM_FILE, [](Stream& file, void* arg) {
        CSV_Parser* parser = (CSV_Parser*)arg;
        while (file.available()) {
            String line = file.readStringUntil('\n');
            line.trim(); // Elimina espacios en blanco o \r al final
            if (line.length() > 0) {
                line += "\n";
                *parser << line.c_str();
            }
        }
    }, &cp);

    // Mapeo de columnas según el esquema "sss"
    char **names  = (char**)cp[0]; // Columna 0: Name
    char **values = (char**)cp[1]; // Columna 1: Value

    int rowCount = cp.getRowsCount();

    if (names != nullptr && values != nullptr) {
        for (int i = 0; i < rowCount; i++) {
            // Ignorar filas donde Name o Value sean nulos
            if (!names[i] || !values[i]) continue;

            // Limpiar posibles espacios extra al inicio/final del nombre
            String keyStr = String(names[i]);
            keyStr.trim();
            const char* key = keyStr.c_str();

            // Comparar y asignar a la variable correspondiente de la estructura
            if (strcmp(key, "mac address") == 0) {
                strncpy(res.mac, values[i], MAX_TEXT_SIZE - 1);
            } else if (strcmp(key, "IP address") == 0) {
                strncpy(res.ip, values[i], MAX_TEXT_SIZE - 1);
            } else if (strcmp(key, "gateway") == 0) {
                strncpy(res.gateway, values[i], MAX_TEXT_SIZE - 1);
            } else if (strcmp(key, "subnet") == 0) {
                strncpy(res.subnet, values[i], MAX_TEXT_SIZE - 1);
            } else if (strcmp(key, "dns") == 0) {
                strncpy(res.dns, values[i], MAX_TEXT_SIZE - 1);
            } else if (strcmp(key, "port") == 0) {
                strncpy(res.port, values[i], MAX_TEXT_SIZE - 1);
            } else if (strcmp(key, "baudrate") == 0) {
                strncpy(res.baudrate, values[i], MAX_TEXT_SIZE - 1);
            } else if (strcmp(key, "txPin") == 0) {
                strncpy(res.txPin, values[i], MAX_TEXT_SIZE - 1);
            } else if (strcmp(key, "dePin") == 0) {
                strncpy(res.dePin, values[i], MAX_TEXT_SIZE - 1);
            } else if (strcmp(key, "rePin") == 0) {
                strncpy(res.rePin, values[i], MAX_TEXT_SIZE - 1);
            } else if (strcmp(key, "RTU Config") == 0) {
                strncpy(res.rtuConfig, values[i], MAX_TEXT_SIZE - 1);
            } else if (strcmp(key, "id") == 0) {
                strncpy(res.internalSlaveId, values[i], MAX_TEXT_SIZE - 1);
            }
        }
    }

    return res; 
}