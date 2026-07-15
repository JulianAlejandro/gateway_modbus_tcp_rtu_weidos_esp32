#include "ModbusTcpBridge.h"
#include "esp_log.h"

static const char* TAG = "MB_TCP_BRDG";

/**
 * @brief Constructor for the Modbus TCP to RTU Bridge.
 * @param port TCP port to listen on (usually 502).
 * @param mbClient Pointer to the Modbus RTU client interface.
 * @param lock Optional thread lock interface for shared hardware resource safety.
 */
ModbusTcpBridge::ModbusTcpBridge(uint16_t port, IModbusClient* mbClient, IThreadLock* lock) 
    : _port(port), _ethernetServer(port), _mbClient(mbClient) {
        // Fallback to dummy lock if a nullptr is provided
        _lock = (lock != nullptr) ? lock : &_defaultLock; 
    }


/**
 * @brief Initializes the Ethernet hardware and starts the TCP server.
 */
void ModbusTcpBridge::begin(byte mac[], IPAddress ip, IPAddress dns, IPAddress gateway, IPAddress subnet) {
    Ethernet.init(ETHERNET_CS);
    Ethernet.begin(mac, ip, dns, gateway, subnet);
    _ethernetServer.begin(_port); 
}

/**
 * @brief Non-blocking poller to check for incoming TCP client connections.
 * This must be called repeatedly in the main loop.
 */
void ModbusTcpBridge::process() {
    EthernetClient client = _ethernetServer.available();
    if (client) {
        ESP_LOGI(TAG, "\n[Modbus TCP] Client connected!");
        handleClient(client);
    }
}

/**
 * @brief Dynamically reads a full Modbus TCP frame from an Ethernet client.
 * * Parses the 7-byte MBAP header on-the-fly to calculate the exact frame 
 * size, avoiding partial reads and preventing buffer overflows.
 * * @param client Active Ethernet client.
 * @param maxBufferSize Maximum capacity of the destination buffer.
 * @param out_buffer Output byte array to store the frame.
 * @return true If the complete frame was successfully read.
 * @return false On timeout or if the frame exceeds maxBufferSize.
 */
bool ModbusTcpBridge::getModbusTcpBuffer(EthernetClient& client, size_t maxBufferSize, byte* out_buffer){
    
    int index = 0;
    uint32_t startTime = millis();
    const uint32_t TIMEOUT_MS = 50; 

    // Intentamos leer al menos la cabecera MBAP (7 bytes) para saber el tamaño real
    int bytesToRead = maxBufferSize; // Por defecto o máximo

    while ((millis() - startTime < TIMEOUT_MS) && (index < bytesToRead)) {
        if (client.available()) {
            out_buffer[index++] = client.read();
            
            // Re-ajustar el tiempo de timeout cada vez que llega un byte
            startTime = millis(); 

            // Dinámico: Si ya leímos los primeros 7 bytes (cabecera MBAP), 
            // calculamos el tamaño exacto de la trama Modbus TCP
            if (index == 7) {
                // El campo Length está en los bytes 4 y 5 de la cabecera (0-indexed)
                uint16_t modbusLength = ((uint16_t)out_buffer[4] << 8) | out_buffer[5];
                // El total de la trama es: 6 bytes (Transaction ID + Protocol ID) + el valor de Length
                bytesToRead = 6 + modbusLength;
                
                // Protección contra desbordamiento de buffer local
                if (bytesToRead > maxBufferSize) {
                    ESP_LOGE(TAG, "Trama Modbus excede el tamaño del buffer");
                    //client.flush(); // Limpiar lo que quede
                    return false; 
                }
            }
        } else {
            // Si no hay datos en este instante, cedemos un poco de tiempo al sistema
            delayMicroseconds(50); 
        }
    }

    // --- Verificación Post-Bucle ---
    if (index < bytesToRead) {
        // Si salimos por TIMEOUT y no completamos la trama esperada
        ESP_LOGW(TAG, "Timeout alcanzado. Trama incompleta (%d/%d bytes). Descartando.", index, bytesToRead);
        // Aquí SÍ descartas la trama porque el cliente falló en transmitir a tiempo
        return false; 
    }

    // Si llegamos aquí, la trama está completa en _modbusTcpBuffer
    ESP_LOGI(TAG, "Trama Modbus TCP recibida con éxito (%d bytes)", index);
    return true; 
}

/**
 * @brief Handles the lifecycle of a connected Modbus TCP client request.
 * Parses the frame, validates it, forwards to RTU, and sends back responses/exceptions.
 */
void ModbusTcpBridge::handleClient(EthernetClient& client) { // funcion no bloqueante

    if (!client.available()) {
        return; 
    }

    memset(_modbusTcpBuffer, 0, SIZE_MB_TCP_FRAME);

    /*
    // version basica de obtencion de datos
    int index = 0; // Todo muy basica, 
    while (client.available() && index < SIZE_MB_TCP_FRAME) {
        _modbusTcpBuffer[index++] = client.read();
        delayMicroseconds(100);
    }
   */
    
    if(!getModbusTcpBuffer(client, SIZE_MB_TCP_FRAME, _modbusTcpBuffer)){
        client.stop();
        return; // de momento machacamos no devolvemos nada
    }
    
    modbusStruct mbData;
            
    // 1. Validate and parse the TCP buffer. Send immediate exception if Function Code is unsupported.
    if (!parseTCPBufferToStruct(_modbusTcpBuffer, &mbData)) {
        uint8_t fCode = _modbusTcpBuffer[7];
        if (fCode != 0) { // Avoid noise
            mbData.transactionID = (_modbusTcpBuffer[0] << 8) | _modbusTcpBuffer[1];
            mbData.slaveID = _modbusTcpBuffer[6];
            mbData.functionCode = fCode;
            sendTCPException(client, mbData, 0x01); // 0x01 = Illegal Function
        }
        //continue;
        return; 
    }
    ESP_LOGI(TAG, "Valid Modbus TCP command received"); 

    // 2. Execute request callback if registered
    if(_tcpReqCallback){
        _tcpReqCallback(mbData); 
    }

    // Check if the RTU client backend is available
    if(_mbClient == nullptr){
        sendTCPException(client, mbData, 0x0A); // Gateway Path Unavailable 
        return; 
    }
    // Thread-safe block to protect the shared physical HW of the client bus (RS485)
    _lock->lock(); 
    bool success = processCommand(mbData);

    // 3. Handle response routing or exception mapping based on RTU results
    if (success) {
        ESP_LOGI(TAG, "Command processed successfully. Sending TCP response.");
        sendTCPResponse(client, mbData);
    } else {
        int errNoCopy = errno; 
        uint8_t exceptionCode = SLAVE_DEVICE_FAILURE; // Default: Slave Device Failure

        ESP_LOGE(TAG, "RTU operation failed. Errno: %d, Text: %s", errNoCopy, _mbClient->lastError());

        // Map RTU/System errors to standard Modbus TCP exception codes
        if (errNoCopy == ETIMEDOUT || errNoCopy == 110) { 
            exceptionCode = 0x0B; // Gateway Target Device Failed to Respond
        } else if (errNoCopy == EINVAL) {
            exceptionCode = ILEGAL_DATA_VALUE; // Illegal Data Value
        } else if (errNoCopy == CODE_ILEGAL_ADDRES){ 
            exceptionCode = 0x02; // Illegal Data Address
        }
        
        sendTCPException(client, mbData, exceptionCode);
    }     
    _lock->unlock();
}

/**
 * @brief Routes the Modbus request to the physical RTU hardware client.
 * @return true if the RTU transaction succeeded, false otherwise.
 */
/**
 * @brief Routes the Modbus request to the physical RTU hardware client.
 * @return true if the RTU transaction succeeded, false otherwise.
 */
bool ModbusTcpBridge::processCommand(const modbusStruct& mbData) {
    switch (mbData.functionCode) {
        case 0x05: { // Write Single Coil
            uint8_t coilValue = (mbData.quantity_value == 0xFF00) ? 1 : 0; 
            // coilWrite devuelve 1 (éxito) o 0 (fallo)
            return (_mbClient->coilWrite(mbData.slaveID, mbData.address, coilValue) == 1);
        }

        case 0x06: { // Write Single Register
            uint16_t registerValue = mbData.quantity_value;
            // holdingRegisterWrite devuelve 1 (éxito) o 0 (fallo)
            return (_mbClient->holdingRegisterWrite(mbData.slaveID, mbData.address, registerValue) == 1);
        }

        case 0x0F: { // FC 15: Write Multiple Coils
            // beginTransmission devuelve 1 (éxito) o 0 (fallo)
            if (_mbClient->beginTransmission(mbData.slaveID, COILS, mbData.address, mbData.quantity_value) != 1) {
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
            // endTransmission devuelve 1 (éxito) o 0 (fallo)
            return (_mbClient->endTransmission() == 1);
        }

        case 0x10: { // FC 16: Write Multiple Registers
            // beginTransmission devuelve 1 (éxito) o 0 (fallo)
            if (_mbClient->beginTransmission(mbData.slaveID, HOLDING_REGISTERS, mbData.address, mbData.quantity_value) != 1) {
                return false;
            }
            int tcpIndex = 13; // Payload data starts at index 13
            for (int i = 0; i < mbData.quantity_value; i++) {
                uint16_t registerValue = (_modbusTcpBuffer[tcpIndex] << 8) | _modbusTcpBuffer[tcpIndex + 1];
                _mbClient->write(registerValue);
                tcpIndex += 2; 
            }
            // endTransmission devuelve 1 (éxito) o 0 (fallo)
            return (_mbClient->endTransmission() == 1);
        }

        default: { // Generic Read Functions (0x01, 0x02, 0x03, 0x04)
            int dataType = getModbusClientDataType(mbData.functionCode); 
            // requestFrom devuelve la cantidad de registros leídos en caso de éxito (> 0) o 0 si falla.
            int result = _mbClient->requestFrom(mbData.slaveID, dataType, mbData.address, mbData.quantity_value);
            return (result > 0);
        }
    }
}

/**
 * @brief Constructs and sends a standard Modbus TCP response frame back to the client.
 */
/**
 * @brief Constructs and sends a standard Modbus TCP response frame back to the client.
 */
void ModbusTcpBridge::sendTCPResponse(EthernetClient& client, const modbusStruct& req) {

    // Write commands (FC 05, 06, 15, 16) return an echo response
    if (req.functionCode == 0x05 || req.functionCode == 0x06 || req.functionCode == 0x0F || req.functionCode == 0x10) { 
        uint16_t tcpLength = 6; // Unit ID (1) + FC (1) + Address (2) + Value/Quant (2)

        // 1. Send MBAP Header
        client.write((uint8_t)(req.transactionID >> 8));
        client.write((uint8_t)(req.transactionID & 0xFF));
        client.write((uint8_t)0); // Protocol ID high
        client.write((uint8_t)0); // Protocol ID low
        client.write(highByte(tcpLength));
        client.write(lowByte(tcpLength));
        client.write(req.slaveID);

        // 2. Send PDU Echo
        client.write(req.functionCode);
        client.write(highByte(req.address));
        client.write(lowByte(req.address));
        client.write(highByte(req.quantity_value)); 
        client.write(lowByte(req.quantity_value));
        
        return; 
    }

    // --- Processing Read Commands (FC 01, 02, 03, 04) ---
    uint8_t byteCount = 0; 
    
    if (req.functionCode == 0x01 || req.functionCode == 0x02) { // Coils/Discrete Inputs
        byteCount = (req.quantity_value + 7) / 8;
    } else { // Holding/Input Registers
        byteCount = req.quantity_value * 2; 
    }

    uint16_t tcpLength = 3 + byteCount; // UnitID (1) + FC (1) + ByteCount (1) + Data(N)

    // Send MBAP Header
    client.write((uint8_t)(req.transactionID >> 8));
    client.write((uint8_t)(req.transactionID & 0xFF));
    client.write((uint8_t)0);
    client.write((uint8_t)0);
    client.write(highByte(tcpLength));
    client.write(lowByte(tcpLength));
    client.write(req.slaveID);
    
    // Send PDU Header
    client.write(req.functionCode);
    client.write(byteCount);

    // Stream payload data from RTU buffer to TCP stream
    if (req.functionCode == 0x01 || req.functionCode == 0x02) { 
        int coilsRead = 0;
        for (int i = 0; i < byteCount; i++) {
            uint8_t currentByte = 0;
            for (int bit = 0; bit < 8; bit++) {
                if (coilsRead < req.quantity_value) {
                    long rawValue = _mbClient->read();
                    if (rawValue != -1) { // Comprobación de seguridad contra fin de buffer o error
                        uint8_t bitValue = (uint8_t)rawValue;
                        if (bitValue == 1) {
                            currentByte |= (1 << bit);
                        }
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
            long rawValue = _mbClient->read();
            uint16_t valorRegistro = 0;

            if (rawValue != -1) {
                valorRegistro = (uint16_t)rawValue;
            } else {
                ESP_LOGE(TAG, "Read buffer underflow at register index: %d", i);
            }

            // Trigger interceptor callback if registered (allows on-the-fly modifications)
            if (_interceptor) { 
                _interceptor(req, i, valorRegistro);
            }

            client.write(highByte(valorRegistro));
            client.write(lowByte(valorRegistro));
        }
    }
}


/**
 * @brief Sends a Modbus TCP Exception response.
 * @param exceptionCode Standard Modbus exception identifier.
 */
void ModbusTcpBridge::sendTCPException(EthernetClient& client, const modbusStruct& req, uint8_t exceptionCode) {
    uint16_t tcpLength = 3; // Unit ID (1) + Exception FC (1) + Exception Code (1)

    // 1. Send MBAP Header
    client.write((uint8_t)(req.transactionID >> 8));
    client.write((uint8_t)(req.transactionID & 0xFF));
    client.write((uint8_t)0);
    client.write((uint8_t)0);
    client.write(highByte(tcpLength));
    client.write(lowByte(tcpLength));
    client.write(req.slaveID);

    // 2. Send Exception PDU (Function Code | 0x80)
    client.write((uint8_t)(req.functionCode | 0x80)); 
    client.write(exceptionCode);
    
    ESP_LOGW(TAG, "TCP Exception sent. FC: 0x%02X, Code: 0x%02X", req.functionCode, exceptionCode);
}


/**
 * @brief Parses a raw Modbus TCP binary array into a structured format.
 * @return true if function code is supported, false otherwise.
 */
bool ModbusTcpBridge::parseTCPBufferToStruct(const byte* tcp_buf, modbusStruct* out_struct) {
  if (out_struct == nullptr) return false;

  uint8_t fCode = tcp_buf[7]; 

  // Validate supported function codes
  if (fCode != 0x01 && fCode != 0x02 && fCode != 0x03 && fCode != 0x04 && 
      fCode != 0x05 && fCode != 0x06 && fCode != 0x0F && fCode != 0x10) {
    return false;
  }

  // Extract fields from raw MBAP and PDU frames (Big-Endian conversion)
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

/**
 * @brief Maps standard Modbus Function Codes to client data type constants.
 */
int ModbusTcpBridge::getModbusClientDataType(uint8_t functionCode) {
  switch (functionCode) {
    case 0x01: return COILS;
    case 0x02: return DISCRETE_INPUTS;
    case 0x03: return HOLDING_REGISTERS;
    case 0x04: return INPUT_REGISTERS;
    default:   return -1;
  }
}