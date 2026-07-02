

#include "ModbusTcpBridge.h"
#include "esp_log.h"

static const char* TAG = "MB_TCP_BRDG";


ModbusTcpBridge::ModbusTcpBridge(uint16_t port, ModbusRTUClientClass* rtuClient ) 
    : _port(port), _ethernetServer(port), _rtuClient(rtuClient) {}

void ModbusTcpBridge::begin(byte mac[], IPAddress ip) {
    Ethernet.init(ETHERNET_CS);
    Ethernet.begin(mac, ip);
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


void ModbusTcpBridge::handleClient(EthernetClient& client) {
    while (client.connected()) { // TODO analizar y arreglar en un futuro: este while en caso de que el cliente externo mantenga la conexion abierta por error genera problemas en IDLE , watchdog timeout. 
        if (client.available()) {
            int index = 0;
            while (client.available() && index < SIZE_MB_TCP_REQUEST) {
                _tcpRequestBuffer[index++] = client.read();
                delayMicroseconds(100);
            }

            modbusTCPStruct req;
            
            // 1. Validar el parseo. Si el buffer no es válido por FC no soportado, respondemos de inmediato
            if (!parseTCPBufferToStruct(_tcpRequestBuffer, &req)) {
                // Si al menos pudimos extraer los datos del MBAP básicos, enviamos una excepción 0x01 (Illegal Function)
                // Nota: Asegúrate de que parseTCPBufferToStruct llene transactionID y slaveID incluso si el FC es incorrecto.
                uint8_t fCode = _tcpRequestBuffer[7];
                if (fCode != 0) { // Evitamos ruidos extraños
                    req.transactionID = (_tcpRequestBuffer[0] << 8) | _tcpRequestBuffer[1];
                    req.slaveID = _tcpRequestBuffer[6];
                    req.functionCode = fCode;
                    sendTCPException(client, req, 0x01); // 0x01 = Illegal Function
                }
                continue;
            }

            bool rtuSuccess = false;

// --- PROCESAMIENTO DIRECTO ---
            if (req.functionCode == 0x0F) { // FC 15: Write Multiple Coils
                if (_rtuClient->beginTransmission(req.slaveID, COILS, req.address, req.quantity_value)) {
                    int coilsWritten = 0;
                    uint8_t byteCount = _tcpRequestBuffer[12];
                    
                    for (int i = 0; i < byteCount; i++) {
                        uint8_t currentByte = _tcpRequestBuffer[13 + i];
                        for (int bit = 0; bit < 8; bit++) {
                            if (coilsWritten < req.quantity_value) {
                                uint8_t bitValue = (currentByte >> bit) & 0x01;
                                _rtuClient->write(bitValue);
                                coilsWritten++;
                            } else {
                                break;
                            }
                        }
                    }
                    rtuSuccess = _rtuClient->endTransmission();
                } else {
                    rtuSuccess = false;
                }

            } else if (req.functionCode == 0x10) { // <<--- FC 16: NUEVO BLOQUE!
                if (_rtuClient->beginTransmission(req.slaveID, HOLDING_REGISTERS, req.address, req.quantity_value)) {
                    int tcpIndex = 13; // Los datos de los registros inician en el byte 13
                    
                    for (int i = 0; i < req.quantity_value; i++) {
                        // Unimos los dos bytes consecutivos para formar el valor de 16 bits
                        uint16_t registerValue = (_tcpRequestBuffer[tcpIndex] << 8) | _tcpRequestBuffer[tcpIndex + 1];
                        
                        _rtuClient->write(registerValue);
                        tcpIndex += 2; // Avanzamos 2 bytes para apuntar al siguiente registro
                    }
                    rtuSuccess = _rtuClient->endTransmission(); // Calcula CRC, commuta pin RS485 y envía de golpe
                } else {
                    rtuSuccess = false;
                }

            } else if (req.functionCode == 0x06) {
                uint16_t registerValue = req.quantity_value;
                rtuSuccess = _rtuClient->holdingRegisterWrite(req.slaveID, req.address, registerValue);

            } else if (req.functionCode == 0x05) {
                uint8_t coilValue = (req.quantity_value == 0xFF00) ? 1 : 0; 
                rtuSuccess = _rtuClient->coilWrite(req.slaveID, req.address, coilValue);
            } else {
                int dataType = -1;
                if (req.functionCode == 0x01) dataType = COILS;
                else if (req.functionCode == 0x02) dataType = DISCRETE_INPUTS;
                else if (req.functionCode == 0x03) dataType = HOLDING_REGISTERS;
                else if (req.functionCode == 0x04) dataType = INPUT_REGISTERS;

                rtuSuccess = _rtuClient->requestFrom(req.slaveID, dataType, req.address, req.quantity_value);
            }

            // 2. Gestionar la Respuesta o la Excepción
            if (rtuSuccess) {
                sendTCPResponse(client, req);
            } else {
                // La operación falló. Analizamos 'errno' para saber qué pasó en el bus RTU
                int errNoCopy = errno; 
                uint8_t exceptionCode = 0x04; // Por defecto: Slave Device Failure (0x04)

                ESP_LOGE(TAG, "Operación RTU fallida. Errno: %d, Texto: %s", errNoCopy, _rtuClient->lastError());

                // Mapeo de errores de libmodbus a excepciones Modbus reales
                // Nota: Dependiendo de tu entorno de compilación, libmodbus define constantes como EMBTIMEDOUT.
                // Si no las tienes accesibles, puedes evaluar los números de error directamente o usar 0x0A de forma genérica para timeouts.
                if (errNoCopy == ETIMEDOUT || errNoCopy == 110) { // 110 suele ser ETIMEDOUT en sistemas embebidos
                    exceptionCode = 0x0A; // Gateway Path Unavailable (El esclavo RTU no respondió)
                } else if (errNoCopy == EINVAL) {
                    exceptionCode = 0x03; // Illegal Data Value
                }
                
                // Enviamos la respuesta de error al cliente TCP para que no se quede colgado
                sendTCPException(client, req, exceptionCode);
            }      
        }
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}


void ModbusTcpBridge::sendTCPResponse(EthernetClient& client, const modbusTCPStruct& req) {


    // Casuística específica para FC 0x05, 0x06 y 0x0F (Respuestas tipo ECO)
if (req.functionCode == 0x05 || req.functionCode == 0x06 || 
        req.functionCode == 0x0F || req.functionCode == 0x10) { // <-- Añadido el 0x10 aquí
        
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
    
    if (req.functionCode == 0x01 || req.functionCode == 0x02) {
        byteCount = (req.quantity_value + 7) / 8;
    } else {
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
                    uint8_t bitValue = (uint8_t)_rtuClient->read();
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
            uint16_t valorRegistro = (uint16_t)_rtuClient->read();
            client.write(highByte(valorRegistro));
            client.write(lowByte(valorRegistro));
        }
    }
}


void ModbusTcpBridge::sendTCPException(EthernetClient& client, const modbusTCPStruct& req, uint8_t exceptionCode) {
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

bool ModbusTcpBridge::parseTCPBufferToStruct(const byte* tcp_buf, modbusTCPStruct* out_struct) {
  if (out_struct == nullptr) return false;

  uint8_t fCode = tcp_buf[7]; 

  if (fCode != 0x01 && fCode !=0x02 && fCode != 0x03 && fCode != 0x04 && 
     fCode != 0x05 && fCode != 0x06 && fCode != 0x0F && fCode != 0x10) {
    out_struct->isValid = false;
    return false;
  }

  ESP_LOGI(TAG, "Fcode: %u: ", fCode); 

  out_struct->transactionID  = (tcp_buf[0] << 8) | tcp_buf[1];
  out_struct->protocolID     = (tcp_buf[2] << 8) | tcp_buf[3];
  out_struct->length         = (tcp_buf[4] << 8) | tcp_buf[5];
  out_struct->slaveID        = tcp_buf[6];
  out_struct->functionCode   = fCode;
  out_struct->address        = (tcp_buf[8] << 8) | tcp_buf[9];
  out_struct->quantity_value = (tcp_buf[10] << 8) | tcp_buf[11]; 
  out_struct->isValid        = true;

  return true;
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