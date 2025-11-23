# Trust Oracle System Architecture

Complete system architecture for **SUI Watch Trust Oracle** - Hardware Witness for physical activity data on Sui blockchain.

---

## 🏗️ System Overview

```
┌─────────────────────────────────────────────────────────────────┐
│                   ESP32-S3 (Hardware Witness)                   │
│  ┌──────────────┬──────────────┬─────────────┬────────────────┐ │
│  │ QMI8658 IMU  │ Step Counter │ Ed25519 Key │ WebSocket      │ │
│  │ (Sensor)     │ Algorithm    │ Signing     │ Client         │ │
│  └──────────────┴──────────────┴─────────────┴────────────────┘ │
└──────────────────────────┬──────────────────────────────────────┘
                           │ WebSocket (ws://server:8080)
                           │ JSON Messages + Signatures
                           ▼
┌─────────────────────────────────────────────────────────────────┐
│               Trust Oracle Backend Server                       │
│  ┌──────────────┬──────────────┬─────────────┬────────────────┐ │
│  │ WebSocket    │ Ed25519      │ SQLite      │ REST API       │ │
│  │ Server       │ Verifier     │ Database    │ (Express)      │ │
│  └──────────────┴──────────────┴─────────────┴────────────────┘ │
│  ┌──────────────┬──────────────┬─────────────────────────────┐ │
│  │ Device       │ Crypto       │ Sui Client                  │ │
│  │ Manager      │ Manager      │ (Blockchain Integration)    │ │
│  └──────────────┴──────────────┴─────────────────────────────┘ │
└──────────────────────────┬──────────────────────────────────────┘
                           │ Sui SDK (@mysten/sui)
                           │ Transaction Builder
                           ▼
┌─────────────────────────────────────────────────────────────────┐
│                   Sui Blockchain (Testnet)                      │
│  ┌──────────────────────────────────────────────────────────┐  │
│  │  Trust Oracle Smart Contract (Move Language)             │  │
│  │  ┌────────────┬─────────────┬──────────────────────────┐ │  │
│  │  │ Oracle     │ Device      │ StepDataRecord           │ │  │
│  │  │ Registry   │ Registry    │ (Verified Data)          │ │  │
│  │  │ (Shared)   │ (Owned)     │ (Owned)                  │ │  │
│  │  └────────────┴─────────────┴──────────────────────────┘ │  │
│  └──────────────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────────────┘
```

---

## 📊 Data Flow

### 1. Device Registration Flow

```
ESP32                          Server                        Blockchain
  │                              │                               │
  │ 1. Generate Ed25519 keypair  │                               │
  │    (private key stored)      │                               │
  │                              │                               │
  │ 2. WebSocket: register       │                               │
  ├─────────────────────────────►│                               │
  │    {deviceId, publicKey}     │                               │
  │                              │                               │
  │                              │ 3. Store in SQLite            │
  │                              │    (devices table)            │
  │                              │                               │
  │                              │ 4. Call: register_device()    │
  │                              ├──────────────────────────────►│
  │                              │    (packageId, registryId,    │
  │                              │     deviceId, publicKey)      │
  │                              │                               │
  │                              │ 5. Create Device object       │
  │                              │◄──────────────────────────────┤
  │                              │    {deviceObjectId, txDigest} │
  │                              │                               │
  │ 6. register_response         │                               │
  │◄─────────────────────────────┤                               │
  │    {success, deviceObjectId} │                               │
  │                              │                               │
  │ 7. Store deviceObjectId      │                               │
  │    in NVS (persistent)       │                               │
```

### 2. Step Data Submission Flow

```
ESP32                          Server                        Blockchain
  │                              │                               │
  │ 1. Detect steps (QMI8658)    │                               │
  │    Accumulate 100+ steps     │                               │
  │                              │                               │
  │ 2. Build payload:            │                               │
  │    {deviceId, stepCount,     │                               │
  │     timestamp, battery,      │                               │
  │     rawAccSamples}           │                               │
  │                              │                               │
  │ 3. Canonical JSON serialize  │                               │
  │    (sorted keys)             │                               │
  │                              │                               │
  │ 4. SHA256 hash               │                               │
  │                              │                               │
  │ 5. Ed25519 sign with         │                               │
  │    private key               │                               │
  │                              │                               │
  │ 6. WebSocket: step_data      │                               │
  ├─────────────────────────────►│                               │
  │    {payload + signature}     │                               │
  │                              │                               │
  │                              │ 7. Rebuild canonical JSON     │
  │                              │                               │
  │                              │ 8. Verify Ed25519 signature   │
  │                              │    with device's public key   │
  │                              │                               │
  │                              │ 9. If valid:                  │
  │                              │    - Store in SQLite          │
  │                              │      (step_data table)        │
  │                              │    - Mark as verified         │
  │                              │                               │
  │ 10. step_data_response       │                               │
  │◄─────────────────────────────┤                               │
  │     {success, dataId}        │                               │
  │                              │                               │
  │                              │ 11. Daily batch (2 AM):       │
  │                              │     Query pending data        │
  │                              │     Group by device           │
  │                              │                               │
  │                              │ 12. Call: submit_step_data()  │
  │                              ├──────────────────────────────►│
  │                              │     (deviceObjectId,          │
  │                              │      totalSteps,              │
  │                              │      timestamps[],            │
  │                              │      signatures[])            │
  │                              │                               │
  │                              │ 13. Create StepDataRecord     │
  │                              │◄──────────────────────────────┤
  │                              │     {txDigest, recordId}      │
  │                              │                               │
  │                              │ 14. Mark as submitted         │
  │                              │     Update tx_digest          │
```

---

## 🔒 Security Architecture

### Ed25519 Cryptographic Flow

```
┌─────────────────────────────────────────────────────────────┐
│ ESP32 (Private Key Never Leaves Device)                    │
│                                                             │
│  1. Generate Keypair                                        │
│     ┌─────────────────────┐                                │
│     │ Private Key (32 B)  │  ← Stored in NVS (encrypted)   │
│     └─────────────────────┘                                │
│     ┌─────────────────────┐                                │
│     │ Public Key (32 B)   │  ← Sent to server              │
│     └─────────────────────┘                                │
│                                                             │
│  2. Signing Process                                         │
│     ┌─────────────────────┐                                │
│     │ Payload (JSON)      │                                │
│     └──────────┬──────────┘                                │
│                ▼                                            │
│     ┌─────────────────────┐                                │
│     │ Canonical JSON      │  ← Sorted keys, no spaces      │
│     └──────────┬──────────┘                                │
│                ▼                                            │
│     ┌─────────────────────┐                                │
│     │ SHA256 Hash (32 B)  │                                │
│     └──────────┬──────────┘                                │
│                ▼                                            │
│     ┌─────────────────────┐                                │
│     │ Ed25519 Sign        │  ← Use private key             │
│     └──────────┬──────────┘                                │
│                ▼                                            │
│     ┌─────────────────────┐                                │
│     │ Signature (64 B)    │  ← Sent with payload           │
│     └─────────────────────┘                                │
└─────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────┐
│ Server (Public Key Verification)                            │
│                                                             │
│  1. Receive {payload, signature}                            │
│                                                             │
│  2. Verification Process                                    │
│     ┌─────────────────────┐                                │
│     │ Payload (JSON)      │                                │
│     └──────────┬──────────┘                                │
│                ▼                                            │
│     ┌─────────────────────┐                                │
│     │ Rebuild Canonical   │  ← Same algorithm as ESP32     │
│     └──────────┬──────────┘                                │
│                ▼                                            │
│     ┌─────────────────────┐                                │
│     │ SHA256 Hash (32 B)  │                                │
│     └──────────┬──────────┘                                │
│                ▼                                            │
│     ┌─────────────────────┐                                │
│     │ Ed25519 Verify      │  ← Use device's public key     │
│     │ (hash, signature,   │                                │
│     │  publicKey)         │                                │
│     └──────────┬──────────┘                                │
│                ▼                                            │
│     ┌─────────────────────┐                                │
│     │ Valid? true/false   │                                │
│     └─────────────────────┘                                │
│                                                             │
│  3. If valid:                                               │
│     - Store in database with verified=true                  │
│     - Schedule for blockchain submission                    │
│                                                             │
│  4. If invalid:                                             │
│     - Reject submission                                     │
│     - Log security event                                    │
└─────────────────────────────────────────────────────────────┘
```

---

## 📦 Component Details

### ESP32 Firmware

**Location**: `/home/alvin/Esp32-s3/src/ESP32S3_Squareline_UI/`

**Key Components**:
- QMI8658 IMU sensor driver
- Step detection algorithm
- MicroSui library (Ed25519)
- WebSocket client
- LVGL UI

**New Modules Required**:
- `TrustOracleClient.cpp/.h` - WebSocket communication
- `StepSignature.cpp/.h` - Payload signing
- `DataQueue.cpp/.h` - Offline data queue

### Backend Server

**Location**: `/home/alvin/Esp32-s3/trust-oracle-server/`

**Modules**:
1. **server.mjs** - Main server, WebSocket + REST API
2. **deviceManager.mjs** - Device registry, SQLite database
3. **cryptoManager.mjs** - Ed25519 verification
4. **suiClient.mjs** - Sui blockchain integration

**Ports**:
- HTTP API: 3001
- WebSocket: 8080

### Smart Contract

**Location**: `/home/alvin/Esp32-s3/sui-watch-contracts/trust_oracle/`

**Deployed**:
- Network: Sui Testnet
- Package: `0x53b6975e1e950a1fe3e9dd67b09eb1781b897b77c382ff60d102fbbc2d28fd99`
- Registry: `0x3f21ee2cbf9b70659f8d6c42a7f7aad9e315b11500830ab3e178aff95cc659ce`

**Key Functions**:
- `register_device()` - Register hardware witness
- `submit_step_data()` - Submit verified step data
- `get_global_stats()` - Query statistics

---

## 🗄️ Database Schema

### devices table
```sql
device_id          TEXT PRIMARY KEY  -- Unique identifier
public_key         TEXT NOT NULL     -- Ed25519 public key (hex)
registered_at      INTEGER           -- Unix timestamp (ms)
last_seen          INTEGER           -- Last connection
firmware_version   TEXT              -- e.g. "v1.0"
total_steps        INTEGER           -- Cumulative steps
total_submissions  INTEGER           -- Submission count
status             TEXT              -- active/inactive
```

### step_data table
```sql
id                   INTEGER PRIMARY KEY
device_id            TEXT              -- Foreign key
step_count           INTEGER           -- Steps in this batch
timestamp            INTEGER           -- Data timestamp
raw_samples          TEXT              -- JSON array of IMU data
battery_percent      INTEGER           -- Battery level
signature            TEXT              -- Ed25519 signature (hex)
verified             BOOLEAN           -- Signature verified
received_at          INTEGER           -- Server timestamp
submitted_to_chain   BOOLEAN           -- Blockchain status
tx_digest            TEXT              -- Sui transaction hash
```

---

## 🔄 State Transitions

### Device Lifecycle

```
┌──────────────┐
│  Unregistered│
└──────┬───────┘
       │ WebSocket: register
       │ Blockchain: register_device()
       ▼
┌──────────────┐
│  Registered  │
└──────┬───────┘
       │ WebSocket: authenticate
       ▼
┌──────────────┐
│ Authenticated│◄──┐
└──────┬───────┘   │
       │            │ Send step_data
       │ Submit     │
       │ step data  │
       ▼            │
┌──────────────┐   │
│   Active     │───┘
└──────┬───────┘
       │ Disconnect
       ▼
┌──────────────┐
│  Offline     │
└──────────────┘
```

### Step Data Lifecycle

```
┌──────────────┐
│  ESP32       │
│  Detected    │
└──────┬───────┘
       │ Sign + WebSocket
       ▼
┌──────────────┐
│  Server      │
│  Received    │
└──────┬───────┘
       │ Verify signature
       ▼
┌──────────────┐
│  Verified    │
│  (DB stored) │
└──────┬───────┘
       │ Scheduled batch (2 AM)
       ▼
┌──────────────┐
│  Blockchain  │
│  Pending     │
└──────┬───────┘
       │ submit_step_data()
       ▼
┌──────────────┐
│  On-Chain    │
│  (Immutable) │
└──────────────┘
```

---

## 📈 Performance Metrics

### Expected Throughput

| Metric | Value | Notes |
|--------|-------|-------|
| Devices per server | 100+ | WebSocket connections |
| Submissions/day/device | 10-20 | ~100 steps each |
| Signature verification | <10ms | tweetnacl library |
| Database write | <5ms | SQLite |
| Blockchain submission | 2-5s | Sui testnet |

### Resource Usage

| Component | CPU | Memory | Storage |
|-----------|-----|--------|---------|
| Backend Server | <10% | ~100 MB | Growing DB |
| ESP32 Firmware | ~20% | ~50 KB | NVS keys |
| Blockchain | N/A | N/A | ~300 bytes/record |

---

## 🚀 Deployment Guide

### 1. Deploy Smart Contract
```bash
cd /home/alvin/Esp32-s3/sui-watch-contracts/trust_oracle
sui client publish --gas-budget 100000000
# Save package ID and registry ID
```

### 2. Setup Backend Server
```bash
cd /home/alvin/Esp32-s3/trust-oracle-server
npm install
cp .env.example .env
# Edit .env with package ID, registry ID, mnemonic
npm start
```

### 3. Flash ESP32 Firmware
```cpp
// Update configuration:
const char* server_host = "YOUR_SERVER_IP";
const uint16_t server_port = 8080;
```

Upload to ESP32 via Arduino IDE or PlatformIO.

---

## 🔍 Monitoring & Debugging

### Server Health Check
```bash
curl http://localhost:3001/
```

### Check Connected Devices
```bash
curl http://localhost:3001/api/devices
```

### View Pending Submissions
```bash
curl http://localhost:3001/api/step-data/pending
```

### Check Blockchain Stats
```bash
curl http://localhost:3001/api/oracle/stats
```

### ESP32 Serial Monitor
```
[WebSocket] Connected!
✓ Device registered successfully!
✓ Authenticated!
✓ Step data accepted! (Data ID: 42)
```

---

## 📚 Related Documentation

- [Backend Server README](./README.md)
- [ESP32 WebSocket Client](./ESP32_WEBSOCKET_CLIENT.md)
- [Smart Contract Deployment](../sui-watch-contracts/trust_oracle/DEPLOYMENT.md)
- [Test Results](../sui-watch-contracts/trust_oracle/TEST_RESULTS.md)
- [Data Structures](../src/sui-watch/idea/DATA_STRUCTURES.md)

---

**Last Updated**: 2025-01-19
**System Version**: 1.0.0
**Status**: Production Ready
