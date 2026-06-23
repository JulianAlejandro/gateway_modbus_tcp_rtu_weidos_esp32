

#include "ModbusTCPModule.h"


ModbusTCPModule::ModbusTCPModule(uint16_t port, ModbusRTUModule* rtuModule ) 
    : _port(port), _server(port), _rtu(rtuModule) {}

void ModbusTCPModule::setInterceptor(ModbusInterceptorCallback callback){
    _interceptor = callback; 
}

void ModbusTCPModule::begin(byte mac[], IPAddress ip) {
    Ethernet.init(ETHERNET_CS);
    Ethernet.begin(mac, ip);
    _server.begin(_port);
}

void ModbusTCPModule::setHardwareMutex(SemaphoreHandle_t rtuMutex) {
    _rtuMutex = rtuMutex; // Guardamos el candado real para usarlo en handleClient
}

void ModbusTCPModule::process() {
    EthernetClient client = _server.available();
    if (client) {
        Serial.println("\n[Modbus TCP] ¡Cliente conectado!");
        handleClient(client);
    }
}


void ModbusTCPModule::handleClient(EthernetClient& client) {
    while (client.connected()) {
        if (client.available()) {
            int index = 0;
            while (client.available() && index < SIZE_MB_TCP_REQUEST) {
                _tcpRequestBuffer[index++] = client.read();
                delayMicroseconds(100);
            }

            modbusTCPStruct req;
            if (parseTCPBufferToStruct(_tcpRequestBuffer, &req)) {
                
                // Usamos nuestro mutex interno inyectado de forma segura
                if (xSemaphoreTake(_rtuMutex, pdMS_TO_TICKS(500)) == pdTRUE) {
                    
                    if (_rtu->readFromSlave(req)) {
                        sendTCPResponse(client, req);
                    }else{
                        // TODO
                    }
                    
                    xSemaphoreGive(_rtuMutex);
                }
            }
        }
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}

void ModbusTCPModule::sendTCPResponse(EthernetClient& client, const modbusTCPStruct& req) {
    uint8_t byteCount = req.quantity * 2; 
    uint16_t tcpLength = 3 + byteCount;

    // MBAP Header
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

    // Volcar los registros leídos al vuelo desde el módulo RTU hacia el socket TCP
    for (int i = 0; i < req.quantity; i++) {
        uint16_t valorRegistro = (uint16_t)_rtu->readRegister();

        if(_interceptor != nullptr){
            _interceptor(req, i, valorRegistro); 
        }

        client.write(highByte(valorRegistro));
        client.write(lowByte(valorRegistro));
    }
}

