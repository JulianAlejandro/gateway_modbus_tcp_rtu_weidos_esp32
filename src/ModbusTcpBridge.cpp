

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

bool ModbusTcpBridge::parseTCPBufferToStruct(const byte* tcp_buf, modbusTCPStruct* out_struct) {
  if (out_struct == nullptr) return false;

  uint8_t fCode = tcp_buf[7];
  if (fCode != 0x03 && fCode != 0x04) {
    out_struct->isValid = false;
    return false;
  }

  out_struct->transactionID = (tcp_buf[0] << 8) | tcp_buf[1];
  out_struct->protocolID    = (tcp_buf[2] << 8) | tcp_buf[3];
  out_struct->length        = (tcp_buf[4] << 8) | tcp_buf[5];
  out_struct->slaveID       = tcp_buf[6];
  out_struct->functionCode  = fCode;
  out_struct->address       = (tcp_buf[8] << 8) | tcp_buf[9];
  out_struct->quantity      = (tcp_buf[10] << 8) | tcp_buf[11];
  out_struct->isValid       = true;

  return true;
}
