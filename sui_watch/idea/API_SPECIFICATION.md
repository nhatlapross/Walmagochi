# SUI Watch API Specification

## Base URL
```
Production: https://api.sui-watch.io
Development: http://localhost:3001
```

---

## Authentication
Currently no authentication required. In production, consider:
- API keys for devices
- Rate limiting
- IP whitelist

---

## Device Management Endpoints

### POST /api/devices/register
Register a new Hardware Witness device.

**Request Body**:
```json
{
  "publicKey": "0123456789abcdef..." // 64 hex characters (32 bytes)
}
```

**Response 200 OK**:
```json
{
  "success": true,
  "deviceId": "a1b2c3d4e5f6g7h8",
  "registered": true
}
```

**Response 400 Bad Request**:
```json
{
  "success": false,
  "error": "Invalid public key format"
}
```

---

### GET /api/devices
List all registered devices.

**Response 200 OK**:
```json
{
  "success": true,
  "count": 5,
  "devices": [
    {
      "device_id": "a1b2c3d4e5f6g7h8",
      "public_key": "0123...",
      "registered_at": 1735492800000,
      "last_seen": 1735579200000,
      "total_steps": 12450,
      "total_submissions": 5,
      "status": "active"
    }
  ]
}
```

---

### GET /api/devices/:deviceId
Get specific device information.

**Response 200 OK**:
```json
{
  "success": true,
  "device": {
    "device_id": "a1b2c3d4e5f6g7h8",
    "public_key": "0123...",
    "registered_at": 1735492800000,
    "last_seen": 1735579200000,
    "total_steps": 12450,
    "total_submissions": 5,
    "status": "active"
  }
}
```

**Response 404 Not Found**:
```json
{
  "success": false,
  "error": "Device not found"
}
```

---

### GET /api/devices/:deviceId/stats
Get device statistics.

**Response 200 OK**:
```json
{
  "success": true,
  "stats": {
    "device_id": "a1b2c3d4e5f6g7h8",
    "total_steps": 12450,
    "total_submissions": 5,
    "pending_submissions": 2,
    "last_step_time": 1735579200,
    "average_steps_per_submission": 2490
  }
}
```

---

## Step Data Endpoints

### POST /api/step-data/submit
Submit signed step data from device.

**Request Body**:
```json
{
  "deviceId": "a1b2c3d4e5f6g7h8",
  "stepCount": 450,
  "timestamp": 1735492800,
  "firmwareVersion": 100,
  "batteryPercent": 85,
  "rawAccSamples": [
    [100.5, 50.2, -980.3],
    [102.1, 51.0, -982.1]
  ],
  "signature": "0x1234567890abcdef..."
}
```

**Response 200 OK**:
```json
{
  "success": true,
  "message": "Step data recorded",
  "dataId": 123
}
```

**Response 403 Forbidden** (Invalid Signature):
```json
{
  "success": false,
  "error": "Invalid signature",
  "code": "VERIFICATION_FAILED"
}
```

**Response 400 Bad Request** (Duplicate):
```json
{
  "success": false,
  "error": "Duplicate submission",
  "code": "DUPLICATE"
}
```

---

### GET /api/step-data/:deviceId
Get step data history for device.

**Query Parameters**:
- `limit` (optional, default: 100): Number of records to return
- `offset` (optional, default: 0): Pagination offset

**Response 200 OK**:
```json
{
  "success": true,
  "count": 5,
  "data": [
    {
      "id": 123,
      "device_id": "a1b2c3d4e5f6g7h8",
      "step_count": 450,
      "timestamp": 1735492800,
      "battery_percent": 85,
      "signature": "0x1234...",
      "verified": true,
      "received_at": 1735492850,
      "submitted_to_chain": true,
      "tx_digest": "abc123..."
    }
  ]
}
```

---

### GET /api/step-data/pending
Get all pending (not yet on blockchain) submissions.

**Response 200 OK**:
```json
{
  "success": true,
  "count": 10,
  "data": [
    {
      "id": 125,
      "device_id": "a1b2c3d4e5f6g7h8",
      "step_count": 450,
      "timestamp": 1735492800,
      "verified": true,
      "submitted_to_chain": false
    }
  ]
}
```

---

## Oracle Submission Endpoints

### POST /api/oracle/submit-batch
Manually trigger batch submission to blockchain.

**Request Body**: None

**Response 200 OK**:
```json
{
  "success": true,
  "submitted": 5,
  "total": 5,
  "results": [
    {
      "success": true,
      "deviceId": "a1b2c3d4e5f6g7h8",
      "txDigest": "abc123...",
      "totalSteps": 2450,
      "recordCount": 5
    }
  ]
}
```

**Response 500 Internal Server Error**:
```json
{
  "success": false,
  "error": "Blockchain submission failed"
}
```

---

### GET /api/oracle/submissions
Get blockchain submission history.

**Query Parameters**:
- `limit` (optional, default: 100): Number of records to return

**Response 200 OK**:
```json
{
  "success": true,
  "submissions": [
    {
      "id": 1,
      "tx_digest": "abc123...",
      "device_ids": ["a1b2c3d4e5f6g7h8"],
      "total_steps": 2450,
      "submitted_at": 1735492800,
      "confirmed": true,
      "confirmed_at": 1735492805,
      "checkpoint": "12345",
      "gas_used": 500000
    }
  ]
}
```

---

## Statistics Endpoints

### GET /api/stats
Get global oracle statistics.

**Response 200 OK**:
```json
{
  "success": true,
  "stats": {
    "total_devices": 10,
    "total_submissions": 150,
    "total_steps": 125000,
    "avg_steps_per_submission": 833,
    "on_chain_count": 140,
    "pending_count": 10
  }
}
```

---

## Error Codes

| Code | HTTP Status | Description |
|------|-------------|-------------|
| `VERIFICATION_FAILED` | 403 | Signature verification failed |
| `DUPLICATE` | 400 | Duplicate submission detected |
| `DEVICE_NOT_FOUND` | 404 | Device not registered |
| `INVALID_TIMESTAMP` | 400 | Timestamp out of range |
| `INVALID_STEP_COUNT` | 400 | Step count invalid |
| `SERVER_ERROR` | 500 | Internal server error |

---

## Rate Limiting
- **Global**: 100 requests per minute per IP
- **Submit Endpoint**: 10 requests per minute per device
- **Retry-After**: Header included in 429 responses

---

## WebSocket API (Future)
Real-time updates for:
- New step submissions
- Blockchain confirmations
- Milestone achievements

```javascript
const ws = new WebSocket('wss://api.sui-watch.io/ws');

ws.on('message', (data) => {
  const event = JSON.parse(data);
  console.log(event);
  // { type: 'step_submitted', deviceId: '...', steps: 450 }
});
```

---

## SDK Examples

### JavaScript/Node.js
```javascript
import axios from 'axios';

const API_BASE = 'http://localhost:3001';

// Register device
async function registerDevice(publicKey) {
  const res = await axios.post(`${API_BASE}/api/devices/register`, {
    publicKey
  });
  return res.data;
}

// Submit step data
async function submitStepData(payload) {
  const res = await axios.post(`${API_BASE}/api/step-data/submit`, payload);
  return res.data;
}
```

### Python
```python
import requests

API_BASE = 'http://localhost:3001'

def register_device(public_key):
    res = requests.post(f'{API_BASE}/api/devices/register', json={
        'publicKey': public_key
    })
    return res.json()

def submit_step_data(payload):
    res = requests.post(f'{API_BASE}/api/step-data/submit', json=payload)
    return res.json()
```

### Arduino/ESP32 (C++)
```cpp
#include <HTTPClient.h>
#include <ArduinoJson.h>

const char* API_BASE = "http://192.168.1.11:3001";

bool submitStepData(const String& jsonPayload) {
    HTTPClient http;
    http.begin(String(API_BASE) + "/api/step-data/submit");
    http.addHeader("Content-Type", "application/json");

    int httpCode = http.POST(jsonPayload);
    bool success = (httpCode == 200);

    http.end();
    return success;
}
```
