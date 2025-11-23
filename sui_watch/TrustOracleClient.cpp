/**
 * Trust Oracle Client Implementation
 * Uses MicroSui for Ed25519 signing
 */

#include "TrustOracleClient.h"
#include "LoadingOverlay.h"
#include <mbedtls/sha256.h>

// Static instance for callback
TrustOracleClient* TrustOracleClient::_instance = nullptr;

// External loading overlay from ui_handlers.cpp
extern LoadingOverlay loadingOverlay;

TrustOracleClient::TrustOracleClient(const char* host, uint16_t port, const char* deviceId, const char* privateKeyHex)
    : _host(host), _port(port), _deviceId(deviceId), _privateKeyHex(privateKeyHex),
      _connected(false), _registered(false), _authenticated(false),
      _lastPingTime(0) {
    _instance = this;
    _status = "Initializing";
}

void TrustOracleClient::begin() {
    Serial.println("\n=== Trust Oracle Client ===");

    bool keypairLoaded = false;

    // Check if private key is provided (supports both hex and bech32)
    if (_privateKeyHex && strlen(_privateKeyHex) > 0) {
        Serial.println("Loading keypair from private key...");

        // MicroSui library handles both hex and bech32 format automatically
        _keypair = SuiKeypair_fromSecretKey(_privateKeyHex);

        // Get public key and address
        const uint8_t* pubKey = _keypair.getPublicKey(&_keypair);
        _publicKeyHex = bytesToHex(pubKey, 32);

        Serial.println("✓ Keypair loaded successfully");
        Serial.print("  Address: ");
        Serial.println(_keypair.toSuiAddress(&_keypair));

        keypairLoaded = true;
    }

    // Try to load from flash if not loaded from config
    if (!keypairLoaded && loadKeypairFromFlash()) {
        Serial.println("✓ Using existing keypair from flash");
        keypairLoaded = true;
    }

    // Generate new keypair if still not loaded
    if (!keypairLoaded) {
        Serial.println("Generating new Ed25519 keypair...");
        _keypair = SuiKeypair_generate((uint8_t)random(256));

        const uint8_t* pubKey = _keypair.getPublicKey(&_keypair);
        _publicKeyHex = bytesToHex(pubKey, 32);

        // Save to flash
        saveKeypairToFlash();
        Serial.println("✓ New keypair generated and saved to flash");
        Serial.println("✓ Copy this private key (hex format) to code:");
        Serial.println("  " + bytesToHex(_keypair.secret_key, 32));
    }

    Serial.println("Device ID: " + _deviceId);
    Serial.println("Public Key: 0x" + _publicKeyHex);

    // Connect to WebSocket
    Serial.printf("Connecting to %s:%d\n", _host, _port);
    _webSocket.begin(_host, _port, "/");
    _webSocket.onEvent(webSocketEvent);
    _webSocket.setReconnectInterval(5000);

    _status = "Connecting";
}

void TrustOracleClient::loop() {
    _webSocket.loop();

    // Send periodic ping
    if (_connected && _authenticated && (millis() - _lastPingTime > PING_INTERVAL)) {
        sendPing();
        _lastPingTime = millis();
    }
}

void TrustOracleClient::disconnect() {
    _webSocket.disconnect();
    _connected = false;
    _registered = false;
    _authenticated = false;
}

bool TrustOracleClient::isConnected() {
    return _connected;
}

bool TrustOracleClient::isRegistered() {
    return _registered;
}

bool TrustOracleClient::isAuthenticated() {
    return _authenticated;
}

String TrustOracleClient::getStatus() {
    return _status;
}

String TrustOracleClient::getLastError() {
    return _lastError;
}

// WebSocket event handler (static)
void TrustOracleClient::webSocketEvent(WStype_t type, uint8_t* payload, size_t length) {
    if (!_instance) return;

    switch(type) {
        case WStype_DISCONNECTED:
            Serial.println("[WS] Disconnected!");
            _instance->_connected = false;
            _instance->_registered = false;
            _instance->_authenticated = false;
            _instance->_status = "Disconnected";
            break;

        case WStype_CONNECTED:
            Serial.println("[WS] Connected!");
            _instance->_connected = true;
            _instance->_status = "Connected";
            break;

        case WStype_TEXT:
            _instance->handleMessage((char*)payload);
            break;

        case WStype_ERROR:
            Serial.println("[WS] Error!");
            _instance->_lastError = "WebSocket error";
            break;

        default:
            break;
    }
}

void TrustOracleClient::handleMessage(const char* payload) {
    // Debug: Print raw message
    Serial.println("📨 Received message:");
    Serial.println(payload);

    StaticJsonDocument<2048> doc;
    DeserializationError error = deserializeJson(doc, payload);

    if (error) {
        Serial.print("JSON parse error: ");
        Serial.println(error.c_str());
        return;
    }

    const char* type = doc["type"];
    if (!type) {
        Serial.println("⚠️ Message has no 'type' field");
        return;
    }

    Serial.print("📋 Message type: ");
    Serial.println(type);

    if (strcmp(type, "welcome") == 0) {
        handleWelcome(doc);
    } else if (strcmp(type, "register_response") == 0) {
        handleRegisterResponse(doc);
    } else if (strcmp(type, "auth_response") == 0) {
        handleAuthResponse(doc);
    } else if (strcmp(type, "step_data_response") == 0) {
        handleStepDataResponse(doc);
    } else if (strcmp(type, "pong") == 0) {
        handlePong(doc);
    } else if (strcmp(type, "error") == 0) {
        handleError(doc);
    } else if (strcmp(type, "pet_data") == 0) {
        handlePetData(doc);
    } else if (strcmp(type, "pet_error") == 0) {
        Serial.println("❌ Pet error received:");
        const char* error = doc["error"];
        if (error) {
            Serial.printf("   %s\n", error);
        }
        _lastError = doc["error"].as<String>();
        // Hide loading overlay on error
        loadingOverlay.hide();
    } else if (strcmp(type, "pet_fed") == 0) {
        Serial.println("✓ Pet fed successfully on blockchain");
        // Hide loading overlay after successful feed
        loadingOverlay.hide();
    } else if (strcmp(type, "pet_played") == 0) {
        Serial.println("✓ Pet played successfully on blockchain");
        // Hide loading overlay after successful play
        loadingOverlay.hide();
    } else if (strcmp(type, "resources_claimed") == 0) {
        Serial.println("✓ Resources claimed successfully on blockchain");
        // Hide loading overlay after successful claim
        loadingOverlay.hide();
    } else {
        Serial.print("⚠️ Unknown message type: ");
        Serial.println(type);
    }
}

void TrustOracleClient::handleWelcome(JsonDocument& doc) {
    Serial.println("✓ Server welcome");
    _status = "Registering";
    sendRegister();
}

void TrustOracleClient::handleRegisterResponse(JsonDocument& doc) {
    bool success = doc["success"];
    if (success) {
        Serial.println("✓ Device registered!");
        const char* txDigest = doc["txDigest"];
        if (txDigest) {
            Serial.print("✓ Blockchain TX: ");
            Serial.println(txDigest);
        }
        _registered = true;
        _status = "Registered";

        // Auto-authenticate
        sendAuthenticate();
    } else {
        Serial.print("✗ Registration failed: ");
        Serial.println(doc["message"].as<const char*>());
        _lastError = doc["message"].as<String>();
    }
}

void TrustOracleClient::handleAuthResponse(JsonDocument& doc) {
    bool success = doc["success"];
    if (success) {
        Serial.println("✓ Authenticated!");
        _authenticated = true;
        _status = "Ready";
        _lastPingTime = millis();

        // Request pet data to get pet object ID
        Serial.println("Requesting pet data...");
        requestPetData();
    } else {
        Serial.print("✗ Authentication failed: ");
        Serial.println(doc["message"].as<const char*>());
        _lastError = doc["message"].as<String>();
    }
}

void TrustOracleClient::handleStepDataResponse(JsonDocument& doc) {
    bool success = doc["success"];
    if (success) {
        Serial.println("✓ Step data accepted!");
        Serial.print("  Data ID: ");
        Serial.println(doc["dataId"].as<int>());
        Serial.print("  Steps: ");
        Serial.println(doc["stepCount"].as<int>());
        Serial.print("  Verified: ");
        Serial.println(doc["verified"].as<bool>() ? "YES" : "NO");
    } else {
        Serial.print("✗ Step data rejected: ");
        Serial.println(doc["message"].as<const char*>());
        _lastError = doc["message"].as<String>();
    }
}

void TrustOracleClient::handlePong(JsonDocument& doc) {
    // Keep-alive successful
}

void TrustOracleClient::handleError(JsonDocument& doc) {
    Serial.print("✗ Server error: ");
    Serial.println(doc["message"].as<const char*>());
    _lastError = doc["message"].as<String>();
}

void TrustOracleClient::handlePetData(JsonDocument& doc) {
    Serial.println("🐾 Handling pet_data message...");

    bool success = doc["success"];
    Serial.printf("Success: %s\n", success ? "true" : "false");

    if (success) {
        JsonObject pet = doc["pet"];
        if (pet) {
            Serial.println("✓ Pet data received");

            // Debug: Print all pet fields
            Serial.println("Pet fields:");
            Serial.printf("  pet_name: %s\n", pet["pet_name"].as<const char*>());
            Serial.printf("  device_id: %s\n", pet["device_id"].as<const char*>());
            Serial.printf("  food: %d\n", pet["food"].as<int>());
            Serial.printf("  energy: %d\n", pet["energy"].as<int>());

            // Extract pet object ID if available
            const char* petObjIdStr = pet["pet_object_id"];
            Serial.printf("  pet_object_id: %s\n", petObjIdStr ? petObjIdStr : "NULL");

            if (petObjIdStr && strlen(petObjIdStr) > 0) {
                // Update global petObjectId variable
                extern String petObjectId;
                petObjectId = String(petObjIdStr);

                Serial.print("✓ Pet NFT Object ID: ");
                Serial.println(petObjectId.c_str());

                // Check if pet is on-chain
                bool onChain = pet["on_chain"];
                if (onChain) {
                    Serial.println("✓ Pet is registered on Sui blockchain");
                } else {
                    Serial.println("ℹ Pet is not yet on blockchain");
                }
            } else {
                Serial.println("⚠️ Pet has no object ID - not on blockchain yet");
            }
        } else {
            Serial.println("❌ Pet object is null");
        }
    } else {
        Serial.println("❌ Pet data request failed");
        const char* error = doc["error"];
        if (error) {
            Serial.printf("Error: %s\n", error);
        }
    }
}

void TrustOracleClient::sendRegister() {
    StaticJsonDocument<512> doc;
    doc["type"] = "register";
    doc["deviceId"] = _deviceId;
    doc["publicKey"] = "0x" + _publicKeyHex;

    String json;
    serializeJson(doc, json);

    Serial.println("Sending registration...");
    _webSocket.sendTXT(json);
}

void TrustOracleClient::sendAuthenticate() {
    StaticJsonDocument<256> doc;
    doc["type"] = "authenticate";
    doc["deviceId"] = _deviceId;

    String json;
    serializeJson(doc, json);

    Serial.println("Sending authentication...");
    _webSocket.sendTXT(json);
}

void TrustOracleClient::sendPing() {
    StaticJsonDocument<128> doc;
    doc["type"] = "ping";

    String json;
    serializeJson(doc, json);
    _webSocket.sendTXT(json);
}

bool TrustOracleClient::submitStepData(int stepCount, unsigned long timestamp,
                                       int batteryPercent, float accSamples[][3], int sampleCount) {
    if (!_authenticated) {
        _lastError = "Not authenticated";
        return false;
    }

    Serial.println("\n=== Submitting to Oracle ===");
    Serial.printf("Submitting step data (%d steps)...\n", stepCount);

    // Build payload (data to be signed)
    StaticJsonDocument<2048> payloadDoc;
    payloadDoc["deviceId"] = _deviceId;
    payloadDoc["stepCount"] = stepCount;
    payloadDoc["timestamp"] = timestamp;
    payloadDoc["firmwareVersion"] = 100;
    payloadDoc["batteryPercent"] = batteryPercent;

    // Add accelerometer samples
    JsonArray samples = payloadDoc.createNestedArray("rawAccSamples");
    for (int i = 0; i < sampleCount && i < 10; i++) {
        JsonArray sample = samples.createNestedArray();
        sample.add(accSamples[i][0]);
        sample.add(accSamples[i][1]);
        sample.add(accSamples[i][2]);
    }

    // Sign the payload
    String signature = signPayload(payloadDoc);

    // Build message (includes payload + signature)
    StaticJsonDocument<2048> messageDoc;
    messageDoc["type"] = "step_data";
    messageDoc["deviceId"] = _deviceId;
    messageDoc["stepCount"] = stepCount;
    messageDoc["timestamp"] = timestamp;
    messageDoc["firmwareVersion"] = 100;
    messageDoc["batteryPercent"] = batteryPercent;
    messageDoc["rawAccSamples"] = payloadDoc["rawAccSamples"];
    messageDoc["signature"] = signature;

    String json;
    serializeJson(messageDoc, json);

    _webSocket.sendTXT(json);
    return true;
}

String TrustOracleClient::signPayload(JsonDocument& payload) {
    // 1. Build canonical JSON (sorted keys)
    String canonicalJson = buildCanonicalJSON(payload);

    Serial.println("Canonical JSON:");
    Serial.println(canonicalJson);

    // 2. SHA256 hash using mbedtls
    uint8_t hash[32];
    mbedtls_sha256_context ctx;
    mbedtls_sha256_init(&ctx);
    mbedtls_sha256_starts(&ctx, 0);  // 0 = SHA256 (not SHA224)
    mbedtls_sha256_update(&ctx, (const unsigned char*)canonicalJson.c_str(), canonicalJson.length());
    mbedtls_sha256_finish(&ctx, hash);
    mbedtls_sha256_free(&ctx);

    Serial.print("Hash: 0x");
    Serial.println(bytesToHex(hash, 32));

    // 3. Sign with Ed25519 using MicroSui
    uint8_t sui_sig[97];  // MicroSui signature format (1 byte scheme + 64 bytes sig + 32 bytes pubkey)
    int result = microsui_sign_ed25519(sui_sig, hash, 32, _keypair.secret_key);

    if (result != 0) {
        Serial.println("✗ Signing failed!");
        return "";
    }

    // Extract just the 64-byte signature (skip scheme byte at index 0)
    String signatureHex = bytesToHex(sui_sig + 1, 64);  // Ed25519 signature is 64 bytes

    Serial.print("  Signature: 0x");
    Serial.println(signatureHex);

    return signatureHex;
}

String TrustOracleClient::buildCanonicalJSON(JsonDocument& obj) {
    // Sort keys alphabetically and build JSON string
    // ArduinoJson maintains insertion order, so we need to rebuild sorted

    StaticJsonDocument<2048> sorted;

    // Extract and sort keys
    String keys[] = {"batteryPercent", "deviceId", "firmwareVersion", "rawAccSamples", "stepCount", "timestamp"};
    int keyCount = 6;

    // Add in sorted order
    for (int i = 0; i < keyCount; i++) {
        if (obj.containsKey(keys[i])) {
            sorted[keys[i]] = obj[keys[i]];
        }
    }

    String result;
    serializeJson(sorted, result);
    return result;
}

String TrustOracleClient::bytesToHex(const uint8_t* bytes, size_t len) {
    String hex = "";
    for (size_t i = 0; i < len; i++) {
        char buf[3];
        sprintf(buf, "%02x", bytes[i]);
        hex += buf;
    }
    return hex;
}

// ============================================
// Keypair Persistence (Save to Flash)
// ============================================

bool TrustOracleClient::loadKeypairFromFlash() {
    Preferences prefs;
    prefs.begin("oracle", true);  // Read-only mode

    // Check if keypair exists
    if (!prefs.isKey("secret_key")) {
        prefs.end();
        return false;
    }

    // Load secret key (32 bytes) into temporary buffer
    uint8_t secret_key[32];
    size_t len = prefs.getBytes("secret_key", secret_key, 32);
    prefs.end();

    if (len != 32) {
        Serial.println("✗ Invalid keypair in flash");
        return false;
    }

    // Create keypair from secret key (this properly initializes all function pointers)
    _keypair = SuiKeypair_fromSecretKey(bytesToHex(secret_key, 32).c_str());

    // Get public key from loaded keypair
    const uint8_t* pubKey = _keypair.getPublicKey(&_keypair);
    _publicKeyHex = bytesToHex(pubKey, 32);

    Serial.println("✓ Loaded keypair from flash");
    Serial.println("  Public Key: 0x" + _publicKeyHex);
    return true;
}

void TrustOracleClient::saveKeypairToFlash() {
    Preferences prefs;
    prefs.begin("oracle", false);  // Read-write mode

    // Save secret key (32 bytes)
    prefs.putBytes("secret_key", _keypair.secret_key, 32);
    prefs.end();

    Serial.println("✓ Saved keypair to flash");
}

// ============================================
// Virtual Pet Sync Functions
// ============================================

bool TrustOracleClient::syncPet(const String& petJson) {
    if (!_connected || !_authenticated) {
        Serial.println("✗ Not connected/authenticated");
        return false;
    }

    JsonDocument doc;
    deserializeJson(doc, petJson);

    JsonDocument message;
    message["type"] = "updatePet";
    message["deviceId"] = _deviceId;

    // Copy pet stats (including new resources)
    message["happiness"] = doc["happiness"];
    message["hunger"] = doc["hunger"];
    message["health"] = doc["health"];
    message["experience"] = doc["experience"];
    message["total_steps_fed"] = doc["totalStepsFed"];
    message["level"] = doc["level"];
    message["food"] = doc["food"];
    message["energy"] = doc["energy"];

    String msg;
    serializeJson(message, msg);
    _webSocket.sendTXT(msg);

    Serial.println("🐾 Pet sync sent to server");
    return true;
}

bool TrustOracleClient::claimResources(int steps) {
    if (!_connected || !_authenticated) {
        Serial.println("✗ Not connected/authenticated");
        return false;
    }

    JsonDocument message;
    message["type"] = "claimResources";
    message["deviceId"] = _deviceId;
    message["steps"] = steps;

    String msg;
    serializeJson(message, msg);
    _webSocket.sendTXT(msg);

    Serial.printf("💰 Claim resources request sent (%d steps)\n", steps);
    return true;
}

bool TrustOracleClient::feedPet() {
    if (!_connected || !_authenticated) {
        Serial.println("✗ Not connected/authenticated");
        return false;
    }

    JsonDocument message;
    message["type"] = "feedPet";
    message["deviceId"] = _deviceId;

    String msg;
    serializeJson(message, msg);
    _webSocket.sendTXT(msg);

    Serial.println("🍔 Feed pet request sent (uses 1 food)");
    return true;
}

bool TrustOracleClient::playWithPet() {
    if (!_connected || !_authenticated) {
        Serial.println("✗ Not connected/authenticated");
        return false;
    }

    JsonDocument message;
    message["type"] = "playWithPet";
    message["deviceId"] = _deviceId;

    String msg;
    serializeJson(message, msg);
    _webSocket.sendTXT(msg);

    Serial.println("🎮 Play with pet request sent (uses 1 energy)");
    return true;
}

void TrustOracleClient::requestPetData() {
    if (!_connected || !_authenticated) {
        Serial.println("✗ Not connected/authenticated");
        return;
    }

    JsonDocument message;
    message["type"] = "getPet";
    message["deviceId"] = _deviceId;

    String msg;
    serializeJson(message, msg);
    _webSocket.sendTXT(msg);

    Serial.println("📡 Requesting pet data from server");
}
