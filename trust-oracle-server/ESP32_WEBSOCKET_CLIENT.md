# ESP32 WebSocket Client Example

Example code for ESP32 to connect to Trust Oracle Backend Server via WebSocket.

---

## 📋 Arduino Libraries Required

```cpp
#include <WiFi.h>
#include <WebSocketsClient.h>  // https://github.com/Links2004/arduinoWebSockets
#include <ArduinoJson.h>        // https://github.com/bblanchon/ArduinoJson
#include <MicroSui.h>           // For Ed25519 signing
```

Install via Arduino Library Manager or PlatformIO.

---

## 🔧 Complete Example Code

```cpp
/**
 * ESP32 Trust Oracle WebSocket Client
 * Connects to backend server and submits signed step data
 */

#include <WiFi.h>
#include <WebSocketsClient.h>
#include <ArduinoJson.h>
#include <MicroSui.h>

// WiFi credentials
const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";

// Server configuration
const char* server_host = "192.168.1.100";  // Your server IP
const uint16_t server_port = 8080;

// Device configuration
const char* DEVICE_ID = "esp32_device_001";

// WebSocket client
WebSocketsClient webSocket;

// MicroSui for Ed25519 signing
SuiKeypair suiKeypair;
String publicKeyHex;

// Connection state
bool authenticated = false;
unsigned long lastPing = 0;
const unsigned long PING_INTERVAL = 30000; // 30 seconds

// Step counter (simulated)
uint32_t totalSteps = 0;
float accSamples[30][3]; // Last 30 accelerometer samples

//==============================================
// Setup
//==============================================

void setup() {
    Serial.begin(115200);
    Serial.println("\n\n=== Trust Oracle ESP32 Client ===\n");

    // Generate Ed25519 keypair
    Serial.println("Generating Ed25519 keypair...");
    suiKeypair.generateKeypair();

    // Get public key in hex format
    uint8_t pubKey[32];
    suiKeypair.getPublicKey(pubKey);
    publicKeyHex = bytesToHex(pubKey, 32);

    Serial.println("Public Key: " + publicKeyHex);

    // Connect to WiFi
    connectWiFi();

    // Connect to WebSocket server
    connectWebSocket();
}

//==============================================
// Main Loop
//==============================================

void loop() {
    webSocket.loop();

    // Send periodic ping
    if (millis() - lastPing > PING_INTERVAL) {
        sendPing();
        lastPing = millis();
    }

    // Simulate step detection
    if (totalSteps < 10000) {
        totalSteps += random(10, 50);
        delay(5000); // Simulate every 5 seconds
    }
}

//==============================================
// WiFi Connection
//==============================================

void connectWiFi() {
    Serial.print("Connecting to WiFi");
    WiFi.begin(ssid, password);

    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }

    Serial.println("\nWiFi connected!");
    Serial.println("IP address: " + WiFi.localIP().toString());
}

//==============================================
// WebSocket Connection
//==============================================

void connectWebSocket() {
    Serial.println("\nConnecting to WebSocket server...");
    Serial.printf("  Host: %s:%d\n", server_host, server_port);

    webSocket.begin(server_host, server_port, "/");
    webSocket.onEvent(webSocketEvent);
    webSocket.setReconnectInterval(5000);
}

//==============================================
// WebSocket Event Handler
//==============================================

void webSocketEvent(WStype_t type, uint8_t * payload, size_t length) {
    switch(type) {
        case WStype_DISCONNECTED:
            Serial.println("[WebSocket] Disconnected");
            authenticated = false;
            break;

        case WStype_CONNECTED:
            Serial.println("[WebSocket] Connected!");
            registerDevice();
            break;

        case WStype_TEXT:
            handleMessage((char*)payload);
            break;

        case WStype_ERROR:
            Serial.printf("[WebSocket] Error: %s\n", (char*)payload);
            break;

        default:
            break;
    }
}

//==============================================
// Message Handlers
//==============================================

void handleMessage(const char* payload) {
    Serial.println("[WebSocket] Received: " + String(payload));

    StaticJsonDocument<2048> doc;
    DeserializationError error = deserializeJson(doc, payload);

    if (error) {
        Serial.println("JSON parse error!");
        return;
    }

    const char* type = doc["type"];

    if (strcmp(type, "welcome") == 0) {
        Serial.println("Server says: " + String(doc["message"].as<const char*>()));
    }
    else if (strcmp(type, "register_response") == 0) {
        handleRegisterResponse(doc);
    }
    else if (strcmp(type, "auth_response") == 0) {
        handleAuthResponse(doc);
    }
    else if (strcmp(type, "step_data_response") == 0) {
        handleStepDataResponse(doc);
    }
    else if (strcmp(type, "pong") == 0) {
        Serial.println("Pong received");
    }
}

void handleRegisterResponse(JsonDocument& doc) {
    bool success = doc["success"];

    if (success) {
        Serial.println("✓ Device registered successfully!");

        // Check if registered on blockchain
        if (doc.containsKey("blockchainResult")) {
            if (doc["blockchainResult"]["success"]) {
                String txDigest = doc["blockchainResult"]["txDigest"];
                String deviceObjectId = doc["blockchainResult"]["deviceObjectId"];

                Serial.println("✓ Registered on blockchain!");
                Serial.println("  TX: " + txDigest);
                Serial.println("  Object ID: " + deviceObjectId);
            }
        }

        // Now authenticate
        authenticateDevice();

    } else {
        Serial.println("✗ Registration failed: " + String(doc["error"].as<const char*>()));
    }
}

void handleAuthResponse(JsonDocument& doc) {
    bool success = doc["success"];

    if (success) {
        Serial.println("✓ Authenticated!");
        authenticated = true;

        // Start sending step data
        delay(2000);
        sendStepData();

    } else {
        Serial.println("✗ Authentication failed: " + String(doc["error"].as<const char*>()));
    }
}

void handleStepDataResponse(JsonDocument& doc) {
    bool success = doc["success"];

    if (success) {
        int dataId = doc["dataId"];
        int stepCount = doc["stepCount"];
        bool verified = doc["verified"];

        Serial.println("✓ Step data accepted!");
        Serial.printf("  Data ID: %d\n", dataId);
        Serial.printf("  Steps: %d\n", stepCount);
        Serial.printf("  Verified: %s\n", verified ? "YES" : "NO");

    } else {
        Serial.println("✗ Step data rejected: " + String(doc["error"].as<const char*>()));
    }
}

//==============================================
// Send Messages
//==============================================

void registerDevice() {
    Serial.println("\nRegistering device...");

    StaticJsonDocument<512> doc;
    doc["type"] = "register";
    doc["deviceId"] = DEVICE_ID;
    doc["publicKey"] = publicKeyHex;

    String json;
    serializeJson(doc, json);

    webSocket.sendTXT(json);
}

void authenticateDevice() {
    Serial.println("\nAuthenticating...");

    StaticJsonDocument<256> doc;
    doc["type"] = "authenticate";
    doc["deviceId"] = DEVICE_ID;

    String json;
    serializeJson(doc, json);

    webSocket.sendTXT(json);
}

void sendStepData() {
    if (!authenticated) {
        Serial.println("Not authenticated, skipping step data");
        return;
    }

    Serial.println("\nSending step data...");

    // Build payload (without signature)
    StaticJsonDocument<2048> payloadDoc;
    payloadDoc["deviceId"] = DEVICE_ID;
    payloadDoc["stepCount"] = totalSteps;
    payloadDoc["timestamp"] = millis(); // Use actual RTC timestamp in production
    payloadDoc["firmwareVersion"] = 100;
    payloadDoc["batteryPercent"] = 85;

    // Add raw accelerometer samples
    JsonArray samples = payloadDoc.createNestedArray("rawAccSamples");
    for (int i = 0; i < 30; i++) {
        JsonArray sample = samples.createNestedArray();
        sample.add(accSamples[i][0]);
        sample.add(accSamples[i][1]);
        sample.add(accSamples[i][2]);
    }

    // Serialize to canonical JSON
    String canonicalJson;
    serializeJson(payloadDoc, canonicalJson);

    // Sign the payload
    String signature = signPayload(canonicalJson);

    // Build final message with signature
    StaticJsonDocument<2048> messageDoc;
    messageDoc["type"] = "step_data";
    messageDoc["stepCount"] = totalSteps;
    messageDoc["timestamp"] = millis();
    messageDoc["firmwareVersion"] = 100;
    messageDoc["batteryPercent"] = 85;

    JsonArray messageSamples = messageDoc.createNestedArray("rawAccSamples");
    for (int i = 0; i < 30; i++) {
        JsonArray sample = messageSamples.createNestedArray();
        sample.add(accSamples[i][0]);
        sample.add(accSamples[i][1]);
        sample.add(accSamples[i][2]);
    }

    messageDoc["signature"] = signature;

    String json;
    serializeJson(messageDoc, json);

    webSocket.sendTXT(json);
}

void sendPing() {
    StaticJsonDocument<64> doc;
    doc["type"] = "ping";

    String json;
    serializeJson(doc, json);

    webSocket.sendTXT(json);
}

//==============================================
// Signature Generation
//==============================================

String signPayload(const String& canonicalJson) {
    // Hash the canonical JSON with SHA256
    uint8_t hash[32];
    // Use your preferred SHA256 library
    // Example: mbedtls_sha256((uint8_t*)canonicalJson.c_str(), canonicalJson.length(), hash, 0);

    // Sign the hash with Ed25519
    uint8_t signature[64];
    suiKeypair.sign(hash, 32, signature);

    // Convert to hex string
    return bytesToHex(signature, 64);
}

//==============================================
// Utility Functions
//==============================================

String bytesToHex(const uint8_t* bytes, size_t len) {
    String hex = "0x";
    for (size_t i = 0; i < len; i++) {
        if (bytes[i] < 0x10) hex += "0";
        hex += String(bytes[i], HEX);
    }
    return hex;
}

//==============================================
// Simulated Step Detection
//==============================================

void simulateStepDetection() {
    // Simulate step counter update
    // In real implementation, read from QMI8658 IMU sensor

    totalSteps += 1;

    // Update accelerometer samples (simulated)
    for (int i = 29; i > 0; i--) {
        accSamples[i][0] = accSamples[i-1][0];
        accSamples[i][1] = accSamples[i-1][1];
        accSamples[i][2] = accSamples[i-1][2];
    }

    // Add new sample (simulated)
    accSamples[0][0] = random(-1000, 1000) / 10.0;
    accSamples[0][1] = random(-1000, 1000) / 10.0;
    accSamples[0][2] = random(-1000, 1000) / 10.0;

    // Send step data every 100 steps
    if (totalSteps % 100 == 0) {
        sendStepData();
    }
}
```

---

## 🧪 Testing Workflow

### 1. Setup Server
```bash
cd trust-oracle-server
npm install
cp .env.example .env
# Edit .env with your configuration
npm start
```

### 2. Upload ESP32 Code
```cpp
// Update these in the code:
const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";
const char* server_host = "192.168.1.100";  // Your server IP
```

### 3. Monitor Serial Output
```
=== Trust Oracle ESP32 Client ===

Generating Ed25519 keypair...
Public Key: 0x0102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f20

Connecting to WiFi........
WiFi connected!
IP address: 192.168.1.150

Connecting to WebSocket server...
  Host: 192.168.1.100:8080

[WebSocket] Connected!
Server says: Connected to Trust Oracle Server

Registering device...
✓ Device registered successfully!
✓ Registered on blockchain!
  TX: 9R8e35P6LGsRnHtLBGabfAHWTPAETRFbauQzX3YoTwKJ
  Object ID: 0xf6a0dcdbfd25662e14742f5197e8aa8f9d83eb837e0fac63238dd93e6b447789

Authenticating...
✓ Authenticated!

Sending step data...
✓ Step data accepted!
  Data ID: 1
  Steps: 450
  Verified: YES
```

---

## 📝 Integration Notes

1. **Store Device Object ID**: After blockchain registration, save `deviceObjectId` to NVS for future reference

2. **Implement SHA256**: Use `mbedtls` or similar library for hashing

3. **Real IMU Data**: Replace simulated accelerometer data with actual QMI8658 readings

4. **NTP Time Sync**: Use real timestamp from NTP instead of `millis()`

5. **Error Handling**: Add retry logic for failed submissions

6. **Battery Monitoring**: Read actual battery percentage from ADC

7. **Offline Queue**: Store failed submissions for retry when reconnected

---

## 🔐 Security Best Practices

1. **Never Log Private Key**: Only log public key

2. **Secure Storage**: Store private key in NVS with encryption

3. **Validate Responses**: Always check `success` field before proceeding

4. **Connection Security**: Use WSS (WebSocket Secure) in production

5. **Rate Limiting**: Don't spam the server with too frequent submissions

---

## 🚀 Production Checklist

- [ ] Replace simulated step detection with QMI8658 IMU
- [ ] Implement proper SHA256 hashing
- [ ] Add NTP time synchronization
- [ ] Implement offline data queue
- [ ] Add proper error handling and retry logic
- [ ] Use secure WebSocket (WSS) connection
- [ ] Store device object ID in NVS
- [ ] Add battery monitoring
- [ ] Implement sleep mode for power saving
- [ ] Add OTA update capability

---

**Last Updated**: 2025-01-19
**Compatible with**: trust-oracle-server v1.0.0
