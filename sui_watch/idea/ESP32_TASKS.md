# ESP32 Firmware Development Tasks

## Overview
Tasks cho phát triển firmware ESP32-S3 với tính năng Hardware Witness, step counter, và SUI wallet.

---

## Module 1: Time Management System

### Task 1.1: NTP Client Implementation
**Priority**: High
**Estimated Time**: 4 hours
**Dependencies**: None

**Implementation Details**:
```cpp
// File: TimeManager.h
class TimeManager {
private:
    struct tm currentTime;
    bool synced;
    unsigned long lastSyncTime;
    const char* ntpServer;

public:
    bool syncWithNTP();
    bool updateFromRTC();
    String getCurrentTime();  // "HH:MM:SS"
    String getCurrentDate();  // "DD/MM/YYYY"
    uint64_t getUnixTimestamp();
    void setTimezone(int offset);
};
```

**Subtasks**:
- [ ] Include WiFiUdp and NTPClient libraries
- [ ] Implement `syncWithNTP()` with retry logic (3 attempts)
- [ ] Handle timezone offset
- [ ] Store last sync timestamp
- [ ] Auto-sync every 24 hours
- [ ] Test with different NTP servers

**Test Cases**:
- [ ] Sync with pool.ntp.org
- [ ] Handle network timeout
- [ ] Verify timezone calculation
- [ ] Test leap second handling

**Files to Create**:
- `TimeManager.h`
- `TimeManager.cpp`

---

### Task 1.2: RTC Integration
**Priority**: Medium
**Estimated Time**: 2 hours
**Dependencies**: Task 1.1

**Implementation Details**:
```cpp
// Use ESP32 internal RTC
void setupRTC() {
    // Configure RTC from NTP
    struct timeval tv;
    tv.tv_sec = getUnixTimestamp();
    settimeofday(&tv, NULL);
}

void readRTC() {
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);
}
```

**Subtasks**:
- [ ] Configure ESP32 internal RTC
- [ ] Update RTC from NTP
- [ ] Read RTC when offline
- [ ] Handle RTC drift

**Test Cases**:
- [ ] RTC accuracy over 24h
- [ ] Offline time keeping
- [ ] Power loss recovery

---

## Module 2: Cryptographic System

### Task 2.1: Key Management
**Priority**: Critical
**Estimated Time**: 6 hours
**Dependencies**: None

**Implementation Details**:
```cpp
// File: CryptoManager.h
class CryptoManager {
private:
    MicroSuiEd25519 keypair;
    bool keyExists;

public:
    bool initializeKeys();
    bool generateNewKeypair();
    bool loadKeypairFromNVS();
    bool saveKeypairToNVS();
    String getPublicKeyHex();
    String getDeviceId();  // First 8 bytes of pubkey hash
    bool signData(const uint8_t* data, size_t len, uint8_t* signature);
};
```

**Subtasks**:
- [ ] Check NVS for existing keys
- [ ] Generate Ed25519 keypair on first boot
- [ ] Encrypt private key before NVS storage
- [ ] Implement secure key retrieval
- [ ] Export public key in hex format
- [ ] Device ID generation (SHA256 of pubkey)

**Security Considerations**:
- [ ] Enable NVS encryption
- [ ] Never log private key
- [ ] Secure erase on factory reset
- [ ] Test key persistence across reboots

**Files to Create**:
- `CryptoManager.h`
- `CryptoManager.cpp`

---

### Task 2.2: Hardware Signing Implementation
**Priority**: Critical
**Estimated Time**: 8 hours
**Dependencies**: Task 2.1

**Implementation Details**:
```cpp
// File: StepSignature.h
struct StepDataPayload {
    char deviceId[17];           // 16 hex chars + null
    uint32_t stepCount;
    uint64_t timestamp;
    uint16_t firmwareVersion;
    float rawAccSamples[30][3];  // 30 samples, XYZ
    uint8_t batteryPercent;
};

class StepSignature {
public:
    bool createSignedBatch(uint32_t steps, String& jsonOutput);
    bool buildPayload(StepDataPayload* payload);
    bool signPayload(const StepDataPayload* payload, uint8_t* signature);
    String toJSON(const StepDataPayload* payload, const uint8_t* signature);
};
```

**Subtasks**:
- [ ] Define payload structure
- [ ] Implement payload serialization
- [ ] Hash payload with SHA256
- [ ] Sign hash with Ed25519
- [ ] Convert to JSON format
- [ ] Optimize for ESP32 performance

**Payload Format**:
```json
{
  "deviceId": "a1b2c3d4e5f6g7h8",
  "stepCount": 450,
  "timestamp": 1735492800,
  "firmwareVersion": 100,
  "rawAccSamples": [[x,y,z], ...],
  "batteryPercent": 85,
  "signature": "0x1234...abcd"
}
```

**Test Cases**:
- [ ] Signature verification on backend
- [ ] Performance < 100ms
- [ ] Memory usage check
- [ ] JSON parsing correctness

---

## Module 3: Step Counter Enhancement

### Task 3.1: Improve Detection Algorithm
**Priority**: High
**Estimated Time**: 10 hours
**Dependencies**: None

**Current Issues** (from line 422-530):
- Accuracy ~90%, need >95%
- False positives when device is flat
- Threshold tuning needed

**Improvements**:
```cpp
// Enhanced detectStep() with ML-inspired features
void detectStepV2() {
    // Feature extraction
    float verticalAcc = acc[2];
    float horizontalMag = sqrt(acc[0]*acc[0] + acc[1]*acc[1]);
    float totalMag = sqrt(acc[0]*acc[0] + acc[1]*acc[1] + acc[2]*acc[2]);

    // Multi-threshold detection
    bool deviceVertical = (abs(verticalAcc) < 700);  // Not flat
    bool peakDetected = (horizontalMag > 200 && horizontalMag < 2000);
    bool valleyDetected = (horizontalChange < -150);

    // Frequency analysis (0.5-2 Hz for walking)
    unsigned long timeSinceLastStep = currentTime - lastStepTime;
    bool validFrequency = (timeSinceLastStep > 400 && timeSinceLastStep < 2000);

    if (deviceVertical && peakDetected && valleyDetected && validFrequency) {
        stepCount++;
    }
}
```

**Subtasks**:
- [ ] Record test dataset (100 real steps)
- [ ] Tune thresholds for accuracy
- [ ] Add frequency analysis
- [ ] Implement step pattern validation
- [ ] Add calibration mode
- [ ] Test with different walking speeds

**Test Cases**:
- [ ] Walking: normal speed
- [ ] Walking: fast speed
- [ ] Running: jogging
- [ ] False positives: sitting, lying down
- [ ] False positives: driving car

---

### Task 3.2: Raw Data Logging
**Priority**: Medium
**Estimated Time**: 4 hours
**Dependencies**: Task 3.1

**Implementation**:
```cpp
// Circular buffer for raw samples
#define RAW_SAMPLE_BUFFER_SIZE 30

struct RawAccSample {
    float acc[3];
    uint32_t timestamp_ms;
};

class StepDataLogger {
private:
    RawAccSample buffer[RAW_SAMPLE_BUFFER_SIZE];
    int writeIndex;

public:
    void logSample(float acc[3]);
    void getRawSamples(float output[][3], int count);
    void clear();
};
```

**Subtasks**:
- [ ] Circular buffer implementation
- [ ] Sample every 50ms
- [ ] Store last 30 samples
- [ ] Include in signature payload
- [ ] Memory optimization

---

## Module 4: Trust Oracle Client

### Task 4.1: HTTP Client Implementation
**Priority**: High
**Estimated Time**: 6 hours
**Dependencies**: Task 2.2

**Implementation**:
```cpp
// File: TrustOracle.h
class TrustOracle {
private:
    const char* serverUrl;
    HTTPClient httpClient;

public:
    bool submitStepData(const String& jsonPayload);
    bool registerDevice(const String& publicKey);
    bool checkConnection();
    int getQueueSize();
};
```

**Subtasks**:
- [ ] POST /api/step-data/submit endpoint
- [ ] POST /api/devices/register endpoint
- [ ] Handle HTTPS with certificates
- [ ] Implement retry logic (3 attempts)
- [ ] Timeout handling (10 seconds)
- [ ] Parse response codes

**Error Handling**:
- [ ] 200 OK: Clear queue
- [ ] 400 Bad Request: Log error, discard
- [ ] 403 Forbidden: Invalid signature, alert user
- [ ] 500 Server Error: Retry later
- [ ] Network timeout: Add to queue

---

### Task 4.2: Offline Data Queue
**Priority**: High
**Estimated Time**: 6 hours
**Dependencies**: Task 4.1

**Implementation**:
```cpp
// File: DataQueue.h
#define MAX_QUEUE_SIZE 10

struct QueuedData {
    char jsonPayload[2048];
    uint64_t timestamp;
    uint8_t retryCount;
};

class DataQueue {
private:
    QueuedData queue[MAX_QUEUE_SIZE];
    int queueSize;

public:
    bool enqueue(const String& payload);
    bool dequeue(String& payload);
    bool saveToNVS();
    bool loadFromNVS();
    void clearOldEntries();  // > 7 days
};
```

**Subtasks**:
- [ ] Ring buffer implementation
- [ ] Persist to NVS
- [ ] Auto-retry on WiFi reconnect
- [ ] Maximum 10 entries (FIFO)
- [ ] Clear old entries (7 days)

**Test Cases**:
- [ ] Queue full scenario
- [ ] NVS persistence across reboots
- [ ] WiFi reconnect auto-upload

---

## Module 5: UI/UX Development

### Task 5.1: Watch Face Screen
**Priority**: High
**Estimated Time**: 8 hours
**Dependencies**: Task 1.1

**Design in SquareLine Studio**:
```
Screen Layout:
┌─────────────────────┐
│    12:34:56 PM     │  ← Large digital time
│   Mon, Jan 15      │  ← Date display
│                    │
│   👟 1,234 steps   │  ← Step counter
│   💰 0.45 SUI      │  ← Balance
│                    │
│  [●●●●●●○○○○] 65%  │  ← Battery
└─────────────────────┘
```

**Subtasks**:
- [ ] Create ui_ScreenWatch in SquareLine
- [ ] Add label for time (HH:MM:SS)
- [ ] Add label for date
- [ ] Add step counter display
- [ ] Add balance display
- [ ] Add battery arc/bar
- [ ] Implement 1-second update timer
- [ ] Export C code

**Files to Create**:
- `ui_ScreenWatch.c`
- `ui_ScreenWatch.h`

---

### Task 5.2: Step Detail Screen
**Priority**: Medium
**Estimated Time**: 6 hours
**Dependencies**: Task 5.1

**Design**:
```
Screen Layout:
┌─────────────────────┐
│  Steps Today: 1,234│
│                    │
│  ┌──────────────┐  │
│  │   Chart      │  │  ← Hourly step chart
│  └──────────────┘  │
│                    │
│  Distance: 0.8 km  │
│  Calories: 45 kcal │
│                    │
│  [Submit to Chain] │  ← Button
└─────────────────────┘
```

**Subtasks**:
- [ ] Create ui_ScreenSteps
- [ ] Add bar chart for hourly data
- [ ] Calculate distance (0.75m per step)
- [ ] Calculate calories (0.04 per step)
- [ ] Add "Submit" button
- [ ] Show submission status

---

### Task 5.3: Enhanced Wallet Screen
**Priority**: Medium
**Estimated Time**: 8 hours
**Dependencies**: Existing ui_Screen1

**New Features**:
```
Screen Layout:
┌─────────────────────┐
│  Balance: 1.234 SUI│
│  0xb0bd..efe3      │
│                    │
│  [Send] [Receive]  │
│                    │
│  Recent:           │
│  • +0.1 SUI ✓      │
│  • -0.05 SUI ✓     │
│  • +0.2 SUI ⏳     │
└─────────────────────┘
```

**Subtasks**:
- [ ] Add transaction history list
- [ ] Implement QR code display for receive
- [ ] Add send transaction UI
- [ ] Auto-refresh balance (30s)
- [ ] Transaction status icons

---

## Module 6: Main Application Updates

### Task 6.1: Integrate Time Manager
**Priority**: High
**Estimated Time**: 4 hours
**Dependencies**: Task 1.1, 1.2

**Changes to ESP32S3_Squareline_UI.ino**:
```cpp
// Add globals
TimeManager timeManager;
bool timeInitialized = false;

// In setup()
void setup() {
    // ... existing code ...

    // Initialize time after WiFi
    if (WiFi.status() == WL_CONNECTED) {
        timeManager.syncWithNTP();
        timeInitialized = true;
    }
}

// In loop()
void loop() {
    // Update watch face every second
    if (timeInitialized && ui_ScreenWatch_visible) {
        String time = timeManager.getCurrentTime();
        lv_label_set_text(ui_timeLabel, time.c_str());
    }
}
```

**Subtasks**:
- [ ] Add TimeManager to setup()
- [ ] Sync on WiFi connect
- [ ] Update UI every second
- [ ] Handle timezone changes

---

### Task 6.2: Integrate Crypto & Signing
**Priority**: Critical
**Estimated Time**: 6 hours
**Dependencies**: Task 2.1, 2.2

**Changes**:
```cpp
// Add globals
CryptoManager cryptoManager;
StepSignature stepSigner;
bool cryptoInitialized = false;

// In setup()
void setup() {
    // ... after WiFi ...

    cryptoManager.initializeKeys();

    // Register device with backend
    String pubKey = cryptoManager.getPublicKeyHex();
    trustOracle.registerDevice(pubKey);

    cryptoInitialized = true;
}

// Update detectStep()
void detectStep() {
    // ... existing detection ...

    if (valid step) {
        stepCount++;
        stepLogger.logSample(acc);

        // Sign every 100 steps
        if (stepCount % 100 == 0) {
            signAndQueueSteps();
        }
    }
}

void signAndQueueSteps() {
    String signedData;
    if (stepSigner.createSignedBatch(stepCount, signedData)) {
        dataQueue.enqueue(signedData);
        Serial.println("Signed batch queued");
    }
}
```

**Subtasks**:
- [ ] Initialize crypto on boot
- [ ] Register device on first boot
- [ ] Trigger signing every 100 steps
- [ ] Add to upload queue
- [ ] Test end-to-end flow

---

### Task 6.3: Integrate Trust Oracle Client
**Priority**: High
**Estimated Time**: 6 hours
**Dependencies**: Task 4.1, 4.2

**Changes**:
```cpp
// Add globals
TrustOracle trustOracle("http://192.168.1.11:3001");
DataQueue dataQueue;

// In loop()
void loop() {
    // ... existing code ...

    // Upload queued data every 5 minutes
    static unsigned long lastUpload = 0;
    if (millis() - lastUpload > 300000) {  // 5 minutes
        uploadQueuedData();
        lastUpload = millis();
    }
}

void uploadQueuedData() {
    if (WiFi.status() != WL_CONNECTED) return;

    String payload;
    while (dataQueue.dequeue(payload)) {
        if (trustOracle.submitStepData(payload)) {
            Serial.println("✓ Data submitted");
        } else {
            dataQueue.enqueue(payload);  // Re-queue
            break;
        }
    }
}
```

**Subtasks**:
- [ ] Initialize trust oracle client
- [ ] Auto-upload every 5 minutes
- [ ] Re-queue on failure
- [ ] Show upload status on UI

---

## Module 7: Testing & Optimization

### Task 7.1: Memory Optimization
**Priority**: Medium
**Estimated Time**: 6 hours
**Dependencies**: All previous tasks

**Checks**:
- [ ] Heap usage < 80%
- [ ] No memory leaks
- [ ] Stack overflow protection
- [ ] PSRAM utilization

**Tools**:
```cpp
void printMemoryStats() {
    Serial.printf("Free heap: %d bytes\n", ESP.getFreeHeap());
    Serial.printf("PSRAM free: %d bytes\n", ESP.getFreePsram());
    Serial.printf("Stack high water mark: %d\n", uxTaskGetStackHighWaterMark(NULL));
}
```

---

### Task 7.2: Power Consumption Testing
**Priority**: Medium
**Estimated Time**: 8 hours
**Dependencies**: Task 7.1

**Optimizations**:
- [ ] WiFi sleep mode when idle
- [ ] Reduce IMU sampling rate (50ms → 100ms?)
- [ ] LCD brightness optimization
- [ ] CPU frequency scaling

**Target**: > 24 hours battery life

---

### Task 7.3: End-to-End Testing
**Priority**: Critical
**Estimated Time**: 12 hours
**Dependencies**: All tasks

**Test Scenarios**:
1. [ ] Normal flow: Walk 100 steps → Sign → Upload → Verify on backend
2. [ ] Offline mode: Walk 500 steps offline → Connect WiFi → Auto-upload
3. [ ] Time sync: Reboot → Sync NTP → Show correct time
4. [ ] Key persistence: Reboot → Keys still valid
5. [ ] Queue full: Generate 15 batches (queue max 10) → Oldest dropped
6. [ ] Network failure: Simulate server down → Re-queue and retry

---

## File Structure After Completion

```
src/sui-watch/
├── ESP32S3_Squareline_UI.ino    (Updated with all integrations)
├── TimeManager.h
├── TimeManager.cpp
├── CryptoManager.h
├── CryptoManager.cpp
├── StepSignature.h
├── StepSignature.cpp
├── TrustOracle.h
├── TrustOracle.cpp
├── DataQueue.h
├── DataQueue.cpp
├── QMI8658.cpp                  (Existing, enhanced)
├── QMI8658.h
├── GPIOGateway.cpp              (Existing, keep)
├── GPIOGateway.h
├── ui_ScreenWatch.c             (New)
├── ui_ScreenWatch.h             (New)
├── ui_ScreenSteps.c             (New)
├── ui_ScreenSteps.h             (New)
├── ui_Screen1.c                 (Updated - Wallet)
├── ui_Screen2.c                 (Existing)
├── ui_Screen3.c                 (Existing)
├── ui_Screen4.c                 (Existing - Settings)
├── ui_Screen5.c                 (Updated - Steps detail)
└── ... (other existing files)
```

---

## Priority Order for Implementation

1. **Week 1**: Tasks 2.1, 2.2 (Crypto + Signing) - Most critical
2. **Week 1**: Tasks 1.1, 1.2 (Time Management) - Core feature
3. **Week 2**: Tasks 3.1, 3.2 (Step Counter) - Improve accuracy
4. **Week 2**: Tasks 4.1, 4.2 (Trust Oracle) - Backend integration
5. **Week 3**: Tasks 5.1, 5.2, 5.3 (UI/UX) - User experience
6. **Week 3**: Tasks 6.1, 6.2, 6.3 (Integration) - Connect everything
7. **Week 4**: Tasks 7.1, 7.2, 7.3 (Testing) - Polish and validate

---

## Dependencies Graph

```
TimeManager (1.1, 1.2)
    ↓
WatchFace UI (5.1)
    ↓
Integration (6.1)

CryptoManager (2.1)
    ↓
StepSignature (2.2)
    ↓
TrustOracle (4.1)
    ↓
DataQueue (4.2)
    ↓
Integration (6.2, 6.3)

StepCounter (3.1, 3.2)
    ↓
StepUI (5.2)
    ↓
Integration (6.2)
```

---

## Completion Checklist

- [ ] All modules compile without errors
- [ ] No memory leaks detected
- [ ] All test cases pass
- [ ] Documentation updated
- [ ] Code reviewed
- [ ] Performance benchmarks met
- [ ] Security audit passed
- [ ] Ready for integration testing
