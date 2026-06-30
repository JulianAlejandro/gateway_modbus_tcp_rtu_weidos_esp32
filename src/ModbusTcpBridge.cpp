

#include "ModbusTcpBridge.h"


ModbusTcpBridge::ModbusTcpBridge(uint16_t port, ModbusRTUClientManager* rtuModule ) 
    : _port(port), _ethernetServer(port), _rtu(rtuModule) {}

void ModbusTcpBridge::begin(byte mac[], IPAddress ip) {
    Ethernet.init(ETHERNET_CS);
    Ethernet.begin(mac, ip);
    _ethernetServer.begin(_port); 
}

void ModbusTcpBridge::process() {
    EthernetClient client = _ethernetServer.available();
    if (client) {
        Serial.println("\n[Modbus TCP] ¡Cliente conectado!");
        handleClient(client);
    }
}

void ModbusTcpBridge::handleClient(EthernetClient& client) {
    while (client.connected()) {
        if (client.available()) {
            int index = 0;
            while (client.available() && index < SIZE_MB_TCP_REQUEST) {
                _tcpRequestBuffer[index++] = client.read();
                delayMicroseconds(100);
            }

            modbusTCPStruct req;
            if (parseTCPBufferToStruct(_tcpRequestBuffer, &req)) {
                                   
                if (_rtu->readFromSlave(req)) {
                        sendTCPResponse(client, req);
                }else{
                        // TODO.
                }      
            }
        }
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}

void ModbusTcpBridge::sendTCPResponse(EthernetClient& client, const modbusTCPStruct& req) {

    uint8_t byteCount = 0;

    // 1. Calcular cuántos bytes de datos reales vamos a enviar
    if (req.functionCode == 0x01 || req.functionCode == 0x02) {
        // Para Coils: 1 byte por cada 8 bits (redondeado hacia arriba)
        byteCount = (req.quantity + 7) / 8;
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
    } else {
        // Lógica existente para Registros (16 bits)
        for (int i = 0; i < req.quantity; i++) {
            uint16_t valorRegistro = (uint16_t)_rtu->readRegister();
            client.write(highByte(valorRegistro));
            client.write(lowByte(valorRegistro));
        }
    }

    /*
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
    */
}

bool ModbusTcpBridge::parseTCPBufferToStruct(const byte* tcp_buf, modbusTCPStruct* out_struct) {
  if (out_struct == nullptr) return false;

  uint8_t fCode = tcp_buf[7]; 

  if (fCode != 0x01 && fCode != 0x03 && fCode != 0x04) {
    out_struct->isValid = false;
    return false;
  }

  out_struct->transactionID = (tcp_buf[0] << 8) | tcp_buf[1];
  out_struct->protocolID    = (tcp_buf[2] << 8) | tcp_buf[3];
  out_struct->length        = (tcp_buf[4] << 8) | tcp_buf[5];
  out_struct->slaveID       = tcp_buf[6];
  out_struct->functionCode  = fCode;
  out_struct->address       = (tcp_buf[8] << 8) | tcp_buf[9];
  out_struct->quantity      = (tcp_buf[10] << 8) | tcp_buf[11]; // Aquí representa cuántas bobinas queremos leer
  out_struct->isValid       = true;

  return true;
}
