#!/usr/bin/env node
/**
 * Test WebSocket Client
 * Simulates ESP32 device with Ed25519 signing
 */

import WebSocket from 'ws';
import nacl from 'tweetnacl';
import crypto from 'crypto';

const WS_URL = 'ws://localhost:8080';
const DEVICE_ID = 'test_device_001';

// Generate Ed25519 keypair (simulating ESP32)
console.log('🔑 Generating Ed25519 keypair...\n');
const keypair = nacl.sign.keyPair();
const privateKey = keypair.secretKey;
const publicKey = keypair.publicKey;
const publicKeyHex = '0x' + Buffer.from(publicKey).toString('hex');

console.log('Public Key:', publicKeyHex);
console.log('Private Key: [HIDDEN]\n');

// Connect to WebSocket
console.log(`📡 Connecting to ${WS_URL}...`);
const ws = new WebSocket(WS_URL);

let authenticated = false;

// Helper: Build canonical JSON (sorted keys)
function buildCanonicalJSON(obj) {
    const sortedKeys = Object.keys(obj).sort();
    const sorted = {};
    for (const key of sortedKeys) {
        sorted[key] = obj[key];
    }
    return JSON.stringify(sorted);
}

// Helper: Sign payload with Ed25519
function signPayload(payload) {
    // 1. Build canonical JSON
    const canonicalJson = buildCanonicalJSON(payload);
    console.log('🔍 [Client] Canonical JSON:', canonicalJson.substring(0, 200) + '...');

    // 2. SHA256 hash
    const hash = crypto.createHash('sha256')
        .update(canonicalJson, 'utf8')
        .digest();
    console.log('🔍 [Client] Hash:', '0x' + hash.toString('hex'));

    // 3. Ed25519 sign
    const signature = nacl.sign.detached(hash, privateKey);

    // 4. Return hex
    return '0x' + Buffer.from(signature).toString('hex');
}

// WebSocket events
ws.on('open', () => {
    console.log('✅ Connected!\n');

    // Step 1: Register device
    console.log('📝 Registering device...');
    ws.send(JSON.stringify({
        type: 'register',
        deviceId: DEVICE_ID,
        publicKey: publicKeyHex
    }));
});

ws.on('message', (data) => {
    const message = JSON.parse(data.toString());
    console.log('📥 Received:', message.type);

    switch (message.type) {
        case 'welcome':
            console.log('   Message:', message.message);
            break;

        case 'register_response':
            if (message.success) {
                console.log('   ✅ Registration successful!');
                if (message.blockchainResult) {
                    console.log('   📦 Blockchain TX:', message.blockchainResult.txDigest);
                }

                // Step 2: Authenticate
                console.log('\n🔐 Authenticating...');
                ws.send(JSON.stringify({
                    type: 'authenticate',
                    deviceId: DEVICE_ID
                }));
            } else {
                console.log('   ❌ Registration failed:', message.error);
            }
            break;

        case 'auth_response':
            if (message.success) {
                console.log('   ✅ Authentication successful!');
                authenticated = true;

                // Step 3: Send step data
                setTimeout(() => sendStepData(), 1000);
            } else {
                console.log('   ❌ Authentication failed:', message.error);
            }
            break;

        case 'step_data_response':
            if (message.success) {
                console.log('   ✅ Step data accepted!');
                console.log('   📊 Data ID:', message.dataId);
                console.log('   👣 Steps:', message.stepCount);
                console.log('   ✓ Verified:', message.verified);

                // Send another batch after 2 seconds
                setTimeout(() => sendStepData(), 2000);
            } else {
                console.log('   ❌ Step data rejected:', message.error);
            }
            break;

        case 'pong':
            console.log('   🏓 Pong received');
            break;

        case 'error':
            console.log('   ❌ Error:', message.error);
            break;
    }
});

ws.on('error', (error) => {
    console.error('❌ WebSocket error:', error.message);
});

ws.on('close', () => {
    console.log('\n❌ Disconnected from server');
    process.exit(0);
});

// Send step data with signature
function sendStepData() {
    if (!authenticated) {
        console.log('⚠️  Not authenticated, skipping step data');
        return;
    }

    const stepCount = Math.floor(Math.random() * 500) + 100;
    const timestamp = Date.now();

    // Generate simulated accelerometer samples
    const rawAccSamples = [];
    for (let i = 0; i < 30; i++) {
        rawAccSamples.push([
            Math.random() * 200 - 100,
            Math.random() * 200 - 100,
            Math.random() * 2000 - 1000
        ]);
    }

    // Build payload (WITHOUT signature)
    const payload = {
        deviceId: DEVICE_ID,
        stepCount: stepCount,
        timestamp: timestamp,
        firmwareVersion: 100,
        batteryPercent: 85,
        rawAccSamples: rawAccSamples
    };

    // Sign the payload
    const signature = signPayload(payload);

    console.log(`\n📤 Sending step data (${stepCount} steps)...`);
    console.log('   Signature:', signature.substring(0, 20) + '...');

    // Send with signature (MUST include deviceId that was signed)
    ws.send(JSON.stringify({
        type: 'step_data',
        deviceId: DEVICE_ID,  // IMPORTANT: Must match signed payload
        stepCount: stepCount,
        timestamp: timestamp,
        firmwareVersion: 100,
        batteryPercent: 85,
        rawAccSamples: rawAccSamples,
        signature: signature
    }));
}

// Handle Ctrl+C
process.on('SIGINT', () => {
    console.log('\n\n👋 Closing connection...');
    ws.close();
});

console.log('\n📚 Test Flow:');
console.log('  1. Register device with public key');
console.log('  2. Authenticate device');
console.log('  3. Send signed step data');
console.log('  4. Server verifies signature');
console.log('  5. Repeat step data submissions\n');
console.log('Press Ctrl+C to exit\n');
