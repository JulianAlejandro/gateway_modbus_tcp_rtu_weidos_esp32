

#include "ModbusTCPModule.h"


ModbusTCPModule::ModbusTCPModule(uint16_t port, ModbusRTUModule* rtuModule) 
    : _port(port), _server(port), _rtu(rtuModule) {}

void ModbusTCPModule::begin(byte mac[], IPAddress ip) {
    Ethernet.init(ETHERNET_CS);
    Ethernet.begin(mac, ip);
    _server.begin(_port);
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
            
            // Leer la ráfaga TCP entrante
            while (client.available() && index < SIZE_MB_TCP_REQUEST) {
                _tcpRequestBuffer[index++] = client.read();
                delayMicroseconds(100);
            }

            modbusTCPStruct req;
            if (parseTCPBufferToStruct(_tcpRequestBuffer, &req)) {
                // Delegamos la lectura física al módulo RTU
                if (_rtu->readFromSlave(req)) {
                    Serial.println("[ÉXITO] Datos del esclavo obtenidos por RTU. Respondiendo por TCP...");
                    sendTCPResponse(client, req);
                } else {
                    Serial.print("[ERROR] Falló la lectura RTU. Código: ");
                    Serial.println(_rtu->getLastError());
                }
            }
        }
        yield(); // Cuidado de watchdog en ESP32
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
        client.write(highByte(valorRegistro));
        client.write(lowByte(valorRegistro));
    }
}

