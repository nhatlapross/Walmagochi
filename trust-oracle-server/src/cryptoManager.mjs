/**
 * Crypto Manager
 * Handles Ed25519 signature verification and data signing
 */

import nacl from 'tweetnacl';
import crypto from 'crypto';

export class CryptoManager {
    constructor() {
        console.log('✓ CryptoManager initialized');
    }

    /**
     * Verify Ed25519 signature
     * @param {object} payload - Data payload (without signature)
     * @param {string} signatureHex - Signature in hex format
     * @param {string} publicKeyHex - Public key in hex format
     * @returns {boolean} True if signature is valid
     */
    verifySignature(payload, signatureHex, publicKeyHex) {
        try {
            // Build canonical JSON (deterministic serialization)
            const canonicalJson = this.buildCanonicalJSON(payload);
            console.log('🔍 Canonical JSON:', canonicalJson.substring(0, 200) + '...');

            // Hash the canonical JSON
            const hash = crypto.createHash('sha256')
                .update(canonicalJson, 'utf8')
                .digest();
            console.log('🔍 Hash:', '0x' + hash.toString('hex'));

            // Convert hex strings to Uint8Array
            const signature = this.hexToBytes(signatureHex);
            const publicKey = this.hexToBytes(publicKeyHex);

            console.log('🔍 Signature length:', signature.length);
            console.log('🔍 Public key length:', publicKey.length);

            // Verify signature
            const isValid = nacl.sign.detached.verify(hash, signature, publicKey);

            if (isValid) {
                console.log('✓ Signature verified');
            } else {
                console.log('✗ Invalid signature');
            }

            return isValid;
        } catch (error) {
            console.error('✗ Signature verification error:', error.message);
            return false;
        }
    }

    /**
     * Build canonical JSON for signing
     * Ensures deterministic serialization
     * @param {object} obj - Object to serialize
     * @returns {string} Canonical JSON string
     */
    buildCanonicalJSON(obj) {
        // Sort keys alphabetically
        const sortedKeys = Object.keys(obj).sort();

        // Build object with sorted keys
        const sorted = {};
        for (const key of sortedKeys) {
            sorted[key] = obj[key];
        }

        // Use compact JSON (no extra spaces)
        return JSON.stringify(sorted);
    }

    /**
     * Convert hex string to Uint8Array
     * @param {string} hexString - Hex string (with or without 0x prefix)
     * @returns {Uint8Array} Byte array
     */
    hexToBytes(hexString) {
        // Remove 0x prefix if present
        const hex = hexString.startsWith('0x') ? hexString.slice(2) : hexString;

        // Validate hex string
        if (hex.length % 2 !== 0) {
            throw new Error('Invalid hex string length');
        }

        // Convert to Uint8Array
        const bytes = new Uint8Array(hex.length / 2);
        for (let i = 0; i < hex.length; i += 2) {
            bytes[i / 2] = parseInt(hex.substr(i, 2), 16);
        }

        return bytes;
    }

    /**
     * Convert Uint8Array to hex string
     * @param {Uint8Array} bytes - Byte array
     * @param {boolean} withPrefix - Add 0x prefix
     * @returns {string} Hex string
     */
    bytesToHex(bytes, withPrefix = true) {
        const hex = Array.from(bytes)
            .map(b => b.toString(16).padStart(2, '0'))
            .join('');

        return withPrefix ? '0x' + hex : hex;
    }

    /**
     * Generate keypair (for testing)
     * @returns {object} {publicKey, privateKey} in hex format
     */
    generateKeypair() {
        const keypair = nacl.sign.keyPair();

        return {
            publicKey: this.bytesToHex(keypair.publicKey),
            privateKey: this.bytesToHex(keypair.secretKey),
            publicKeyBytes: keypair.publicKey,
            privateKeyBytes: keypair.secretKey
        };
    }

    /**
     * Sign data with private key (for testing)
     * @param {object} payload - Data to sign
     * @param {string} privateKeyHex - Private key in hex
     * @returns {string} Signature in hex
     */
    signData(payload, privateKeyHex) {
        try {
            const canonicalJson = this.buildCanonicalJSON(payload);
            const hash = crypto.createHash('sha256')
                .update(canonicalJson, 'utf8')
                .digest();

            const privateKey = this.hexToBytes(privateKeyHex);
            const signature = nacl.sign.detached(hash, privateKey);

            return this.bytesToHex(signature);
        } catch (error) {
            console.error('✗ Signing error:', error.message);
            throw error;
        }
    }

    /**
     * Validate step data payload format
     * @param {object} payload - Step data payload
     * @returns {object} {valid: boolean, errors: string[]}
     */
    validatePayload(payload) {
        const errors = [];

        // Required fields
        if (!payload.deviceId || typeof payload.deviceId !== 'string') {
            errors.push('Missing or invalid deviceId');
        }

        if (!payload.stepCount || typeof payload.stepCount !== 'number') {
            errors.push('Missing or invalid stepCount');
        }

        if (!payload.timestamp || typeof payload.timestamp !== 'number') {
            errors.push('Missing or invalid timestamp');
        }

        if (!payload.signature || typeof payload.signature !== 'string') {
            errors.push('Missing or invalid signature');
        }

        // Validate step count range
        if (payload.stepCount && (payload.stepCount < 0 || payload.stepCount > 100000)) {
            errors.push('Step count out of range (0-100000)');
        }

        // Validate timestamp (not too old, not in future)
        if (payload.timestamp) {
            const now = Date.now();
            const maxAge = 7 * 24 * 60 * 60 * 1000; // 7 days
            const maxFuture = 5 * 60 * 1000; // 5 minutes

            if (payload.timestamp > now + maxFuture) {
                errors.push('Timestamp is too far in the future');
            }

            if (payload.timestamp < now - maxAge) {
                errors.push('Timestamp is too old (max 7 days)');
            }
        }

        // Validate battery percent
        if (payload.batteryPercent !== undefined) {
            if (payload.batteryPercent < 0 || payload.batteryPercent > 100) {
                errors.push('Battery percent out of range (0-100)');
            }
        }

        return {
            valid: errors.length === 0,
            errors
        };
    }

    /**
     * Extract public data from payload (without signature)
     * @param {object} payload - Full payload including signature
     * @returns {object} Payload without signature
     */
    extractPublicData(payload) {
        const { signature, ...publicData } = payload;
        return publicData;
    }
}

// Export singleton instance
export const cryptoManager = new CryptoManager();
