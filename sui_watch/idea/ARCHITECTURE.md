# SUI Watch - Hardware Witness Architecture

## Tổng quan

SUI Watch là một **Trust Oracle** cho dữ liệu vật lý, biến ESP32 thành một "Hardware Witness" với khả năng ký điện tử dữ liệu sensor ngay tại phần cứng.

## Kiến trúc hệ thống

```
┌─────────────────────────────────────────────────────────────┐
│                     ESP32-S3 (Hardware Witness)              │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌──────────┐   │
│  │ QMI8658  │  │   RTC    │  │  LVGL    │  │  WiFi    │   │
│  │   IMU    │  │  Clock   │  │   UI     │  │  Stack   │   │
│  └────┬─────┘  └────┬─────┘  └────┬─────┘  └────┬─────┘   │
│       │             │              │             │          │
│  ┌────▼─────────────▼──────────────▼─────────────▼─────┐   │
│  │           Main Application Logic                     │   │
│  │  - Step Detection (detectStep())                     │   │
│  │  - Time Management (NTP Sync)                        │   │
│  │  - Hardware Signing (Ed25519)                        │   │
│  │  - Data Queue Management                             │   │
│  └───────────────────────┬──────────────────────────────┘   │
│                          │                                   │
│  ┌───────────────────────▼──────────────────────────────┐   │
│  │         Cryptographic Module (MicroSui)              │   │
│  │  - Ed25519 Key Management                            │   │
│  │  - Signature Generation                              │   │
│  │  - Payload Construction                              │   │
│  └───────────────────────┬──────────────────────────────┘   │
└────────────────────────────┬────────────────────────────────┘
                             │ HTTPS/JSON
                             ▼
┌─────────────────────────────────────────────────────────────┐
│                   SUI Watch Server (Backend)                 │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐      │
│  │   Express    │  │  Signature   │  │   SQLite     │      │
│  │   Router     │  │  Validator   │  │   Database   │      │
│  └──────┬───────┘  └──────┬───────┘  └──────┬───────┘      │
│         │                 │                  │              │
│  ┌──────▼─────────────────▼──────────────────▼─────────┐   │
│  │           Data Aggregator & Queue                   │   │
│  │  - Receive signed data from devices                 │   │
│  │  - Verify Ed25519 signatures                        │   │
│  │  - Store valid data                                 │   │
│  │  - Batch aggregation (every 24h)                    │   │
│  └──────────────────────┬───────────────────────────────┘   │
│                         │                                    │
│  ┌──────────────────────▼───────────────────────────────┐   │
│  │           Oracle Submitter                           │   │
│  │  - Build Sui transactions                            │   │
│  │  - Call smart contract                               │   │
│  │  - Handle confirmations                              │   │
│  └──────────────────────┬───────────────────────────────┘   │
└───────────────────────────┬─────────────────────────────────┘
                            │ Sui SDK
                            ▼
┌─────────────────────────────────────────────────────────────┐
│                    Sui Blockchain (Testnet)                  │
│  ┌──────────────────────────────────────────────────────┐   │
│  │           Trust Oracle Smart Contract                │   │
│  │                                                       │   │
│  │  struct StepDataRecord {                             │   │
│  │    device_id: vector<u8>,                            │   │
│  │    step_count: u64,                                  │   │
│  │    timestamp: u64,                                   │   │
│  │    signature: vector<u8>,                            │   │
│  │    verified: bool                                    │   │
│  │  }                                                   │   │
│  │                                                       │   │
│  │  Functions:                                          │   │
│  │  - submit_step_data()                                │   │
│  │  - verify_signature()                                │   │
│  │  - get_device_stats()                                │   │
│  │  - mint_achievement_nft()                            │   │
│  └──────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────┘
```

## Luồng dữ liệu chính

### 1. Step Detection & Signing Flow

```
IMU Sensor (50ms interval)
  ↓
detectStep() algorithm
  ↓
Step counter incremented
  ↓
Every 100 steps OR 1 hour
  ↓
Build payload: {
  deviceId: "ESP32-XXXX",
  stepCount: 450,
  timestamp: 1735492800,
  rawSamples: [...accelerometer data...],
  firmwareVersion: "1.0.0"
}
  ↓
Sign with Ed25519 private key
  ↓
signature = Ed25519.sign(privateKey, SHA256(payload))
  ↓
Queue for upload: {payload, signature}
  ↓
Wait for WiFi connection
  ↓
POST to /api/step-data/submit
```

### 2. Backend Verification Flow

```
Receive POST /api/step-data/submit
  ↓
Extract {payload, signature}
  ↓
Lookup device public key from database
  ↓
Verify: Ed25519.verify(publicKey, payload, signature)
  ↓
If valid:
  - Store in database
  - Add to batch queue
  - Return 200 OK
If invalid:
  - Log attempt
  - Return 403 Forbidden
```

### 3. Blockchain Submission Flow

```
Batch aggregator (runs every 24h)
  ↓
Select all verified records from last 24h
  ↓
Group by device_id
  ↓
For each device:
  Build Sui transaction:
    - Call trust_oracle::submit_step_data()
    - Include: device_id, total_steps, signatures[]
  ↓
Sign transaction with server keypair
  ↓
Submit to Sui network
  ↓
Wait for confirmation
  ↓
Update database with tx_digest
  ↓
Mark records as "submitted"
```

## Tính năng bảo mật

### 1. Device Identity

- Mỗi ESP32 generates unique Ed25519 keypair at first boot
- Private key stored in ESP32 secure storage (NVS encrypted)
- Public key registered with backend server
- Device ID = first 8 bytes of public key hash

### 2. Data Integrity

- Every data batch signed with device private key
- Signature covers: deviceId + stepCount + timestamp + rawSamples
- Backend verifies signature before accepting data
- On-chain smart contract can also verify (optional)

### 3. Tamper Detection

- Include raw accelerometer samples in payload
- Backend can validate step detection algorithm
- Detect anomalies (e.g., 10,000 steps in 1 minute)
- Blacklist suspicious devices

### 4. Replay Attack Prevention

- Include monotonic timestamp in payload
- Backend rejects old timestamps
- Nonce system (optional): include nonce from server

## Các thành phần chính

### ESP32 Firmware

| Module | File | Chức năng |
|--------|------|-----------|
| Time Manager | `TimeManager.cpp/h` | NTP sync, RTC management |
| Step Signature | `StepSignature.cpp/h` | Hardware signing logic |
| Trust Oracle Client | `TrustOracle.cpp/h` | Backend communication |
| IMU Driver | `QMI8658.cpp/h` | Sensor data reading |
| UI Screens | `ui_Screen*.c` | LVGL interface |

### Backend Server

| Module | File | Chức năng |
|--------|------|-----------|
| Main Server | `server.mjs` | Express app, routes |
| Device Manager | `deviceManager.mjs` | Device registration |
| Data Validator | `dataValidator.mjs` | Signature verification |
| Step Aggregator | `stepDataAggregator.mjs` | Data collection |
| Oracle Submitter | `oracleSubmitter.mjs` | Blockchain submission |

### Smart Contract

| Module | File | Chức năng |
|--------|------|-----------|
| Trust Oracle | `trust_oracle.move` | Core contract |
| Step NFT | `step_nft.move` | Achievement NFTs |

## Công nghệ sử dụng

### ESP32 Side
- **Platform**: ESP32-S3 with PSRAM
- **Crypto**: MicroSui library (Ed25519)
- **UI**: LVGL 8.x
- **Sensor**: QMI8658 6-axis IMU
- **Network**: WiFi (802.11 b/g/n)
- **Storage**: NVS (encrypted)

### Backend Side
- **Runtime**: Node.js 18+
- **Framework**: Express.js
- **Blockchain**: @mysten/sui SDK
- **Database**: SQLite3
- **Crypto**: Node.js crypto (Ed25519)

### Blockchain Side
- **Network**: Sui Testnet
- **Language**: Move
- **Contract Type**: Shared object
- **Gas Model**: Sponsored transactions (optional)

## Performance Targets

| Metric | Target | Current |
|--------|--------|---------|
| Step detection accuracy | > 95% | ~90% (needs tuning) |
| Signature generation time | < 100ms | TBD |
| Data upload frequency | Every 1 hour | - |
| Backend throughput | 100 devices/sec | - |
| Blockchain confirmation | < 5 seconds | ~3 seconds |
| Battery life | > 24 hours | TBD |

## Security Considerations

### Threat Model

| Threat | Mitigation |
|--------|-----------|
| Key extraction from ESP32 | Use secure boot + flash encryption |
| Man-in-the-middle attack | HTTPS only, certificate pinning |
| Device impersonation | Ed25519 signatures |
| Data tampering | Cryptographic signatures |
| Replay attacks | Timestamps + nonces |
| Sybil attack | Device registration + rate limiting |

## Future Enhancements

1. **Multi-signature**: Require multiple devices to confirm same event
2. **Zero-knowledge proofs**: Prove step count without revealing exact data
3. **Decentralized storage**: IPFS for raw sensor data
4. **Cross-chain**: Submit to multiple blockchains
5. **Hardware TEE**: Use ESP32's secure enclave
6. **ML-based validation**: Detect fake step patterns

## References

- [Sui Move Documentation](https://docs.sui.io/build/move)
- [Ed25519 Signature Scheme](https://ed25519.cr.yp.to/)
- [QMI8658 Datasheet](https://www.qstcorp.com/en_comp_prod/QMI8658)
- [ESP32-S3 Security Features](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/security/index.html)
