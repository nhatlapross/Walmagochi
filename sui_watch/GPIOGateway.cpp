#include "GPIOGateway.h"
#include <WiFi.h>

// Constructor
GPIOGateway::GPIOGateway(int port) : server(port), running(false) {
}

// Initialize GPIO Gateway Server
void GPIOGateway::begin() {
    Serial.println("\n========================================");
    Serial.println("=== GPIO Gateway Initializing ===");
    Serial.println("========================================");

    // Setup routes
    server.on("/", HTTP_GET, [this]() { this->handleRoot(); });
    server.on("/gpio/status", HTTP_GET, [this]() { this->handleGPIOStatus(); });
    server.on("/gpio/read", HTTP_GET, [this]() { this->handleGPIORead(); });
    server.on("/gpio/write", HTTP_POST, [this]() { this->handleGPIOWrite(); });
    server.on("/gpio/mode", HTTP_POST, [this]() { this->handleGPIOMode(); });

    // CORS preflight
    server.on("/gpio/status", HTTP_OPTIONS, [this]() { this->handleCORS(); });
    server.on("/gpio/read", HTTP_OPTIONS, [this]() { this->handleCORS(); });
    server.on("/gpio/write", HTTP_OPTIONS, [this]() { this->handleCORS(); });
    server.on("/gpio/mode", HTTP_OPTIONS, [this]() { this->handleCORS(); });

    server.onNotFound([this]() { this->handleNotFound(); });

    // Start server
    server.begin();
    running = true;

    Serial.println("✓ GPIO Gateway started successfully!");
    Serial.print("  Server IP: ");
    Serial.println(WiFi.localIP());
    Serial.print("  Server Port: 8080");
    Serial.println("\n  Available endpoints:");
    Serial.println("    GET  http://" + WiFi.localIP().toString() + ":8080/");
    Serial.println("    GET  http://" + WiFi.localIP().toString() + ":8080/gpio/status");
    Serial.println("    GET  http://" + WiFi.localIP().toString() + ":8080/gpio/read?pin=15");
    Serial.println("    POST http://" + WiFi.localIP().toString() + ":8080/gpio/write");
    Serial.println("    POST http://" + WiFi.localIP().toString() + ":8080/gpio/mode");
    Serial.println("========================================\n");
}

// Handle client requests
void GPIOGateway::handleClient() {
    server.handleClient();
}

// Check if server is running
bool GPIOGateway::isRunning() {
    return running;
}

// Get server IP address
String GPIOGateway::getServerIP() {
    return WiFi.localIP().toString();
}

// Handle CORS preflight
void GPIOGateway::handleCORS() {
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.sendHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
    server.sendHeader("Access-Control-Allow-Headers", "Content-Type");
    server.send(200, "text/plain", "");
}

// Handle root - serve API info
void GPIOGateway::handleRoot() {
    String info = "ESP32 GPIO Gateway API\n\n";
    info += "Available Endpoints:\n";
    info += "  GET  /gpio/status       - Get all GPIO status\n";
    info += "  GET  /gpio/read?pin=X   - Read specific GPIO\n";
    info += "  POST /gpio/write        - Write GPIO {pin, value}\n";
    info += "  POST /gpio/mode         - Set pinMode {pin, mode}\n\n";
    info += "Available GPIO pins: 15, 16, 17, 18, 21, 33\n\n";
    info += "Example:\n";
    info += "  curl http://" + WiFi.localIP().toString() + ":8080/gpio/status\n";

    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.send(200, "text/plain", info);
}

// Handle GET /gpio/status - Get all GPIO status
void GPIOGateway::handleGPIOStatus() {
    StaticJsonDocument<1024> doc;
    JsonArray gpios = doc.createNestedArray("gpios");

    for (int i = 0; i < NUM_GPIOS; i++) {
        int pin = AVAILABLE_GPIOS[i];
        JsonObject gpio = gpios.createNestedObject();
        gpio["pin"] = pin;
        gpio["value"] = digitalRead(pin);
        gpio["mode"] = getPinModeString(pin);
    }

    doc["status"] = "success";
    doc["count"] = NUM_GPIOS;

    String response;
    serializeJson(doc, response);

    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.send(200, "application/json", response);
}

// Handle GET /gpio/read?pin=15
void GPIOGateway::handleGPIORead() {
    if (!server.hasArg("pin")) {
        sendJSON(400, "error", "Missing 'pin' parameter");
        return;
    }

    int pin = server.arg("pin").toInt();

    if (!isValidGPIO(pin)) {
        sendJSON(400, "error", "Invalid GPIO pin");
        return;
    }

    StaticJsonDocument<256> doc;
    doc["status"] = "success";
    doc["pin"] = pin;
    doc["value"] = digitalRead(pin);
    doc["mode"] = getPinModeString(pin);

    String response;
    serializeJson(doc, response);

    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.send(200, "application/json", response);
}

// Handle POST /gpio/write - Write GPIO value
void GPIOGateway::handleGPIOWrite() {
    if (server.method() != HTTP_POST) {
        sendJSON(405, "error", "Method not allowed");
        return;
    }

    StaticJsonDocument<256> doc;
    DeserializationError error = deserializeJson(doc, server.arg("plain"));

    if (error) {
        sendJSON(400, "error", "Invalid JSON");
        return;
    }

    if (!doc.containsKey("pin") || !doc.containsKey("value")) {
        sendJSON(400, "error", "Missing 'pin' or 'value' in request");
        return;
    }

    int pin = doc["pin"];
    int value = doc["value"];

    if (!isValidGPIO(pin)) {
        sendJSON(400, "error", "Invalid GPIO pin");
        return;
    }

    digitalWrite(pin, value);

    Serial.print("GPIO ");
    Serial.print(pin);
    Serial.print(" -> ");
    Serial.println(value ? "HIGH" : "LOW");

    StaticJsonDocument<256> response;
    response["status"] = "success";
    response["pin"] = pin;
    response["value"] = value;
    response["message"] = "GPIO written successfully";

    String responseStr;
    serializeJson(response, responseStr);

    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.send(200, "application/json", responseStr);
}

// Handle POST /gpio/mode - Set GPIO pinMode
void GPIOGateway::handleGPIOMode() {
    if (server.method() != HTTP_POST) {
        sendJSON(405, "error", "Method not allowed");
        return;
    }

    StaticJsonDocument<256> doc;
    DeserializationError error = deserializeJson(doc, server.arg("plain"));

    if (error) {
        sendJSON(400, "error", "Invalid JSON");
        return;
    }

    if (!doc.containsKey("pin") || !doc.containsKey("mode")) {
        sendJSON(400, "error", "Missing 'pin' or 'mode' in request");
        return;
    }

    int pin = doc["pin"];
    String mode = doc["mode"].as<String>();

    if (!isValidGPIO(pin)) {
        sendJSON(400, "error", "Invalid GPIO pin");
        return;
    }

    // Set pinMode
    if (mode == "INPUT") {
        pinMode(pin, INPUT);
    } else if (mode == "OUTPUT") {
        pinMode(pin, OUTPUT);
    } else if (mode == "INPUT_PULLUP") {
        pinMode(pin, INPUT_PULLUP);
    } else if (mode == "INPUT_PULLDOWN") {
        pinMode(pin, INPUT_PULLDOWN);
    } else {
        sendJSON(400, "error", "Invalid mode. Use INPUT, OUTPUT, INPUT_PULLUP, or INPUT_PULLDOWN");
        return;
    }

    Serial.print("GPIO ");
    Serial.print(pin);
    Serial.print(" mode -> ");
    Serial.println(mode);

    StaticJsonDocument<256> response;
    response["status"] = "success";
    response["pin"] = pin;
    response["mode"] = mode;
    response["message"] = "GPIO mode set successfully";

    String responseStr;
    serializeJson(response, responseStr);

    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.send(200, "application/json", responseStr);
}

// Handle 404
void GPIOGateway::handleNotFound() {
    sendJSON(404, "error", "Endpoint not found");
}

// Check if GPIO is valid
bool GPIOGateway::isValidGPIO(int pin) {
    for (int i = 0; i < NUM_GPIOS; i++) {
        if (AVAILABLE_GPIOS[i] == pin) {
            return true;
        }
    }
    return false;
}

// Get pinMode as string (simplified)
String GPIOGateway::getPinModeString(int pin) {
    // Note: ESP32 doesn't have a direct way to read pinMode
    // This is a simplified implementation
    // You may need to track pinMode states manually for accurate results
    return "UNKNOWN";
}

// Send JSON response
void GPIOGateway::sendJSON(int code, const char* status, const char* message) {
    StaticJsonDocument<256> doc;
    doc["status"] = status;
    doc["message"] = message;

    String response;
    serializeJson(doc, response);

    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.send(code, "application/json", response);
}
