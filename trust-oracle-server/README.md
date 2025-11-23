# Trust Oracle Backend Server

Backend server for **SUI Watch Trust Oracle** - Handles ESP32 device connections, Ed25519 signature verification, and Sui blockchain integration.

## 🚀 Features

- **WebSocket Server** - Real-time communication with ESP32 devices
- **Ed25519 Verification** - Cryptographic signature verification for step data
- **Device Management** - SQLite database for device registry and step data
- **Sui Integration** - Automated blockchain submissions
- **REST API** - Management and monitoring endpoints
- **Scheduled Batch Submissions** - Daily at 2 AM (configurable)

---

## 📋 Prerequisites

- Node.js 18+
- npm or yarn
- Sui wallet with testnet funds (for blockchain submissions)

---

## 🛠️ Installation

### 1. Clone & Navigate
```bash
cd /home/alvin/Esp32-s3/trust-oracle-server
```

### 2. Install Dependencies
```bash
npm install
```

### 3. Configure Environment
```bash
cp .env.example .env
# Edit .env with your configuration
```

**Required Configuration**:

First, export your Sui private key:
```bash
# List your keys
sui keytool list

# Export private key (replace with your key alias/address)
sui keytool export --key-identity 0xYourAddress --json
```

Then update `.env`:
```env
# Sui blockchain
SUI_PACKAGE_ID=0x53b6975e1e950a1fe3e9dd67b09eb1781b897b77c382ff60d102fbbc2d28fd99
SUI_REGISTRY_ID=0x3f21ee2cbf9b70659f8d6c42a7f7aad9e315b11500830ab3e178aff95cc659ce
SUI_PRIVATE_KEY=suiprivkey1... (base64 encoded from sui keytool export)
```

### 4. Start Server
```bash
# Production
npm start

# Development (auto-reload)
npm run dev
```

---

## 🌐 API Endpoints

### REST API (Port 3001)

#### Health Check
```bash
GET /
```

Response:
```json
{
  "status": "ok",
  "service": "Trust Oracle Backend Server",
  "version": "1.0.0",
  "network": "testnet",
  "stats": {
    "total_devices": 1,
    "total_submissions": 5,
    "total_steps": 1250,
    "pending_submissions": 2,
    "connected_devices": 1
  }
}
```

#### Get All Devices
```bash
GET /api/devices
```

#### Get Device by ID
```bash
GET /api/devices/:deviceId
```

#### Get Pending Step Data
```bash
GET /api/step-data/pending?deviceId=xxx
```

#### Manual Blockchain Submission
```bash
POST /api/oracle/submit-batch
```

#### Get Registry Stats
```bash
GET /api/oracle/stats
```

#### Get Server Balance
```bash
GET /api/oracle/balance
```

---

## 🌐 WebSocket Protocol

### Connection
```javascript
const ws = new WebSocket('ws://localhost:8080');
```

### Message Types

#### 1. Register Device
**Client → Server**:
```json
{
  "type": "register",
  "deviceId": "test_device_01",
  "publicKey": "0x0102030405..."
}
```

**Server → Client**:
```json
{
  "type": "register_response",
  "success": true,
  "device": {
    "device_id": "test_device_01",
    "public_key": "0x0102030405...",
    "registered_at": 1735492800000
  },
  "blockchainResult": {
    "success": true,
    "txDigest": "...",
    "deviceObjectId": "0xabcd..."
  }
}
```

#### 2. Authenticate
**Client → Server**:
```json
{
  "type": "authenticate",
  "deviceId": "test_device_01"
}
```

**Server → Client**:
```json
{
  "type": "auth_response",
  "success": true,
  "deviceId": "test_device_01"
}
```

#### 3. Submit Step Data
**Client → Server**:
```json
{
  "type": "step_data",
  "stepCount": 450,
  "timestamp": 1735492800000,
  "firmwareVersion": 100,
  "batteryPercent": 85,
  "rawAccSamples": [[100.5, 50.2, -980.3], ...],
  "signature": "0x123456..."
}
```

**Server → Client**:
```json
{
  "type": "step_data_response",
  "success": true,
  "dataId": 42,
  "stepCount": 450,
  "verified": true
}
```

#### 4. Ping/Pong (Keep-Alive)
**Client → Server**:
```json
{
  "type": "ping"
}
```

**Server → Client**:
```json
{
  "type": "pong",
  "timestamp": 1735492800000
}
```

---

## 🔐 Signature Verification

### Data Signing Process (ESP32 Side)

1. **Build Payload** (without signature):
```json
{
  "deviceId": "test_device_01",
  "stepCount": 450,
  "timestamp": 1735492800000,
  "firmwareVersion": 100,
  "batteryPercent": 85,
  "rawAccSamples": [[100.5, 50.2, -980.3], ...]
}
```

2. **Create Canonical JSON** (sorted keys):
```json
{"batteryPercent":85,"deviceId":"test_device_01","firmwareVersion":100,"rawAccSamples":[[100.5,50.2,-980.3]],"stepCount":450,"timestamp":1735492800000}
```

3. **Hash with SHA256**:
```javascript
const hash = SHA256(canonicalJson);
```

4. **Sign with Ed25519**:
```javascript
const signature = ed25519_sign(hash, privateKey);
```

5. **Send with Signature**:
```json
{
  ...payload,
  "signature": "0x123456..."
}
```

### Verification Process (Server Side)

Server automatically:
1. Extracts payload (without signature)
2. Builds canonical JSON
3. Hashes with SHA256
4. Verifies signature with device's public key
5. Stores if valid, rejects if invalid

---

## 📊 Database Schema

### devices
```sql
CREATE TABLE devices (
    device_id TEXT PRIMARY KEY,
    public_key TEXT NOT NULL UNIQUE,
    registered_at INTEGER NOT NULL,
    last_seen INTEGER,
    firmware_version TEXT,
    total_steps INTEGER DEFAULT 0,
    total_submissions INTEGER DEFAULT 0,
    status TEXT DEFAULT 'active'
);
```

### step_data
```sql
CREATE TABLE step_data (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    device_id TEXT NOT NULL,
    step_count INTEGER NOT NULL,
    timestamp INTEGER NOT NULL,
    raw_samples TEXT,
    battery_percent INTEGER,
    signature TEXT NOT NULL,
    verified BOOLEAN DEFAULT FALSE,
    received_at INTEGER NOT NULL,
    submitted_to_chain BOOLEAN DEFAULT FALSE,
    tx_digest TEXT,
    FOREIGN KEY (device_id) REFERENCES devices(device_id)
);
```

---

## ⏰ Automated Batch Submissions

Server automatically submits pending step data to blockchain:
- **Schedule**: Daily at 2:00 AM (configurable with `node-cron`)
- **Process**:
  1. Query all pending (not submitted) step data
  2. Group by device
  3. Aggregate steps, timestamps, signatures
  4. Submit to blockchain via `submit_step_data()`
  5. Mark as submitted with transaction digest

Manual trigger:
```bash
curl -X POST http://localhost:3001/api/oracle/submit-batch
```

---

## 🧪 Testing

### Test with curl

#### Register Device
```bash
curl -X POST http://localhost:3001/api/devices/register \
  -H "Content-Type: application/json" \
  -d '{"deviceId": "test_device_01", "publicKey": "0x0102030405..."}'
```

#### Get Devices
```bash
curl http://localhost:3001/api/devices
```

### Test with WebSocket (Node.js)

```javascript
import WebSocket from 'ws';

const ws = new WebSocket('ws://localhost:8080');

ws.on('open', () => {
    // Register device
    ws.send(JSON.stringify({
        type: 'register',
        deviceId: 'test_device_01',
        publicKey: '0x0102030405...'
    }));
});

ws.on('message', (data) => {
    console.log('Received:', JSON.parse(data.toString()));
});
```

---

## 📦 Project Structure

```
trust-oracle-server/
├── src/
│   ├── server.mjs              # Main server
│   ├── deviceManager.mjs       # Device & DB management
│   ├── cryptoManager.mjs       # Ed25519 verification
│   └── suiClient.mjs           # Sui blockchain client
├── data/
│   └── devices.db              # SQLite database (auto-created)
├── package.json
├── .env.example
└── README.md
```

---

## 🔧 Configuration

### Environment Variables

| Variable | Description | Required |
|----------|-------------|----------|
| `PORT` | HTTP server port | No (default: 3001) |
| `WS_PORT` | WebSocket server port | No (default: 8080) |
| `SUI_NETWORK` | Sui network (testnet/mainnet) | No (default: testnet) |
| `SUI_PACKAGE_ID` | Trust Oracle package ID | Yes |
| `SUI_REGISTRY_ID` | OracleRegistry object ID | Yes |
| `SUI_MNEMONIC` | Server wallet mnemonic | Yes (for blockchain) |

---

## 🚦 Monitoring

### Check Server Status
```bash
curl http://localhost:3001/
```

### Check Connected Devices
```bash
curl http://localhost:3001/api/devices
```

### Check Pending Submissions
```bash
curl http://localhost:3001/api/step-data/pending
```

### Check Blockchain Stats
```bash
curl http://localhost:3001/api/oracle/stats
```

### Check Server Balance
```bash
curl http://localhost:3001/api/oracle/balance
```

---

## 🐛 Troubleshooting

### "Blockchain integration disabled"
- Ensure `SUI_PACKAGE_ID` and `SUI_REGISTRY_ID` are set in `.env`
- Verify package and registry IDs are correct

### "Invalid signature" errors
- Verify device public key matches the one used for signing
- Ensure canonical JSON format is consistent (sorted keys)
- Check that payload excludes the signature field when verifying

### Database errors
- Check write permissions for `./data/` directory
- Verify SQLite is properly installed

### WebSocket connection fails
- Ensure port 8080 is not blocked by firewall
- Check if another service is using the port
- Verify ESP32 is connecting to correct IP:PORT

---

## 📚 Related Documentation

- [Trust Oracle Smart Contract](../sui-watch-contracts/trust_oracle/DEPLOYMENT.md)
- [ESP32 Integration Guide](../src/sui-watch/idea/ESP32_TASKS.md)
- [Data Structures Spec](../src/sui-watch/idea/DATA_STRUCTURES.md)
- [API Specification](../src/sui-watch/idea/API_SPECIFICATION.md)

---

## 📄 License

MIT License

---

## 👥 Team

**sui-watch team**
- Hardware Witness Architecture
- Trust Oracle Implementation
- ESP32 Firmware Integration

---

**Last Updated**: 2025-01-19
**Version**: 1.0.0
**Status**: Production Ready
