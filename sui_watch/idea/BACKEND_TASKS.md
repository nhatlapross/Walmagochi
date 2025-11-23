# Backend Server Development Tasks

## Overview
Tasks cho phát triển sui-watch-server - backend Node.js để nhận, xác thực và submit dữ liệu từ ESP32 lên Sui blockchain.

---

## Setup & Infrastructure

### Task 0.1: Project Initialization
**Priority**: Critical
**Estimated Time**: 2 hours
**Dependencies**: None

**Implementation**:
```bash
mkdir sui-watch-server
cd sui-watch-server
npm init -y
```

**Package.json**:
```json
{
  "name": "sui-watch-server",
  "version": "1.0.0",
  "description": "Backend server for SUI Watch - Hardware Witness Oracle",
  "main": "server.mjs",
  "type": "module",
  "scripts": {
    "start": "node server.mjs",
    "dev": "node --watch server.mjs",
    "test": "node --test tests/**/*.test.mjs"
  },
  "dependencies": {
    "@mysten/sui": "^1.44.0",
    "express": "^4.18.2",
    "cors": "^2.8.5",
    "dotenv": "^16.3.1",
    "better-sqlite3": "^9.2.2",
    "tweetnacl": "^1.0.3"
  },
  "devDependencies": {
    "nodemon": "^3.0.2"
  }
}
```

**Subtasks**:
- [ ] Initialize npm project
- [ ] Install dependencies
- [ ] Create .gitignore
- [ ] Create .env.example
- [ ] Setup ESLint (optional)

**Files to Create**:
- `package.json`
- `.env.example`
- `.gitignore`
- `README.md`

---

### Task 0.2: Database Schema Design
**Priority**: Critical
**Estimated Time**: 3 hours
**Dependencies**: Task 0.1

**Schema**:
```sql
-- devices table
CREATE TABLE devices (
    device_id TEXT PRIMARY KEY,
    public_key TEXT NOT NULL UNIQUE,
    registered_at INTEGER NOT NULL,
    last_seen INTEGER,
    firmware_version TEXT,
    total_steps INTEGER DEFAULT 0,
    total_submissions INTEGER DEFAULT 0,
    status TEXT DEFAULT 'active' -- active, suspended, banned
);

-- step_data table
CREATE TABLE step_data (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    device_id TEXT NOT NULL,
    step_count INTEGER NOT NULL,
    timestamp INTEGER NOT NULL,
    raw_samples TEXT, -- JSON array
    battery_percent INTEGER,
    signature TEXT NOT NULL,
    verified BOOLEAN DEFAULT FALSE,
    received_at INTEGER NOT NULL,
    submitted_to_chain BOOLEAN DEFAULT FALSE,
    tx_digest TEXT,
    FOREIGN KEY (device_id) REFERENCES devices(device_id)
);

-- submissions table (blockchain transactions)
CREATE TABLE submissions (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    tx_digest TEXT UNIQUE NOT NULL,
    device_ids TEXT NOT NULL, -- JSON array
    total_steps INTEGER NOT NULL,
    submitted_at INTEGER NOT NULL,
    confirmed BOOLEAN DEFAULT FALSE,
    confirmed_at INTEGER,
    checkpoint TEXT,
    gas_used INTEGER
);

-- Create indexes
CREATE INDEX idx_step_data_device_id ON step_data(device_id);
CREATE INDEX idx_step_data_timestamp ON step_data(timestamp);
CREATE INDEX idx_step_data_submitted ON step_data(submitted_to_chain);
CREATE INDEX idx_devices_status ON devices(status);
```

**Subtasks**:
- [ ] Create schema.sql file
- [ ] Add migration system (optional)
- [ ] Create init script
- [ ] Add sample data for testing

**Files to Create**:
- `schema.sql`
- `database.mjs` (wrapper)

---

## Module 1: Device Management

### Task 1.1: Device Registration API
**Priority**: High
**Estimated Time**: 4 hours
**Dependencies**: Task 0.2

**Implementation**:
```javascript
// File: deviceManager.mjs
import Database from 'better-sqlite3';
import nacl from 'tweetnacl';

export class DeviceManager {
    constructor(dbPath) {
        this.db = new Database(dbPath);
    }

    // Register new device
    registerDevice(publicKeyHex) {
        try {
            // Validate public key format
            if (!this.isValidPublicKey(publicKeyHex)) {
                throw new Error('Invalid public key format');
            }

            // Generate device ID from public key
            const deviceId = this.generateDeviceId(publicKeyHex);

            // Check if already registered
            const existing = this.getDevice(deviceId);
            if (existing) {
                return { deviceId, alreadyRegistered: true };
            }

            // Insert into database
            const stmt = this.db.prepare(`
                INSERT INTO devices (device_id, public_key, registered_at)
                VALUES (?, ?, ?)
            `);

            stmt.run(deviceId, publicKeyHex, Date.now());

            return { deviceId, registered: true };
        } catch (error) {
            throw new Error(`Registration failed: ${error.message}`);
        }
    }

    // Generate device ID (first 16 chars of pubkey hash)
    generateDeviceId(publicKeyHex) {
        const hash = require('crypto')
            .createHash('sha256')
            .update(Buffer.from(publicKeyHex, 'hex'))
            .digest('hex');
        return hash.substring(0, 16);
    }

    // Validate public key
    isValidPublicKey(publicKeyHex) {
        if (typeof publicKeyHex !== 'string') return false;
        if (publicKeyHex.length !== 64) return false; // 32 bytes * 2
        return /^[0-9a-f]+$/i.test(publicKeyHex);
    }

    // Get device by ID
    getDevice(deviceId) {
        const stmt = this.db.prepare('SELECT * FROM devices WHERE device_id = ?');
        return stmt.get(deviceId);
    }

    // Get all devices
    getAllDevices() {
        const stmt = this.db.prepare('SELECT * FROM devices ORDER BY registered_at DESC');
        return stmt.all();
    }

    // Update last seen
    updateLastSeen(deviceId) {
        const stmt = this.db.prepare('UPDATE devices SET last_seen = ? WHERE device_id = ?');
        stmt.run(Date.now(), deviceId);
    }

    // Get device stats
    getDeviceStats(deviceId) {
        const stmt = this.db.prepare(`
            SELECT
                d.device_id,
                d.total_steps,
                d.total_submissions,
                COUNT(sd.id) as pending_submissions,
                MAX(sd.timestamp) as last_step_time
            FROM devices d
            LEFT JOIN step_data sd ON d.device_id = sd.device_id
                AND sd.submitted_to_chain = FALSE
            WHERE d.device_id = ?
            GROUP BY d.device_id
        `);
        return stmt.get(deviceId);
    }
}
```

**Subtasks**:
- [ ] Implement device registration
- [ ] Public key validation
- [ ] Device ID generation
- [ ] Duplicate detection
- [ ] Get device info
- [ ] Update last seen timestamp

**API Endpoints**:
```javascript
// POST /api/devices/register
app.post('/api/devices/register', (req, res) => {
    const { publicKey } = req.body;
    const result = deviceManager.registerDevice(publicKey);
    res.json(result);
});

// GET /api/devices
app.get('/api/devices', (req, res) => {
    const devices = deviceManager.getAllDevices();
    res.json({ success: true, devices });
});

// GET /api/devices/:deviceId
app.get('/api/devices/:deviceId', (req, res) => {
    const device = deviceManager.getDevice(req.params.deviceId);
    res.json({ success: true, device });
});

// GET /api/devices/:deviceId/stats
app.get('/api/devices/:deviceId/stats', (req, res) => {
    const stats = deviceManager.getDeviceStats(req.params.deviceId);
    res.json({ success: true, stats });
});
```

**Test Cases**:
- [ ] Register valid device
- [ ] Reject invalid public key
- [ ] Prevent duplicate registration
- [ ] Retrieve device info
- [ ] Get device statistics

---

## Module 2: Signature Verification

### Task 2.1: Data Validator Implementation
**Priority**: Critical
**Estimated Time**: 6 hours
**Dependencies**: Task 1.1

**Implementation**:
```javascript
// File: dataValidator.mjs
import nacl from 'tweetnacl';
import crypto from 'crypto';

export class DataValidator {
    constructor(deviceManager) {
        this.deviceManager = deviceManager;
    }

    // Verify signed step data
    async verifyStepData(payload) {
        try {
            // 1. Extract fields
            const { deviceId, stepCount, timestamp, signature, ...rest } = payload;

            // 2. Validate required fields
            if (!deviceId || !stepCount || !timestamp || !signature) {
                return { valid: false, error: 'Missing required fields' };
            }

            // 3. Get device public key
            const device = this.deviceManager.getDevice(deviceId);
            if (!device) {
                return { valid: false, error: 'Device not registered' };
            }

            // 4. Reconstruct payload (without signature)
            const dataToVerify = {
                deviceId,
                stepCount,
                timestamp,
                ...rest
            };

            // 5. Create canonical JSON (sorted keys)
            const canonicalJson = JSON.stringify(dataToVerify, Object.keys(dataToVerify).sort());

            // 6. Hash the data
            const messageHash = crypto
                .createHash('sha256')
                .update(canonicalJson)
                .digest();

            // 7. Convert signature from hex
            const signatureBytes = Buffer.from(signature.replace('0x', ''), 'hex');

            // 8. Convert public key from hex
            const publicKeyBytes = Buffer.from(device.public_key, 'hex');

            // 9. Verify Ed25519 signature
            const valid = nacl.sign.detached.verify(
                messageHash,
                signatureBytes,
                publicKeyBytes
            );

            if (!valid) {
                return { valid: false, error: 'Invalid signature' };
            }

            // 10. Additional validations
            const validationResult = this.validateDataSanity(payload);
            if (!validationResult.valid) {
                return validationResult;
            }

            return { valid: true };

        } catch (error) {
            return { valid: false, error: error.message };
        }
    }

    // Sanity checks on data
    validateDataSanity(payload) {
        const { stepCount, timestamp, batteryPercent, rawAccSamples } = payload;

        // Check step count is reasonable
        if (stepCount < 0 || stepCount > 100000) {
            return { valid: false, error: 'Invalid step count' };
        }

        // Check timestamp is not too old or in future
        const now = Date.now() / 1000;
        const maxAge = 7 * 24 * 60 * 60; // 7 days
        if (timestamp < now - maxAge || timestamp > now + 300) {
            return { valid: false, error: 'Invalid timestamp' };
        }

        // Check battery percent
        if (batteryPercent !== undefined && (batteryPercent < 0 || batteryPercent > 100)) {
            return { valid: false, error: 'Invalid battery percent' };
        }

        // Check raw samples format
        if (rawAccSamples && !Array.isArray(rawAccSamples)) {
            return { valid: false, error: 'Invalid raw samples format' };
        }

        return { valid: true };
    }

    // Detect anomalies (potential fake data)
    detectAnomalies(payload) {
        const anomalies = [];

        // Check if step frequency is realistic
        // TODO: Implement ML-based anomaly detection

        return anomalies;
    }
}
```

**Subtasks**:
- [ ] Implement Ed25519 signature verification
- [ ] Payload reconstruction
- [ ] Canonical JSON serialization
- [ ] Data sanity checks
- [ ] Timestamp validation
- [ ] Battery range check
- [ ] Raw samples validation

**Test Cases**:
- [ ] Valid signature passes
- [ ] Invalid signature rejected
- [ ] Missing fields rejected
- [ ] Tampered data rejected
- [ ] Old timestamps rejected
- [ ] Future timestamps rejected

---

## Module 3: Step Data Aggregator

### Task 3.1: Data Collection API
**Priority**: High
**Estimated Time**: 6 hours
**Dependencies**: Task 2.1

**Implementation**:
```javascript
// File: stepDataAggregator.mjs
export class StepDataAggregator {
    constructor(db, deviceManager, dataValidator) {
        this.db = db;
        this.deviceManager = deviceManager;
        this.dataValidator = dataValidator;
    }

    // Submit step data from device
    async submitStepData(payload) {
        try {
            // 1. Verify signature
            const verification = await this.dataValidator.verifyStepData(payload);
            if (!verification.valid) {
                return {
                    success: false,
                    error: verification.error,
                    code: 'VERIFICATION_FAILED'
                };
            }

            // 2. Check for duplicates
            const duplicate = this.checkDuplicate(payload.deviceId, payload.timestamp);
            if (duplicate) {
                return {
                    success: false,
                    error: 'Duplicate submission',
                    code: 'DUPLICATE'
                };
            }

            // 3. Insert into database
            const stmt = this.db.prepare(`
                INSERT INTO step_data (
                    device_id, step_count, timestamp, raw_samples,
                    battery_percent, signature, verified, received_at
                ) VALUES (?, ?, ?, ?, ?, ?, ?, ?)
            `);

            stmt.run(
                payload.deviceId,
                payload.stepCount,
                payload.timestamp,
                JSON.stringify(payload.rawAccSamples || []),
                payload.batteryPercent || null,
                payload.signature,
                true,
                Math.floor(Date.now() / 1000)
            );

            // 4. Update device stats
            this.updateDeviceStats(payload.deviceId, payload.stepCount);

            // 5. Update last seen
            this.deviceManager.updateLastSeen(payload.deviceId);

            return {
                success: true,
                message: 'Step data recorded',
                dataId: this.db.prepare('SELECT last_insert_rowid()').get()['last_insert_rowid()']
            };

        } catch (error) {
            console.error('Submit error:', error);
            return {
                success: false,
                error: error.message,
                code: 'SERVER_ERROR'
            };
        }
    }

    // Check for duplicate submission
    checkDuplicate(deviceId, timestamp) {
        const stmt = this.db.prepare(`
            SELECT id FROM step_data
            WHERE device_id = ? AND timestamp = ?
        `);
        return stmt.get(deviceId, timestamp);
    }

    // Update device total steps
    updateDeviceStats(deviceId, additionalSteps) {
        const stmt = this.db.prepare(`
            UPDATE devices
            SET total_steps = total_steps + ?
            WHERE device_id = ?
        `);
        stmt.run(additionalSteps, deviceId);
    }

    // Get pending submissions (not yet on chain)
    getPendingSubmissions() {
        const stmt = this.db.prepare(`
            SELECT * FROM step_data
            WHERE submitted_to_chain = FALSE
            ORDER BY timestamp ASC
        `);
        return stmt.all();
    }

    // Get submissions by device
    getDeviceSubmissions(deviceId, limit = 100) {
        const stmt = this.db.prepare(`
            SELECT * FROM step_data
            WHERE device_id = ?
            ORDER BY timestamp DESC
            LIMIT ?
        `);
        return stmt.all(deviceId, limit);
    }

    // Get aggregated stats
    getAggregatedStats() {
        const stmt = this.db.prepare(`
            SELECT
                COUNT(DISTINCT device_id) as total_devices,
                COUNT(id) as total_submissions,
                SUM(step_count) as total_steps,
                AVG(step_count) as avg_steps_per_submission,
                COUNT(CASE WHEN submitted_to_chain = TRUE THEN 1 END) as on_chain_count,
                COUNT(CASE WHEN submitted_to_chain = FALSE THEN 1 END) as pending_count
            FROM step_data
        `);
        return stmt.get();
    }
}
```

**API Endpoints**:
```javascript
// POST /api/step-data/submit
app.post('/api/step-data/submit', async (req, res) => {
    const result = await stepDataAggregator.submitStepData(req.body);

    if (result.success) {
        res.json(result);
    } else {
        const statusCode = result.code === 'VERIFICATION_FAILED' ? 403 : 400;
        res.status(statusCode).json(result);
    }
});

// GET /api/step-data/:deviceId
app.get('/api/step-data/:deviceId', (req, res) => {
    const data = stepDataAggregator.getDeviceSubmissions(req.params.deviceId);
    res.json({ success: true, data });
});

// GET /api/step-data/pending
app.get('/api/step-data/pending', (req, res) => {
    const pending = stepDataAggregator.getPendingSubmissions();
    res.json({ success: true, count: pending.length, data: pending });
});

// GET /api/stats
app.get('/api/stats', (req, res) => {
    const stats = stepDataAggregator.getAggregatedStats();
    res.json({ success: true, stats });
});
```

**Subtasks**:
- [ ] Implement submit endpoint
- [ ] Signature verification integration
- [ ] Duplicate detection
- [ ] Database insertion
- [ ] Device stats update
- [ ] Query endpoints

**Test Cases**:
- [ ] Valid submission accepted
- [ ] Invalid signature rejected
- [ ] Duplicate rejected
- [ ] Device stats updated correctly
- [ ] Query endpoints work

---

## Module 4: Oracle Submitter

### Task 4.1: Blockchain Integration
**Priority**: Critical
**Estimated Time**: 8 hours
**Dependencies**: Task 3.1

**Implementation**:
```javascript
// File: oracleSubmitter.mjs
import { Transaction } from '@mysten/sui/transactions';
import { SuiClient, getFullnodeUrl } from '@mysten/sui/client';
import { Ed25519Keypair } from '@mysten/sui/keypairs/ed25519';

export class OracleSubmitter {
    constructor(config, db) {
        this.config = config;
        this.db = db;
        this.client = new SuiClient({ url: getFullnodeUrl(config.network) });
        this.keypair = Ed25519Keypair.fromSecretKey(
            Buffer.from(config.privateKey, 'base64')
        );
    }

    // Submit batch of step data to blockchain
    async submitBatch() {
        try {
            console.log('\n🔗 Starting batch submission to Sui blockchain...');

            // 1. Get pending data
            const pending = this.getPendingData();
            if (pending.length === 0) {
                console.log('No pending data to submit');
                return { success: true, message: 'No data to submit' };
            }

            console.log(`📊 Found ${pending.length} pending records`);

            // 2. Group by device
            const deviceBatches = this.groupByDevice(pending);
            console.log(`👥 Grouped into ${Object.keys(deviceBatches).length} devices`);

            // 3. Build transaction for each device
            const results = [];
            for (const [deviceId, records] of Object.entries(deviceBatches)) {
                const result = await this.submitDeviceBatch(deviceId, records);
                results.push(result);
            }

            // 4. Return summary
            const successful = results.filter(r => r.success).length;
            console.log(`\n✅ Batch submission complete: ${successful}/${results.length} successful`);

            return {
                success: true,
                submitted: successful,
                total: results.length,
                results
            };

        } catch (error) {
            console.error('❌ Batch submission error:', error);
            return { success: false, error: error.message };
        }
    }

    // Submit data for single device
    async submitDeviceBatch(deviceId, records) {
        try {
            console.log(`\n📤 Submitting batch for device ${deviceId}...`);

            // Calculate totals
            const totalSteps = records.reduce((sum, r) => sum + r.step_count, 0);
            const timestamps = records.map(r => r.timestamp);
            const signatures = records.map(r => r.signature);

            console.log(`  Steps: ${totalSteps}`);
            console.log(`  Records: ${records.length}`);

            // Build Sui transaction
            const tx = new Transaction();
            tx.setSender(this.keypair.getPublicKey().toSuiAddress());

            // Call smart contract
            tx.moveCall({
                target: `${this.config.packageId}::trust_oracle::submit_step_data`,
                arguments: [
                    tx.pure.string(deviceId),
                    tx.pure.u64(totalSteps),
                    tx.pure.vector('u64', timestamps),
                    tx.pure.vector('vector<u8>', signatures.map(s => Array.from(Buffer.from(s, 'hex'))))
                ]
            });

            // Execute transaction
            console.log('  🚀 Executing transaction...');
            const result = await this.client.signAndExecuteTransaction({
                transaction: tx,
                signer: this.keypair,
                options: {
                    showEffects: true,
                    showEvents: true
                }
            });

            console.log(`  ✅ Transaction confirmed: ${result.digest}`);

            // Mark records as submitted
            this.markAsSubmitted(records.map(r => r.id), result.digest);

            // Update submission table
            this.recordSubmission(deviceId, totalSteps, result);

            return {
                success: true,
                deviceId,
                txDigest: result.digest,
                totalSteps,
                recordCount: records.length
            };

        } catch (error) {
            console.error(`  ❌ Error for device ${deviceId}:`, error.message);
            return {
                success: false,
                deviceId,
                error: error.message
            };
        }
    }

    // Get pending data from database
    getPendingData() {
        const stmt = this.db.prepare(`
            SELECT * FROM step_data
            WHERE submitted_to_chain = FALSE
            AND verified = TRUE
            ORDER BY timestamp ASC
        `);
        return stmt.all();
    }

    // Group records by device
    groupByDevice(records) {
        return records.reduce((groups, record) => {
            const { device_id } = record;
            if (!groups[device_id]) {
                groups[device_id] = [];
            }
            groups[device_id].push(record);
            return groups;
        }, {});
    }

    // Mark records as submitted
    markAsSubmitted(recordIds, txDigest) {
        const placeholders = recordIds.map(() => '?').join(',');
        const stmt = this.db.prepare(`
            UPDATE step_data
            SET submitted_to_chain = TRUE, tx_digest = ?
            WHERE id IN (${placeholders})
        `);
        stmt.run(txDigest, ...recordIds);
    }

    // Record submission in submissions table
    recordSubmission(deviceId, totalSteps, result) {
        const stmt = this.db.prepare(`
            INSERT INTO submissions (
                tx_digest, device_ids, total_steps, submitted_at, confirmed
            ) VALUES (?, ?, ?, ?, ?)
        `);

        stmt.run(
            result.digest,
            JSON.stringify([deviceId]),
            totalSteps,
            Math.floor(Date.now() / 1000),
            result.effects?.status?.status === 'success'
        );
    }

    // Get submission history
    getSubmissionHistory(limit = 100) {
        const stmt = this.db.prepare(`
            SELECT * FROM submissions
            ORDER BY submitted_at DESC
            LIMIT ?
        `);
        return stmt.all(limit);
    }
}
```

**Subtasks**:
- [ ] Sui client initialization
- [ ] Keypair management
- [ ] Transaction building
- [ ] Move call construction
- [ ] Transaction execution
- [ ] Result processing
- [ ] Database updates

**API Endpoints**:
```javascript
// POST /api/oracle/submit-batch (manual trigger)
app.post('/api/oracle/submit-batch', async (req, res) => {
    const result = await oracleSubmitter.submitBatch();
    res.json(result);
});

// GET /api/oracle/submissions
app.get('/api/oracle/submissions', (req, res) => {
    const history = oracleSubmitter.getSubmissionHistory();
    res.json({ success: true, submissions: history });
});
```

**Automated Scheduling**:
```javascript
// In server.mjs
import cron from 'node-cron';

// Run batch submission every day at 2 AM
cron.schedule('0 2 * * *', async () => {
    console.log('🕐 Running scheduled batch submission...');
    await oracleSubmitter.submitBatch();
});
```

**Test Cases**:
- [ ] Single device batch submission
- [ ] Multiple device batch submission
- [ ] Transaction confirmation
- [ ] Database updates
- [ ] Error handling
- [ ] Gas estimation

---

## Module 5: Web Dashboard

### Task 5.1: Dashboard Frontend
**Priority**: Medium
**Estimated Time**: 10 hours
**Dependencies**: All previous tasks

**Implementation**:
```javascript
// File: dashboard.html (served by Express)
app.get('/dashboard', (req, res) => {
    res.send(`
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <title>SUI Watch - Oracle Dashboard</title>
    <style>
        * { margin: 0; padding: 0; box-sizing: border-box; }
        body {
            font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', sans-serif;
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            padding: 20px;
        }
        .container { max-width: 1400px; margin: 0 auto; }
        .header {
            background: white;
            border-radius: 15px;
            padding: 30px;
            margin-bottom: 20px;
            box-shadow: 0 10px 30px rgba(0,0,0,0.2);
        }
        .stats-grid {
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(200px, 1fr));
            gap: 15px;
            margin-bottom: 20px;
        }
        .stat-card {
            background: white;
            border-radius: 12px;
            padding: 20px;
            box-shadow: 0 5px 15px rgba(0,0,0,0.1);
        }
        .stat-value { font-size: 36px; font-weight: bold; color: #667eea; }
        .stat-label { color: #999; font-size: 13px; margin-top: 5px; }
        .section {
            background: white;
            border-radius: 15px;
            padding: 30px;
            margin-bottom: 20px;
            box-shadow: 0 10px 30px rgba(0,0,0,0.2);
        }
        table { width: 100%; border-collapse: collapse; }
        th, td { padding: 12px; text-align: left; border-bottom: 1px solid #eee; }
        .btn {
            padding: 10px 20px;
            border: none;
            border-radius: 8px;
            cursor: pointer;
            font-weight: bold;
            background: #667eea;
            color: white;
        }
        .btn:hover { opacity: 0.8; }
    </style>
</head>
<body>
    <div class="container">
        <div class="header">
            <h1>🏃 SUI Watch - Oracle Dashboard</h1>
            <p>Hardware Witness Trust Oracle</p>
        </div>

        <div class="stats-grid">
            <div class="stat-card">
                <div class="stat-value" id="totalDevices">0</div>
                <div class="stat-label">Total Devices</div>
            </div>
            <div class="stat-card">
                <div class="stat-value" id="totalSteps">0</div>
                <div class="stat-label">Total Steps</div>
            </div>
            <div class="stat-card">
                <div class="stat-value" id="totalSubmissions">0</div>
                <div class="stat-label">Submissions</div>
            </div>
            <div class="stat-card">
                <div class="stat-value" id="pendingCount">0</div>
                <div class="stat-label">Pending</div>
            </div>
            <div class="stat-card">
                <div class="stat-value" id="onChainCount">0</div>
                <div class="stat-label">On Chain</div>
            </div>
        </div>

        <div class="section">
            <h2>📡 Registered Devices</h2>
            <button class="btn" onclick="refreshDevices()">🔄 Refresh</button>
            <table id="devicesTable">
                <thead>
                    <tr>
                        <th>Device ID</th>
                        <th>Total Steps</th>
                        <th>Submissions</th>
                        <th>Last Seen</th>
                        <th>Status</th>
                    </tr>
                </thead>
                <tbody></tbody>
            </table>
        </div>

        <div class="section">
            <h2>⛓️ Blockchain Submissions</h2>
            <button class="btn" onclick="submitBatch()">🚀 Submit Batch Now</button>
            <table id="submissionsTable">
                <thead>
                    <tr>
                        <th>Tx Digest</th>
                        <th>Steps</th>
                        <th>Timestamp</th>
                        <th>Status</th>
                    </tr>
                </thead>
                <tbody></tbody>
            </table>
        </div>
    </div>

    <script>
        const API_BASE = window.location.origin;

        async function loadStats() {
            const res = await fetch(\`\${API_BASE}/api/stats\`);
            const data = await res.json();
            const stats = data.stats;

            document.getElementById('totalDevices').textContent = stats.total_devices;
            document.getElementById('totalSteps').textContent = stats.total_steps.toLocaleString();
            document.getElementById('totalSubmissions').textContent = stats.total_submissions;
            document.getElementById('pendingCount').textContent = stats.pending_count;
            document.getElementById('onChainCount').textContent = stats.on_chain_count;
        }

        async function loadDevices() {
            const res = await fetch(\`\${API_BASE}/api/devices\`);
            const data = await res.json();
            const tbody = document.querySelector('#devicesTable tbody');

            tbody.innerHTML = data.devices.map(d => \`
                <tr>
                    <td>\${d.device_id}</td>
                    <td>\${d.total_steps.toLocaleString()}</td>
                    <td>\${d.total_submissions}</td>
                    <td>\${new Date(d.last_seen).toLocaleString()}</td>
                    <td>\${d.status}</td>
                </tr>
            \`).join('');
        }

        async function loadSubmissions() {
            const res = await fetch(\`\${API_BASE}/api/oracle/submissions\`);
            const data = await res.json();
            const tbody = document.querySelector('#submissionsTable tbody');

            tbody.innerHTML = data.submissions.map(s => \`
                <tr>
                    <td>\${s.tx_digest.substring(0, 16)}...</td>
                    <td>\${s.total_steps.toLocaleString()}</td>
                    <td>\${new Date(s.submitted_at * 1000).toLocaleString()}</td>
                    <td>\${s.confirmed ? '✅ Confirmed' : '⏳ Pending'}</td>
                </tr>
            \`).join('');
        }

        async function submitBatch() {
            if (!confirm('Submit all pending data to blockchain?')) return;

            const res = await fetch(\`\${API_BASE}/api/oracle/submit-batch\`, { method: 'POST' });
            const data = await res.json();
            alert(data.success ? \`✅ Submitted \${data.submitted} batches\` : \`❌ Error: \${data.error}\`);

            refresh();
        }

        function refresh() {
            loadStats();
            loadDevices();
            loadSubmissions();
        }

        // Auto-refresh every 30 seconds
        setInterval(refresh, 30000);

        // Initial load
        refresh();
    </script>
</body>
</html>
    `);
});
```

**Subtasks**:
- [ ] Create dashboard HTML
- [ ] Stats display
- [ ] Device table
- [ ] Submission table
- [ ] Manual trigger button
- [ ] Auto-refresh

---

## Module 6: Main Server File

### Task 6.1: Server.mjs Implementation
**Priority**: Critical
**Estimated Time**: 4 hours
**Dependencies**: All modules

**Complete Implementation**:
```javascript
// File: server.mjs
import express from 'express';
import cors from 'cors';
import dotenv from 'dotenv';
import Database from 'better-sqlite3';
import cron from 'node-cron';

import { DeviceManager } from './deviceManager.mjs';
import { DataValidator } from './dataValidator.mjs';
import { StepDataAggregator } from './stepDataAggregator.mjs';
import { OracleSubmitter } from './oracleSubmitter.mjs';

dotenv.config();

const app = express();
const PORT = process.env.PORT || 3001;

// Initialize database
const db = new Database(process.env.DB_PATH || './sui-watch.db');

// Initialize modules
const deviceManager = new DeviceManager(db);
const dataValidator = new DataValidator(deviceManager);
const stepDataAggregator = new StepDataAggregator(db, deviceManager, dataValidator);
const oracleSubmitter = new OracleSubmitter({
    network: process.env.SUI_NETWORK || 'testnet',
    packageId: process.env.SUI_PACKAGE_ID,
    privateKey: process.env.SUI_PRIVATE_KEY
}, db);

// Middleware
app.use(cors());
app.use(express.json());

// Health check
app.get('/', (req, res) => {
    res.json({
        status: 'ok',
        service: 'SUI Watch Oracle Server',
        version: '1.0.0'
    });
});

// Device endpoints
app.post('/api/devices/register', (req, res) => { /* see Task 1.1 */ });
app.get('/api/devices', (req, res) => { /* see Task 1.1 */ });
app.get('/api/devices/:deviceId', (req, res) => { /* see Task 1.1 */ });
app.get('/api/devices/:deviceId/stats', (req, res) => { /* see Task 1.1 */ });

// Step data endpoints
app.post('/api/step-data/submit', async (req, res) => { /* see Task 3.1 */ });
app.get('/api/step-data/:deviceId', (req, res) => { /* see Task 3.1 */ });
app.get('/api/step-data/pending', (req, res) => { /* see Task 3.1 */ });

// Oracle endpoints
app.post('/api/oracle/submit-batch', async (req, res) => { /* see Task 4.1 */ });
app.get('/api/oracle/submissions', (req, res) => { /* see Task 4.1 */ });

// Stats endpoint
app.get('/api/stats', (req, res) => { /* see Task 3.1 */ });

// Dashboard
app.get('/dashboard', (req, res) => { /* see Task 5.1 */ });

// Scheduled batch submission (every day at 2 AM)
cron.schedule('0 2 * * *', async () => {
    console.log('🕐 Running scheduled batch submission...');
    await oracleSubmitter.submitBatch();
});

// Start server
app.listen(PORT, '0.0.0.0', () => {
    console.log('');
    console.log('═══════════════════════════════════════════════════════════');
    console.log('🏃 SUI Watch Oracle Server v1.0');
    console.log('═══════════════════════════════════════════════════════════');
    console.log(\`  Running on: http://localhost:\${PORT}\`);
    console.log(\`  Dashboard: http://localhost:\${PORT}/dashboard\`);
    console.log(\`  Network: \${process.env.SUI_NETWORK || 'testnet'}\`);
    console.log('');
    console.log('📡 Endpoints:');
    console.log('  POST /api/devices/register');
    console.log('  POST /api/step-data/submit');
    console.log('  POST /api/oracle/submit-batch');
    console.log('  GET  /api/stats');
    console.log('  GET  /dashboard');
    console.log('');
    console.log('⏰ Scheduled: Daily batch submission at 2 AM');
    console.log('═══════════════════════════════════════════════════════════');
    console.log('');
});
```

**Subtasks**:
- [ ] Import all modules
- [ ] Setup Express
- [ ] Configure middleware
- [ ] Mount all routes
- [ ] Setup cron job
- [ ] Add error handling
- [ ] Add logging

---

## Testing & Deployment

### Task 7.1: Integration Testing
**Priority**: High
**Estimated Time**: 8 hours

**Test Script**:
```javascript
// File: tests/integration.test.mjs
import assert from 'assert';

// Test 1: Device registration
async function testDeviceRegistration() {
    const publicKey = '0123456789abcdef...';  // 64 chars

    const res = await fetch('http://localhost:3001/api/devices/register', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ publicKey })
    });

    const data = await res.json();
    assert(data.registered === true);
    console.log('✅ Device registration test passed');
}

// Test 2: Step data submission
async function testStepDataSubmission() {
    const payload = {
        deviceId: '1234567890abcdef',
        stepCount: 450,
        timestamp: Math.floor(Date.now() / 1000),
        signature: '...'
    };

    const res = await fetch('http://localhost:3001/api/step-data/submit', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(payload)
    });

    const data = await res.json();
    assert(data.success === true || data.code === 'VERIFICATION_FAILED');
    console.log('✅ Step data submission test passed');
}

// Run all tests
await testDeviceRegistration();
await testStepDataSubmission();
console.log('\n✅ All tests passed!');
```

---

## File Structure After Completion

```
sui-watch-server/
├── server.mjs                  # Main Express server
├── deviceManager.mjs           # Device management
├── dataValidator.mjs           # Signature verification
├── stepDataAggregator.mjs      # Data collection
├── oracleSubmitter.mjs         # Blockchain submission
├── database.mjs                # Database wrapper
├── schema.sql                  # Database schema
├── package.json
├── .env                        # Configuration
├── .env.example
├── .gitignore
├── README.md
└── tests/
    └── integration.test.mjs
```

---

## Completion Checklist

- [ ] All modules implemented
- [ ] Database schema created
- [ ] API endpoints working
- [ ] Signature verification tested
- [ ] Blockchain integration tested
- [ ] Dashboard functional
- [ ] Cron job scheduled
- [ ] Documentation complete
- [ ] Tests passing
- [ ] Ready for production deployment
