/**
 * Trust Oracle Client for ESP32
 * Handles WebSocket communication with Trust Oracle backend
 * Implements Ed25519 signing for step data using MicroSui
 */

#ifndef TRUST_ORACLE_CLIENT_H
#define TRUST_ORACLE_CLIENT_H

#include <Arduino.h>
#include <WebSocketsClient.h>
#include <ArduinoJson.h>
#include <MicroSui.h>  // MicroSui library (includes Keypair and compact_ed25519)
#include <Preferences.h>  // ESP32 NVS for persistent keypair storage

class TrustOracleClient {
public:
    TrustOracleClient(const char* host, uint16_t port, const char* deviceId, const char* privateKeyHex = nullptr);

    // Lifecycle
    void begin();
    void loop();
    void disconnect();

    // Connection status
    bool isConnected();
    bool isRegistered();
    bool isAuthenticated();

    // Step data submission
    bool submitStepData(int stepCount, unsigned long timestamp,
                       int batteryPercent, float accSamples[][3], int sampleCount);

    // Virtual Pet sync
    bool syncPet(const String& petJson);
    bool claimResources(int steps);  // Claim food/energy from steps
    bool feedPet();                   // Feed pet (uses 1 food)
    bool playWithPet();               // Play with pet (uses 1 energy)
    void requestPetData();

    // Status
    String getStatus();
    String getLastError();

private:
    // Configuration
    const char* _host;
    uint16_t _port;
    String _deviceId;
    const char* _privateKeyHex;  // Optional pre-configured private key

    // WebSocket
    WebSocketsClient _webSocket;
    bool _connected;
    bool _registered;
    bool _authenticated;

    // Ed25519 Keypair (MicroSui)
    MicroSuiEd25519 _keypair;
    String _publicKeyHex;

    // Status
    String _status;
    String _lastError;
    unsigned long _lastPingTime;
    const unsigned long PING_INTERVAL = 30000; // 30s

    // WebSocket event handler
    static void webSocketEvent(WStype_t type, uint8_t* payload, size_t length);
    static TrustOracleClient* _instance; // For static callback

    void handleMessage(const char* payload);
    void handleWelcome(JsonDocument& doc);
    void handleRegisterResponse(JsonDocument& doc);
    void handleAuthResponse(JsonDocument& doc);
    void handleStepDataResponse(JsonDocument& doc);
    void handlePong(JsonDocument& doc);
    void handleError(JsonDocument& doc);
    void handlePetData(JsonDocument& doc);

    // Message sending
    void sendRegister();
    void sendAuthenticate();
    void sendPing();

    // Signing (using MicroSui)
    String signPayload(JsonDocument& payload);
    String buildCanonicalJSON(JsonDocument& obj);
    String bytesToHex(const uint8_t* bytes, size_t len);

    // Keypair persistence
    bool loadKeypairFromFlash();
    void saveKeypairToFlash();
};

#endif
