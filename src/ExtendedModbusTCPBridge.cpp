#include "ExtendedModbusTCPBridge.h"

ExtendedModbusTCPBridge::ExtendedModbusTCPBridge(uint16_t port, ModbusRTUClientManager* rtuModule)
    : ModbusTcpBridge(port, rtuModule) {} // Llama al constructor base

void ExtendedModbusTCPBridge::setHardwareMutex(SemaphoreHandle_t rtuMutex) {
    _rtuMutex = rtuMutex;
}

void ExtendedModbusTCPBridge::setInterceptor(ModbusInterceptorCallback callback) {
    _interceptor = callback;
}

void ExtendedModbusTCPBridge::handleClient(EthernetClient& client) {
    while (client.connected()) {
        if (client.available()) {
            int index = 0;
            while (client.available() && index < SIZE_MB_TCP_REQUEST) {
                _tcpRequestBuffer[index++] = client.read();
                delayMicroseconds(100);
            }

            modbusTCPStruct req;
            if (parseTCPBufferToStruct(_tcpRequestBuffer, &req)) {
                
                // --- Lógica del Mutex añadida de forma limpia en la clase hija ---
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
            }
        }
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}

void ExtendedModbusTCPBridge::sendTCPResponse(EthernetClient& client, const modbusTCPStruct& req) {
    uint8_t byteCount = req.quantity * 2; 
    uint16_t tcpLength = 3 + byteCount;

    // Cabecera MBAP
    client.write((uint8_t)(req.transactionID >> 8));
    client.write((uint8_t)(req.transactionID & 0xFF));
    client.write((uint8_t)0);
    client.write((uint8_t)0);
    client.write(highByte(tcpLength));
    client.write(lowByte(tcpLength));
    client.write(req.slaveID);

    // PDU
    client.write(req.functionCode);
    client.write(byteCount);

    for (int i = 0; i < req.quantity; i++) {
        uint16_t valorRegistro = (uint16_t)_rtu->readRegister();

        // --- Lógica del Interceptor añadida aquí ---
        if (_interceptor != nullptr) {
            _interceptor(req, i, valorRegistro); 
        }

        client.write(highByte(valorRegistro));
        client.write(lowByte(valorRegistro));
    }
}