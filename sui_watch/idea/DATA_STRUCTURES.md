# Data Structures Specification

## ESP32 → Backend

### StepDataPayload (Signed)
Format sent from ESP32 to backend server.

```json
{
  "deviceId": "a1b2c3d4e5f6g7h8",
  "stepCount": 450,
  "timestamp": 1735492800,
  "firmwareVersion": 100,
  "batteryPercent": 85,
  "rawAccSamples": [
    [100.5, 50.2, -980.3],
    [102.1, 51.0, -982.1],
    ...
  ],
  "signature": "0x1234567890abcdef..."
}
```

**Fields**:
- `deviceId` (string, 16 chars): Unique device identifier
- `stepCount` (uint32): Number of steps in this batch
- `timestamp` (uint64): Unix timestamp in seconds
- `firmwareVersion` (uint16): Firmware version (e.g., 100 = v1.0.0)
- `batteryPercent` (uint8, 0-100): Battery level
- `rawAccSamples` (array): Last 30 accelerometer samples [x, y, z]
- `signature` (string, hex): Ed25519 signature of all above fields

**Canonical Serialization for Signing**:
```json
{
  "deviceId": "a1b2c3d4e5f6g7h8",
  "stepCount": 450,
  "timestamp": 1735492800,
  "firmwareVersion": 100,
  "batteryPercent": 85,
  "rawAccSamples": [[100.5,50.2,-980.3],[102.1,51.0,-982.1]]
}
```
→ SHA256 hash → Sign with Ed25519 private key

---

## Backend Database Schema

### devices table
```sql
CREATE TABLE devices (
    device_id TEXT PRIMARY KEY,          -- 16 char hex
    public_key TEXT NOT NULL UNIQUE,     -- 64 char hex (32 bytes)
    registered_at INTEGER NOT NULL,      -- Unix timestamp (ms)
    last_seen INTEGER,                   -- Unix timestamp (ms)
    firmware_version TEXT,               -- "1.0.0"
    total_steps INTEGER DEFAULT 0,
    total_submissions INTEGER DEFAULT 0,
    status TEXT DEFAULT 'active'         -- active, suspended, banned
);
```

### step_data table
```sql
CREATE TABLE step_data (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    device_id TEXT NOT NULL,
    step_count INTEGER NOT NULL,
    timestamp INTEGER NOT NULL,          -- Unix timestamp (seconds)
    raw_samples TEXT,                    -- JSON array
    battery_percent INTEGER,             -- 0-100
    signature TEXT NOT NULL,             -- Hex string
    verified BOOLEAN DEFAULT FALSE,
    received_at INTEGER NOT NULL,        -- Unix timestamp (seconds)
    submitted_to_chain BOOLEAN DEFAULT FALSE,
    tx_digest TEXT,                      -- Sui transaction digest
    FOREIGN KEY (device_id) REFERENCES devices(device_id)
);
```

### submissions table
```sql
CREATE TABLE submissions (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    tx_digest TEXT UNIQUE NOT NULL,
    device_ids TEXT NOT NULL,            -- JSON array
    total_steps INTEGER NOT NULL,
    submitted_at INTEGER NOT NULL,       -- Unix timestamp (seconds)
    confirmed BOOLEAN DEFAULT FALSE,
    confirmed_at INTEGER,
    checkpoint TEXT,
    gas_used INTEGER
);
```

---

## Backend → Blockchain

### Transaction Call Structure
```javascript
{
  target: "0xPACKAGE::trust_oracle::submit_step_data",
  arguments: [
    registry_id,              // Shared object ID
    device_object_id,         // Device object ID
    step_count,               // u64
    timestamps,               // vector<u64>
    signatures                // vector<vector<u8>>
  ]
}
```

**Example**:
```javascript
tx.moveCall({
  target: `${packageId}::trust_oracle::submit_step_data`,
  arguments: [
    tx.object(registryId),
    tx.object(deviceObjectId),
    tx.pure.u64(2450),
    tx.pure.vector('u64', [1735492800, 1735496400, 1735500000]),
    tx.pure.vector('vector<u8>', [
      [0x12, 0x34, ...],  // signature 1
      [0x56, 0x78, ...],  // signature 2
      [0x9a, 0xbc, ...]   // signature 3
    ])
  ]
});
```

---

## Blockchain Data Structures (Move)

### OracleRegistry (Shared Object)
```move
struct OracleRegistry has key {
    id: UID,
    total_devices: u64,
    total_submissions: u64,
    total_steps_recorded: u64,
}
```

### Device (Owned Object)
```move
struct Device has key, store {
    id: UID,
    device_id: String,              // UTF-8 string
    public_key: vector<u8>,         // 32 bytes
    registered_at: u64,             // Timestamp (ms)
    total_steps: u64,
    total_submissions: u64,
    is_active: bool,
}
```

### StepDataRecord (Owned Object)
```move
struct StepDataRecord has key, store {
    id: UID,
    device_id: String,
    step_count: u64,
    timestamp: u64,                 // Timestamp (ms)
    signatures: vector<vector<u8>>, // Multiple signatures for batch
    submitter: address,             // Backend server address
    submitted_at: u64,
    verified: bool,
}
```

### AchievementNFT (Owned Object)
```move
struct StepAchievementNFT has key, store {
    id: UID,
    device_id: String,
    milestone: String,              // "1000_steps"
    steps: u64,
    achieved_at: u64,
    image_url: Url,
    description: String,
}
```

---

## Events

### DeviceRegistered
```move
struct DeviceRegistered has copy, drop {
    device_id: String,
    public_key: vector<u8>,
    timestamp: u64,
}
```

Emitted when: New device registers
Use case: Index devices off-chain

---

### StepDataSubmitted
```move
struct StepDataSubmitted has copy, drop {
    device_id: String,
    step_count: u64,
    timestamp: u64,
    record_id: address,
}
```

Emitted when: Step data submitted to chain
Use case: Notify frontend, update analytics

---

### MilestoneAchieved
```move
struct MilestoneAchieved has copy, drop {
    device_id: String,
    milestone: String,
    total_steps: u64,
}
```

Emitted when: Device reaches milestone
Use case: Trigger NFT minting, notify user

---

## ESP32 Internal Structures

### C++ Structs

**StepDataPayload** (before signing):
```cpp
struct StepDataPayload {
    char deviceId[17];           // 16 hex + null
    uint32_t stepCount;
    uint64_t timestamp;
    uint16_t firmwareVersion;
    uint8_t batteryPercent;
    float rawAccSamples[30][3];  // 30 samples, XYZ
};
```

**QueuedData** (offline queue):
```cpp
struct QueuedData {
    char jsonPayload[2048];      // Full JSON string
    uint64_t timestamp;
    uint8_t retryCount;
};
```

**RawAccSample** (IMU data):
```cpp
struct RawAccSample {
    float acc[3];                // X, Y, Z in mg
    uint32_t timestamp_ms;
};
```

---

## Message Flow Diagram

```
ESP32                     Backend                    Blockchain
  │                          │                           │
  │  1. Generate Keypair     │                           │
  ├─────────────────────────►│ POST /register           │
  │  { publicKey }           │                           │
  │                          ├──────────────────────────►│
  │                          │  register_device()        │
  │◄─────────────────────────│                           │
  │  { deviceId }            │◄──────────────────────────│
  │                          │  Device object created    │
  │                          │                           │
  │  2. Detect 100 steps     │                           │
  │     Sign data            │                           │
  ├─────────────────────────►│ POST /step-data/submit   │
  │  { payload, signature }  │                           │
  │                          │  Verify signature         │
  │                          │  Store in DB              │
  │◄─────────────────────────│                           │
  │  { success: true }       │                           │
  │                          │                           │
  │                          │  3. Daily batch (2 AM)    │
  │                          ├──────────────────────────►│
  │                          │  submit_step_data()       │
  │                          │◄──────────────────────────│
  │                          │  TX confirmed             │
  │                          │                           │
  │  4. Query status         │                           │
  ├─────────────────────────►│ GET /stats               │
  │◄─────────────────────────│                           │
  │  { totalSteps: 2450 }    │                           │
```

---

## Size Estimations

### ESP32 Memory Usage
- **StepDataPayload**: ~400 bytes
- **QueuedData**: 2.1 KB per entry
- **Queue (10 entries)**: ~21 KB
- **Total RAM usage**: < 50 KB

### Backend Database
- **Device record**: ~200 bytes
- **Step data record**: ~500 bytes
- **1000 devices, 10 submissions each**: ~5 MB

### Blockchain
- **Device object**: ~150 bytes
- **StepDataRecord**: ~300 bytes
- **Gas per submission**: ~0.005 SUI

---

## Signature Scheme Details

### Ed25519 Signature
- **Algorithm**: Ed25519 (Edwards-curve Digital Signature Algorithm)
- **Key size**: 32 bytes (256 bits)
- **Signature size**: 64 bytes (512 bits)
- **Security**: ~128-bit security level

### Signing Process (ESP32)
```cpp
// 1. Build canonical JSON
String canonical = buildCanonicalJSON(payload);

// 2. Hash with SHA256
uint8_t hash[32];
SHA256(canonical.c_str(), canonical.length(), hash);

// 3. Sign hash
uint8_t signature[64];
suiKeypair.sign(hash, 32, signature);

// 4. Encode to hex
String sigHex = bytesToHex(signature, 64);
```

### Verification Process (Backend)
```javascript
// 1. Reconstruct canonical JSON
const canonical = JSON.stringify(payload, Object.keys(payload).sort());

// 2. Hash
const hash = crypto.createHash('sha256').update(canonical).digest();

// 3. Verify
const valid = nacl.sign.detached.verify(
    hash,
    signatureBytes,
    publicKeyBytes
);
```

---

## Validation Rules

### ESP32 Validation
- Step count: 0 < count ≤ 100,000
- Battery percent: 0 ≤ battery ≤ 100
- Timestamp: within ±5 minutes of current time
- Raw samples: exactly 30 samples

### Backend Validation
- Device ID: 16 hex characters
- Public key: 64 hex characters (32 bytes)
- Signature: 128 hex characters (64 bytes)
- Timestamp: not older than 7 days
- No duplicate submissions (device + timestamp)

### Blockchain Validation
- Step count: 0 < count ≤ 100,000
- Timestamp: not in future
- Device must be active
- Sender must be registered backend

---

## JSON Schema (OpenAPI)

### StepDataPayload
```yaml
StepDataPayload:
  type: object
  required:
    - deviceId
    - stepCount
    - timestamp
    - signature
  properties:
    deviceId:
      type: string
      pattern: '^[0-9a-f]{16}$'
    stepCount:
      type: integer
      minimum: 1
      maximum: 100000
    timestamp:
      type: integer
      format: int64
    firmwareVersion:
      type: integer
      minimum: 0
      maximum: 65535
    batteryPercent:
      type: integer
      minimum: 0
      maximum: 100
    rawAccSamples:
      type: array
      items:
        type: array
        items:
          type: number
        minItems: 3
        maxItems: 3
      maxItems: 30
    signature:
      type: string
      pattern: '^0x[0-9a-f]{128}$'
```

---

## Constants

```cpp
// ESP32
#define MAX_QUEUE_SIZE 10
#define RAW_SAMPLE_BUFFER_SIZE 30
#define SIGNATURE_SIZE 64
#define PUBLIC_KEY_SIZE 32
#define DEVICE_ID_LENGTH 16

// Backend
const MAX_TIMESTAMP_AGE = 7 * 24 * 60 * 60; // 7 days
const MAX_STEP_COUNT = 100000;
const BATCH_SUBMISSION_HOUR = 2; // 2 AM

// Blockchain
const MAX_STEP_COUNT: u64 = 100000;
const MAX_TIMESTAMP_FUTURE: u64 = 300000; // 5 minutes (ms)
```

This completes the data structures specification!
