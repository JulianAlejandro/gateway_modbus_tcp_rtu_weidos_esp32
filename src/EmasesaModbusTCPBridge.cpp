/*
#include "EmasesaModbusTcpBridge.h"

EmasesaModbusTcpBridge::EmasesaModbusTcpBridge(uint16_t port, ModbusRTUClientManager* rtuModule)
    : ModbusTcpBridge(port, rtuModule) {} // Llama al constructor base

void EmasesaModbusTcpBridge::setHardwareMutex(SemaphoreHandle_t rtuMutex) {
    _rtuMutex = rtuMutex;
}

void EmasesaModbusTcpBridge::setInterceptor(ModbusInterceptorCallback callback) {
    _interceptor = callback;
}

void EmasesaModbusTcpBridge::handleClient(EthernetClient& client) {
    while (client.connected()) {
        if (client.available()) {
            int index = 0;
            while (client.available() && index < SIZE_MB_TCP_REQUEST) {
                _tcpRequestBuffer[index++] = client.read();
                delayMicroseconds(100);
            }

            modbusTCPStruct req;
            if (parseTCPBufferToStruct(_tcpRequestBuffer, &req)) {

                //if(callback(req)){
                
                    // --- Lógica del Mutex añadida de forma limpia en la clase hija ---
                    // en esta clase especifica , se usa el _rtuMutex siendo compartido con otro hilo externo. 
                    bool hasMutex = true;
                    if (_rtuMutex != nullptr) {
                        hasMutex = (xSemaphoreTake(_rtuMutex, pdMS_TO_TICKS(500)) == pdTRUE);
                    }

                    if (hasMutex) {
                        if (_rtu->readFromSlave(req)) {
                            sendTCPResponse(client, req); // Llamará al sendTCPResponse de esta clase hija
                        }else{
                            // todo.
                        }
                    
                        if (_rtuMutex != nullptr) {
                            xSemaphoreGive(_rtuMutex);
                        }
                    }

                //}else{

                //}
            }
        }
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}

void EmasesaModbusTcpBridge::sendTCPResponse(EthernetClient& client, const modbusTCPStruct& req) {

    uint8_t byteCount = 0; 
    
    // 1. Calcular cuántos bytes de datos reales vamos a enviar
    if (req.functionCode == 0x01 || req.functionCode == 0x02) {
        // Para Coils: 1 byte por cada 8 bits (redondeado hacia arriba)
        byteCount = (req.quantity + 7) / 8;
        //ESP_LOGI(TAG, "function code 0x01 y 0x02 byteCount %u \n", byteCount); 
    } else {
        // Para Registros (0x03 / 0x04): 2 bytes por registro
        byteCount = req.quantity * 2; 
    }

    // Longitud total restante para el MBAP header (Unit ID + Function Code + Byte Count + Datos)
    uint16_t tcpLength = 3 + byteCount;

    // 2. Enviar MBAP Header
    client.write((uint8_t)(req.transactionID >> 8));
    client.write((uint8_t)(req.transactionID & 0xFF));
    client.write((uint8_t)0);
    client.write((uint8_t)0);
    client.write(highByte(tcpLength));
    client.write(lowByte(tcpLength));
    client.write(req.slaveID);

    // 3. Enviar PDU básica
    client.write(req.functionCode);
    client.write(byteCount);

    // 4. Volcar los datos leyendo desde el búfer de la librería RTU
    if (req.functionCode == 0x01 || req.functionCode == 0x02) { 

        int coilsRead = 0;
        // Iteramos sobre la cantidad de bytes que debemos construir y enviar
        for (int i = 0; i < byteCount; i++) {
            uint8_t currentByte = 0;

            // Rellenamos el byte actual bit por bit (hasta 8 bits o hasta terminar los solicitados)
            for (int bit = 0; bit < 8; bit++) {
                if (coilsRead < req.quantity) {
                    // Leemos el bit individual desde el cliente RTU (devuelve 0 o 1)
                    uint8_t bitValue = (uint8_t)_rtu->readRegister();
                    
                    // Modbus manda el primer coil en el bit menos significativo (LSB) del byte
                    if (bitValue == 1) {
                        currentByte |= (1 << bit);
                    }
                    coilsRead++;
                } else {
                    // Si ya leímos todos los coils solicitados pero el byte no se ha llenado (8 bits),
                    // los bits restantes se quedan en 0 por norma de Modbus.
                    break; 
                }
            }
            // Enviamos el byte empaquetado al cliente TCP
            client.write(currentByte);
        }
    } else { // esto es en caso de que llegue un function code 03 o 04 
        // Lógica existente para Registros (16 bits)
        for (int i = 0; i < req.quantity; i++) {
            uint16_t valorRegistro = (uint16_t)_rtu->readRegister();

            
            if (_interceptor != nullptr) {
                _interceptor(req, i, valorRegistro); 
             }

            client.write(highByte(valorRegistro));
            client.write(lowByte(valorRegistro));
        }
    }
}

*/