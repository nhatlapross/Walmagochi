#ifndef GPIO_GATEWAY_H
#define GPIO_GATEWAY_H

#include <Arduino.h>
#include <WebServer.h>
#include <ArduinoJson.h>

// Available GPIO pins on P2 header (excluding power/ground)
const int AVAILABLE_GPIOS[] = {15, 16, 17, 18, 21, 33};
const int NUM_GPIOS = 6;

// GPIO Gateway class
class GPIOGateway {
public:
    GPIOGateway(int port = 8080);
    void begin();
    void handleClient();
    bool isRunning();
    String getServerIP();

private:
    WebServer server;
    bool running;

    // Request handlers
    void handleRoot();
    void handleGPIOStatus();
    void handleGPIORead();
    void handleGPIOWrite();
    void handleGPIOMode();
    void handleNotFound();
    void handleCORS();

    // Helper functions
    bool isValidGPIO(int pin);
    String getPinModeString(int pin);
    void sendJSON(int code, const char* status, const char* message);
};

#endif // GPIO_GATEWAY_H
