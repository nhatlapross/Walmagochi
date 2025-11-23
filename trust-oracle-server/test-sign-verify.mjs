#!/usr/bin/env node
/**
 * Simple test: Sign and Verify
 */

import nacl from 'tweetnacl';
import crypto from 'crypto';

// Generate keypair
const keypair = nacl.sign.keyPair();
const privateKey = keypair.secretKey;
const publicKey = keypair.publicKey;

console.log('🔑 Testing Ed25519 sign/verify\n');

// Test payload
const payload = {
    deviceId: "test_001",
    stepCount: 100,
    timestamp: 1234567890,
    firmwareVersion: 100,
    batteryPercent: 85,
    rawAccSamples: [[1.1, 2.2, 3.3], [4.4, 5.5, 6.6]]
};

// Build canonical JSON
function buildCanonicalJSON(obj) {
    const sortedKeys = Object.keys(obj).sort();
    const sorted = {};
    for (const key of sortedKeys) {
        sorted[key] = obj[key];
    }
    return JSON.stringify(sorted);
}

// Sign
const canonicalJson = buildCanonicalJSON(payload);
console.log('Canonical JSON:', canonicalJson);

const hash = crypto.createHash('sha256').update(canonicalJson, 'utf8').digest();
console.log('Hash:', '0x' + hash.toString('hex'));

const signature = nacl.sign.detached(hash, privateKey);
console.log('Signature:', '0x' + Buffer.from(signature).toString('hex').substring(0, 40) + '...');

// Verify
const isValid = nacl.sign.detached.verify(hash, signature, publicKey);
console.log('\n✅ Verification:', isValid ? 'SUCCESS' : 'FAILED');

// Test with same payload but rebuilt
const canonicalJson2 = buildCanonicalJSON(payload);
const hash2 = crypto.createHash('sha256').update(canonicalJson2, 'utf8').digest();
const isValid2 = nacl.sign.detached.verify(hash2, signature, publicKey);
console.log('✅ Re-verify:', isValid2 ? 'SUCCESS' : 'FAILED');
