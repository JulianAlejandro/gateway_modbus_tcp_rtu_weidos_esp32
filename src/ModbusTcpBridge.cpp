#include "ModbusTcpBridge.h"
#include "esp_log.h"

static const char* TAG = "MB_TCP_BRDG";

ModbusTcpBridge::ModbusTcpBridge(uint16_t port, IModbusClient* mbClient, IThreadLock* lock) 
    : _port(port), _ethernetServer(port), _mbClient(mbClient) {
        _lock = (lock != nullptr) ? lock : &_defaultLock; // si hay nullptr se usa el lock vacio
    }

void ModbusTcpBridge::begin(byte mac[], IPAddress ip, IPAddress dns, IPAddress gateway, IPAddress subnet) {
    Ethernet.init(ETHERNET_CS);
    Ethernet.begin(mac, ip, dns, gateway, subnet);
    _ethernetServer.begin(_port); 
}

void ModbusTcpBridge::process() {
    
    EthernetClient client = _ethernetServer.available();
    if (client) {
        //Serial.println("\n[Modbus TCP] ¡Cliente conectado!");
        ESP_LOGI(TAG, "\n[Modbus TCP] ¡Cliente conectado!");
        handleClient(client);
    }
}

void ModbusTcpBridge::handleClient(EthernetClient& client) { // funcion no bloqueante

    if (!client.available()) {
        return; 
    }

    int index = 0;
    while (client.available() && index < SIZE_MB_TCP_FRAME) {
        _modbusTcpBuffer[index++] = client.read();
        delayMicroseconds(100);
    }

    modbusStruct mbData;
            
    // 1. Validar el parseo. Si el buffer no es válido por FC no soportado, respondemos de inmediato
    if (!parseTCPBufferToStruct(_modbusTcpBuffer, &mbData)) {
        uint8_t fCode = _modbusTcpBuffer[7];
        if (fCode != 0) { // Evitamos ruidos extraños
            mbData.transactionID = (_modbusTcpBuffer[0] << 8) | _modbusTcpBuffer[1];
            mbData.slaveID = _modbusTcpBuffer[6];
            mbData.functionCode = fCode;
            sendTCPException(client, mbData, 0x01); // 0x01 = Illegal Function
        }
        //continue;
        return; 
    }
    ESP_LOGI(TAG, "Hay un commando a procesar"); 

    // 2. Procesamiento RTU delegando a la nueva función modular
    if(_tcpReqCallback){
        _tcpReqCallback(mbData); 
    }

    // en este callback damos la posibilidad de establecer un modbusClient
    if(_mbClient == nullptr){
        sendTCPException(client, mbData, 0x0A); // Gateway Path Unavailable 
        return; // no se avanza mas...no se establecio un Cliente 
    }
    
    _lock->lock(); 
    bool success = processCommand(mbData);

    // 3. Gestionar la Respuesta o la Excepción
    if (success) {
        ESP_LOGI(TAG, "Comando procesado, enviamos la respuesta: "); 
        sendTCPResponse(client, mbData);
    } else {
        int errNoCopy = errno; 
        uint8_t exceptionCode = 0x04; // Slave Device Failure

        ESP_LOGE(TAG, "Operación RTU fallida. Errno: %d, Texto: %s", errNoCopy, _mbClient->lastError());

        if (errNoCopy == ETIMEDOUT || errNoCopy == 110) { 
            exceptionCode = 0x0B; // el esclavo no esta respondiendo....gateway informa
        } else if (errNoCopy == EINVAL) {
            exceptionCode = 0x03; // Illegal Data Value
        } else if (errNoCopy == 112345680){ // ilegal data adress
            exceptionCode = 0x02; 
        }
        
        sendTCPException(client, mbData, exceptionCode);
    }     
    _lock->unlock();
}

bool ModbusTcpBridge::processCommand(const modbusStruct& mbData) {
    switch (mbData.functionCode) {
        case 0x05: { // Write Single Coil
            uint8_t coilValue = (mbData.quantity_value == 0xFF00) ? 1 : 0; 
            return _mbClient->coilWrite(mbData.slaveID, mbData.address, coilValue);
        }

        case 0x06: { // Write Single Register
            uint16_t registerValue = mbData.quantity_value;
            return _mbClient->holdingRegisterWrite(mbData.slaveID, mbData.address, registerValue);
        }

        case 0x0F: { // FC 15: Write Multiple Coils
            if (!_mbClient->beginTransmission(mbData.slaveID, COILS, mbData.address, mbData.quantity_value)) {
                return false;
            }
            int coilsWritten = 0;
            uint8_t byteCount = _modbusTcpBuffer[12];
            
            for (int i = 0; i < byteCount; i++) {
                uint8_t currentByte = _modbusTcpBuffer[13 + i];
                for (int bit = 0; bit < 8; bit++) {
                    if (coilsWritten < mbData.quantity_value) {
                        uint8_t bitValue = (currentByte >> bit) & 0x01;
                        _mbClient->write(bitValue);
                        coilsWritten++;
                    } else {
                        break;
                    }
                }
            }
            return _mbClient->endTransmission();
        }

        case 0x10: { // FC 16: Write Multiple Registers
            if (!_mbClient->beginTransmission(mbData.slaveID, HOLDING_REGISTERS, mbData.address, mbData.quantity_value)) {
                return false;
            }
            int tcpIndex = 13; 
            for (int i = 0; i < mbData.quantity_value; i++) {
                uint16_t registerValue = (_modbusTcpBuffer[tcpIndex] << 8) | _modbusTcpBuffer[tcpIndex + 1];
                _mbClient->write(registerValue);
                tcpIndex += 2; 
            }
            return _mbClient->endTransmission();
        }

        default: { // FCs de Lectura genéricos (0x01, 0x02, 0x03, 0x04, etc.)
            int dataType = getModbusClientDataType(mbData.functionCode); 
            return _mbClient->requestFrom(mbData.slaveID, dataType, mbData.address, mbData.quantity_value);
        }
    }
}

void ModbusTcpBridge::sendTCPResponse(EthernetClient& client, const modbusStruct& req) {

    // Casuística específica para FC 0x05, 0x06 y 0x0F y 0x10 (Respuestas tipo ECO)
    if (req.functionCode == 0x05 || req.functionCode == 0x06 ||  req.functionCode == 0x0F || req.functionCode == 0x10) { 
        uint16_t tcpLength = 6; 

        // 1. Enviar MBAP Header
        client.write((uint8_t)(req.transactionID >> 8));
        client.write((uint8_t)(req.transactionID & 0xFF));
        client.write((uint8_t)0);
        client.write((uint8_t)0);
        client.write(highByte(tcpLength));
        client.write(lowByte(tcpLength));
        client.write(req.slaveID);

        // 2. Enviar PDU (Eco exacto del comando recibido)
        client.write(req.functionCode);
        client.write(highByte(req.address));
        client.write(lowByte(req.address));
        client.write(highByte(req.quantity_value)); // Devuelve la cantidad de registros que se escribieron
        client.write(lowByte(req.quantity_value));
        
        return; // Salimos de la función inmediatamente
    }

    // --- Lógica existente para lecturas (0x01, 0x02, 0x03, 0x04) ---
    uint8_t byteCount = 0; 
    
    if (req.functionCode == 0x01 || req.functionCode == 0x02) { // COILS 
        byteCount = (req.quantity_value + 7) / 8;
    } else { // REGISTERS
        byteCount = req.quantity_value * 2; 
    }

    uint16_t tcpLength = 3 + byteCount;

    client.write((uint8_t)(req.transactionID >> 8));
    client.write((uint8_t)(req.transactionID & 0xFF));
    client.write((uint8_t)0);
    client.write((uint8_t)0);
    client.write(highByte(tcpLength));
    client.write(lowByte(tcpLength));
    client.write(req.slaveID);

    client.write(req.functionCode);
    client.write(byteCount);

    if (req.functionCode == 0x01 || req.functionCode == 0x02) { 
        int coilsRead = 0;
        for (int i = 0; i < byteCount; i++) {
            uint8_t currentByte = 0;
            for (int bit = 0; bit < 8; bit++) {
                if (coilsRead < req.quantity_value) {
                    uint8_t bitValue = (uint8_t)_mbClient->read();
                    if (bitValue == 1) {
                        currentByte |= (1 << bit);
                    }
                    coilsRead++;
                } else {
                    break; 
                }
            }
            client.write(currentByte);
        }
    } else { 
        for (int i = 0; i < req.quantity_value; i++) {
            uint16_t valorRegistro = (uint16_t)_mbClient->read();

            if (_interceptor) { 
                _interceptor(req, i, valorRegistro);
            }

            client.write(highByte(valorRegistro));
            client.write(lowByte(valorRegistro));
        }
    }
}


void ModbusTcpBridge::sendTCPException(EthernetClient& client, const modbusStruct& req, uint8_t exceptionCode) {
    // En una excepción, la PDU mide exactamente 2 bytes: [Function Code + 0x80] + [Exception Code]
    // Sumando el Unit ID (1 byte), el campo de longitud del MBAP es 3.
    uint16_t tcpLength = 3;

    // 1. Enviar MBAP Header
    client.write((uint8_t)(req.transactionID >> 8));
    client.write((uint8_t)(req.transactionID & 0xFF));
    client.write((uint8_t)0);
    client.write((uint8_t)0);
    client.write(highByte(tcpLength));
    client.write(lowByte(tcpLength));
    client.write(req.slaveID);

    // 2. Enviar PDU de Excepción
    // El código de función se marca con el bit más significativo en 1 (FC + 0x80)
    client.write((uint8_t)(req.functionCode | 0x80)); 
    client.write(exceptionCode);
    
    ESP_LOGW(TAG, "Excepción enviada al cliente TCP. FC: 0x%02X, Code: 0x%02X", req.functionCode, exceptionCode);
}

bool ModbusTcpBridge::parseTCPBufferToStruct(const byte* tcp_buf, modbusStruct* out_struct) {
  if (out_struct == nullptr) return false;

  uint8_t fCode = tcp_buf[7]; 

  if (fCode != 0x01 && fCode !=0x02 && fCode != 0x03 && fCode != 0x04 && 
     fCode != 0x05 && fCode != 0x06 && fCode != 0x0F && fCode != 0x10) {
    return false;
  }

  out_struct->transactionID  = (tcp_buf[0] << 8) | tcp_buf[1];
  out_struct->protocolID     = (tcp_buf[2] << 8) | tcp_buf[3];
  out_struct->length         = (tcp_buf[4] << 8) | tcp_buf[5];
  out_struct->slaveID        = tcp_buf[6];
  out_struct->functionCode   = fCode;
  out_struct->address        = (tcp_buf[8] << 8) | tcp_buf[9];
  out_struct->quantity_value = (tcp_buf[10] << 8) | tcp_buf[11]; 
  
  return true;
}

void ModbusTcpBridge::setModbusClient(IModbusClient* mbClient){

    _mbClient = mbClient; 
}

void ModbusTcpBridge::setThreadLock(IThreadLock* lock){
    _lock = (lock != nullptr) ? lock : &_defaultLock; 
}

int ModbusTcpBridge::getModbusClientDataType(uint8_t functionCode) {
  switch (functionCode) {
    case 0x01: return COILS;
    case 0x02: return DISCRETE_INPUTS;
    case 0x03: return HOLDING_REGISTERS;
    case 0x04: return INPUT_REGISTERS;
    default:   return -1;
  }
}