/**
 * SUI Watch - Trust Oracle Edition
 * Hardware Witness for Physical Activity Data
 *
 * Features:
 * - Step Counter with QMI8658 IMU
 * - WebSocket connection to Trust Oracle backend
 * - Ed25519 signing of step data
 * - Simple LCD UI
 */

#include <lvgl.h>
#include "lv_conf.h"
#include "LCD_1in28.h"
#include "DEV_Config.h"
#include "CST816S.h"
#include <WiFi.h>
#include <WiFiMulti.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <esp_timer.h>
#include "QMI8658.h"
#include "TrustOracleClient.h"
#include "VirtualPet.h"
#include "ui.h"  // SquareLine Studio UI

// WiFiMulti required by MicroSui
extern WiFiMulti WiFiMulti;

#define EXAMPLE_LVGL_TICK_PERIOD_MS 2

// WiFi Configuration
char WIFI_SSID[33] = "";
char WIFI_PASSWORD[65] = "";

// Trust Oracle Backend Configuration
const char* ORACLE_HOST = "";  // Your backend server IP
const uint16_t ORACLE_PORT = 8080;
const char* DEVICE_ID = "";  // Unique device ID

// Device Private Key - KEEP THIS SECRET!
// Supports both formats:
// - Hex: "a1b2c3d4..." (64 hex characters = 32 bytes)
// - Bech32: "suiprivkey1..." (Sui keytool format)
const char* DEVICE_PRIVATE_KEY = "";
const char* DEVICE_WALLET_ADDRESS = "";

// Sui RPC Configuration
const char* SUI_RPC_URL = "https://fullnode.testnet.sui.io";  // Testnet
// const char* SUI_RPC_URL = "https://fullnode.mainnet.sui.io";  // Mainnet

// Balance tracking
String suiBalance = "0.00";
unsigned long lastBalanceFetch = 0;
const unsigned long BALANCE_FETCH_INTERVAL = 30000;  // Fetch every 30 seconds

// Pet blockchain info
String petObjectId = "";  // NFT Pet Object ID on Sui blockchain

// Display config
static const uint16_t screenWidth = 240;
static const uint16_t screenHeight = 240;

static lv_disp_draw_buf_t draw_buf;
static lv_color_t buf[screenWidth * screenHeight / 10];

CST816S touch(6, 7, 13, 5);  // sda, scl, rst, irq
UWORD *BlackImage = NULL;  // Required by LCD_1in28.cpp but not used by LVGL

// Trust Oracle Client
TrustOracleClient* oracleClient = nullptr;

// Virtual Pet
VirtualPet virtualPet;
unsigned long lastPetUpdate = 0;
const unsigned long PET_UPDATE_INTERVAL = 5000;  // Update pet every 5 seconds

// Step counter variables
int stepCount = 0;
float lastAccMagnitude = 0;
float lastVerticalAcc = 0;
bool stepDetected = false;
bool stepPeakDetected = false;
unsigned long lastStepTime = 0;
const unsigned long stepCooldown = 400;  // 400ms between steps
const unsigned long stepMaxInterval = 2000;  // 2s max between steps
const float stepThresholdMin = 1.2;  // Minimum g-force change
const float stepThresholdMax = 3.0;  // Maximum g-force
const float deviceOrientationThreshold = 0.7;  // Device must be vertical/angled
bool imuInitialized = false;

// UI is now managed by SquareLine Studio (see ui.h)
// Access UI elements through: ui_Screen1, ui_Screen2, ui_Screen3, ui_Screen4
int currentScreen = 1;  // Start with Screen1 (pet screen)

// Data submission variables
unsigned long lastSubmissionTime = 0;
const unsigned long submissionInterval = 60000;  // Submit every 60 seconds
const int minStepsForSubmission = 10;  // Minimum steps before submitting

// Accelerometer sample buffer
float accSampleBuffer[30][3];  // Store last 30 samples
int accSampleIndex = 0;

// IMU reading throttle
unsigned long lastIMUReadTime = 0;
const unsigned long IMU_READ_INTERVAL = 50;  // Read IMU every 50ms (20Hz)

// ============================================
// Forward Declarations
// ============================================
// UI Functions from ui_handlers.cpp
// ============================================
void setupUIHandlers();
void updateScreen1PetUI();
void updateScreen2ResourcesUI();
void updateScreen3StepsUI();
void updateScreen4WalletUI();

// ============================================
// LVGL Display Driver
// ============================================

void my_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p) {
    uint32_t w = (area->x2 - area->x1 + 1);
    uint32_t h = (area->y2 - area->y1 + 1);

    // Copy LVGL buffer to BlackImage at correct position
    uint16_t *src = (uint16_t *)&color_p->full;

    for(uint32_t row = 0; row < h; row++) {
        uint32_t y = area->y1 + row;
        uint32_t dst_offset = area->x1 + y * 240;  // BlackImage is 240 width
        uint16_t *src_row = &src[row * w];

        // Copy and swap bytes for this row
        for(uint32_t col = 0; col < w; col++) {
            uint16_t color = src_row[col];
            BlackImage[dst_offset + col] = (color >> 8) | (color << 8);
        }
    }

    // Use LCD_1IN28_DisplayWindows with BlackImage buffer
    LCD_1IN28_DisplayWindows(area->x1, area->y1, area->x2 + 1, area->y2 + 1, BlackImage);

    lv_disp_flush_ready(disp);
}

void my_touchpad_read(lv_indev_drv_t *indev_driver, lv_indev_data_t *data) {
    bool touched = touch.available();
    if (touched) {
        data->point.x = touch.data.x;
        data->point.y = touch.data.y;
        data->state = LV_INDEV_STATE_PR;
    } else {
        data->state = LV_INDEV_STATE_REL;
    }
}

// LVGL tick timer callback
void example_increase_lvgl_tick(void *arg) {
    lv_tick_inc(EXAMPLE_LVGL_TICK_PERIOD_MS);
}

// ============================================
// UI Setup
// ============================================

void setupUI() {
    lv_init();
    lv_disp_draw_buf_init(&draw_buf, buf, NULL, screenWidth * screenHeight / 10);

    // Display driver
    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = screenWidth;
    disp_drv.ver_res = screenHeight;
    disp_drv.flush_cb = my_disp_flush;
    disp_drv.draw_buf = &draw_buf;
    lv_disp_drv_register(&disp_drv);

    // Touch driver
    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = my_touchpad_read;
    lv_indev_drv_register(&indev_drv);

    // Setup LVGL tick timer
    const esp_timer_create_args_t lvgl_tick_timer_args = {
        .callback = &example_increase_lvgl_tick,
        .name = "lvgl_tick"
    };
    esp_timer_handle_t lvgl_tick_timer = NULL;
    esp_timer_create(&lvgl_tick_timer_args, &lvgl_tick_timer);
    esp_timer_start_periodic(lvgl_tick_timer, EXAMPLE_LVGL_TICK_PERIOD_MS * 1000);

    // Show splash screen with "Walmagotchi" - blinking animation
    Serial.println("Showing splash screen...");
    lv_obj_t * splash_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(splash_screen, lv_color_black(), 0);
    lv_obj_clear_flag(splash_screen, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t * splash_label = lv_label_create(splash_screen);
    lv_label_set_text(splash_label, "Walmagotchi");
    lv_obj_set_style_text_font(splash_label, &lv_font_montserrat_32, 0);
    lv_obj_set_style_text_color(splash_label, lv_color_hex(0x00ADB5), 0);
    lv_obj_center(splash_label);

    lv_scr_load(splash_screen);

    // Blink effect - show/hide every 0.5 seconds for 3 seconds
    for(int blink = 0; blink < 6; blink++) {
        if (blink % 2 == 0) {
            lv_obj_add_flag(splash_label, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_clear_flag(splash_label, LV_OBJ_FLAG_HIDDEN);
        }

        // Update screen for 0.5 seconds
        for(int i = 0; i < 17; i++) {  // 17 * 30ms ≈ 500ms
            lv_timer_handler();
            delay(30);
        }
    }

    Serial.println("Splash screen shown");

    // Initialize SquareLine Studio UI
    ui_init();

    // Setup event handlers for new UI
    setupUIHandlers();

    // Load first screen (Pet Screen) after splash
    lv_scr_load(ui_Screen1);
}

// ============================================
// Sui Balance Fetch
// ============================================

void fetchSuiBalance() {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[BALANCE] WiFi not connected");
        return;
    }

    unsigned long now = millis();
    if (now - lastBalanceFetch < BALANCE_FETCH_INTERVAL) {
        return;  // Don't fetch too often
    }
    lastBalanceFetch = now;

    Serial.println("[BALANCE] Fetching balance from Sui RPC...");

    HTTPClient http;
    http.begin(SUI_RPC_URL);
    http.addHeader("Content-Type", "application/json");

    // Build JSON-RPC request to get balance
    JsonDocument doc;
    doc["jsonrpc"] = "2.0";
    doc["id"] = 1;
    doc["method"] = "suix_getBalance";
    JsonArray params = doc.createNestedArray("params");
    params.add(DEVICE_WALLET_ADDRESS);

    String requestBody;
    serializeJson(doc, requestBody);

    Serial.printf("[BALANCE] Request: %s\n", requestBody.c_str());

    int httpCode = http.POST(requestBody);

    if (httpCode > 0) {
        if (httpCode == HTTP_CODE_OK) {
            String payload = http.getString();
            Serial.printf("[BALANCE] Response: %s\n", payload.c_str());

            // Parse response
            JsonDocument response;
            DeserializationError error = deserializeJson(response, payload);

            if (!error) {
                if (response.containsKey("result")) {
                    // Balance is in MIST (1 SUI = 1,000,000,000 MIST)
                    long long totalBalance = response["result"]["totalBalance"];
                    double suiAmount = totalBalance / 1000000000.0;

                    // Format balance string
                    char balanceBuf[32];
                    snprintf(balanceBuf, sizeof(balanceBuf), "%.4f", suiAmount);
                    suiBalance = String(balanceBuf);

                    Serial.printf("[BALANCE] ✓ Balance: %s SUI (%lld MIST)\n",
                                  suiBalance.c_str(), totalBalance);
                } else if (response.containsKey("error")) {
                    Serial.printf("[BALANCE] RPC Error: %s\n",
                                  response["error"]["message"].as<const char*>());
                    suiBalance = "Error";
                }
            } else {
                Serial.printf("[BALANCE] JSON parse error: %s\n", error.c_str());
                suiBalance = "Parse error";
            }
        } else {
            Serial.printf("[BALANCE] HTTP error: %d\n", httpCode);
            suiBalance = "HTTP error";
        }
    } else {
        Serial.printf("[BALANCE] Connection failed: %s\n", http.errorToString(httpCode).c_str());
        suiBalance = "No connection";
    }

    http.end();
}

// ============================================
// WiFi Setup
// ============================================

void setupWiFi() {
    Serial.println("\n=== Connecting to WiFi ===");
    Serial.print("SSID: ");
    Serial.println(WIFI_SSID);

    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
        delay(500);
        Serial.print(".");
        attempts++;
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\n✓ WiFi Connected!");
        Serial.print("IP: ");
        Serial.println(WiFi.localIP());
    } else {
        Serial.println("\n✗ WiFi Connection Failed");
    }
}

// ============================================
// IMU & Step Detection
// ============================================

void setupIMU() {
    Serial.println("\n=== Initializing IMU ===");

    if (QMI8658_init() == 1) {
        Serial.println("✓ QMI8658 initialized");
        imuInitialized = true;
    } else {
        Serial.println("✗ QMI8658 initialization failed");
        imuInitialized = false;
    }
}

void detectSteps() {
    if (!imuInitialized) return;

    // Throttle IMU reading to reduce CPU load and prevent screen flicker
    unsigned long currentTime = millis();
    if (currentTime - lastIMUReadTime < IMU_READ_INTERVAL) {
        return;
    }
    lastIMUReadTime = currentTime;

    float acc[3];
    QMI8658_read_acc_xyz(acc);

    // Store sample in buffer
    accSampleBuffer[accSampleIndex][0] = acc[0];
    accSampleBuffer[accSampleIndex][1] = acc[1];
    accSampleBuffer[accSampleIndex][2] = acc[2];
    accSampleIndex = (accSampleIndex + 1) % 30;

    // Calculate magnitude
    float magnitude = sqrt(acc[0]*acc[0] + acc[1]*acc[1] + acc[2]*acc[2]);
    float verticalAcc = acc[2];  // Z-axis

    // Check device orientation
    bool isVertical = (verticalAcc > deviceOrientationThreshold ||
                      verticalAcc < -deviceOrientationThreshold);

    // Step detection algorithm (currentTime already declared above)
    float accChange = abs(magnitude - lastAccMagnitude);

    if (isVertical &&
        accChange > stepThresholdMin &&
        accChange < stepThresholdMax &&
        (currentTime - lastStepTime) > stepCooldown) {

        if (!stepPeakDetected) {
            stepPeakDetected = true;
            stepCount++;
            lastStepTime = currentTime;

            // Every 100 steps = 1 food
            if (stepCount % 100 == 0) {
                virtualPet.addFood(1);
            }

            // Every 150 steps = 2 energy
            if (stepCount % 150 == 0) {
                virtualPet.addEnergy(2);
            }

            Serial.printf("✓ Step detected! Total: %d\n", stepCount);
        }
    } else {
        if ((currentTime - lastStepTime) > 300) {
            stepPeakDetected = false;
        }
    }

    lastAccMagnitude = magnitude;
    lastVerticalAcc = verticalAcc;
}

// ============================================
// Oracle Data Submission
// ============================================

void submitStepsToOracle() {
    if (!oracleClient || !oracleClient->isAuthenticated()) {
        Serial.println("Oracle not ready");
        return;
    }

    if (stepCount < minStepsForSubmission) {
        Serial.printf("Not enough steps (%d < %d)\n", stepCount, minStepsForSubmission);
        return;
    }

    unsigned long now = millis();
    if (now - lastSubmissionTime < submissionInterval) {
        return;  // Not time yet
    }

    Serial.println("\n=== Submitting to Oracle ===");

    // Get battery level (mock for now)
    int batteryPercent = 85;

    // Submit with current accelerometer samples
    bool success = oracleClient->submitStepData(
        stepCount,
        now,
        batteryPercent,
        accSampleBuffer,
        30
    );

    if (success) {
        lastSubmissionTime = now;
        Serial.println("[ORACLE] Data submitted successfully");
    } else {
        Serial.println("[ORACLE] Submit failed");
    }
}

// ============================================
// Setup & Loop
// ============================================

void setup() {
    Serial.begin(115200);
    Serial.println("\n\n=== SUI Watch - Trust Oracle ===");

    // Initialize PSRAM
    if (psramInit()) {
        Serial.println("PSRAM initialized");
    } else {
        Serial.println("PSRAM not available");
    }

    // Allocate BlackImage in PSRAM (required by LCD driver)
    UDOUBLE Imagesize = LCD_1IN28_HEIGHT * LCD_1IN28_WIDTH * 2;
    if ((BlackImage = (UWORD *)ps_malloc(Imagesize)) == NULL) {
        Serial.println("Failed to allocate BlackImage!");
        while(1);
    }
    Serial.println("BlackImage allocated");

    // Initialize hardware
    DEV_Module_Init();
    LCD_1IN28_Init(HORIZONTAL);
    LCD_1IN28_Clear(0x0000);  // Clear to black

    // Setup touch
    touch.begin();

    // Setup UI
    setupUI();

    // Initialize Virtual Pet
    virtualPet.init("Tamagotchi");
    Serial.println("[PET] Virtual Pet initialized!");

    // Setup WiFi
    setupWiFi();

    // Setup IMU
    setupIMU();

    // Initialize Trust Oracle Client
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\n=== Initializing Trust Oracle ===");

        // Pass private key if configured (supports both hex and bech32)
        const char* privKey = (strlen(DEVICE_PRIVATE_KEY) > 0) ? DEVICE_PRIVATE_KEY : nullptr;
        oracleClient = new TrustOracleClient(ORACLE_HOST, ORACLE_PORT, DEVICE_ID, privKey);
        oracleClient->begin();
        Serial.println("[ORACLE] Connecting...");
    }

    Serial.println("\n✓ Setup Complete!\n");
}

void loop() {
    // LVGL timer
    lv_timer_handler();

    // IMU & Step detection (throttled internally)
    detectSteps();

    // Oracle client loop
    if (oracleClient) {
        oracleClient->loop();

        // Submit data periodically
        submitStepsToOracle();
    }

    // Update Virtual Pet
    unsigned long currentTime = millis();
    if (currentTime - lastPetUpdate > PET_UPDATE_INTERVAL) {
        lastPetUpdate = currentTime;
        virtualPet.update(currentTime);

        // Check if pet needs attention
        if (virtualPet.needsAttention()) {
            Serial.println("[PET] Your pet needs attention!");
        }

        // Sync pet with blockchain every minute
        static unsigned long lastPetSync = 0;
        if (currentTime - lastPetSync > 60000) {  // 60 seconds
            lastPetSync = currentTime;
            if (oracleClient && oracleClient->isAuthenticated()) {
                String petJson = virtualPet.toJSON();
                oracleClient->syncPet(petJson);
                Serial.println("🔄 Pet synced to blockchain");
            }
        }
    }

    // Fetch SUI balance periodically (30s interval, throttled internally)
    fetchSuiBalance();

    // Update UI screens periodically (100ms interval)
    static unsigned long lastUIUpdate = 0;
    unsigned long uiTime = millis();
    if (uiTime - lastUIUpdate > 100) {
        lastUIUpdate = uiTime;

        // Update all screens (they check internally if they're visible)
        updateScreen1PetUI();    // Pet display - always update for animation
        updateScreen2ResourcesUI();  // Food/Energy counts
        updateScreen3StepsUI();  // Step counter
        updateScreen4WalletUI(); // Wallet info
    }

    // Increase delay to reduce CPU load and prevent screen flicker
    delay(10);  // 10ms delay (was 2ms)
}
