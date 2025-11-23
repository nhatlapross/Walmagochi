/**
 * Device Manager
 * Manages ESP32 device connections, authentication, and state
 */

import sqlite3 from 'sqlite3';
import { fileURLToPath } from 'url';
import { dirname, join } from 'path';
import fs from 'fs';
import { promisify } from 'util';

const __filename = fileURLToPath(import.meta.url);
const __dirname = dirname(__filename);
const DB_PATH = join(__dirname, '../data/devices.db');

// Promisify sqlite3 methods
class Database {
    constructor(path) {
        this.db = new sqlite3.Database(path);
        this.run = promisify(this.db.run.bind(this.db));
        this.get = promisify(this.db.get.bind(this.db));
        this.all = promisify(this.db.all.bind(this.db));
    }

    async exec(sql) {
        return new Promise((resolve, reject) => {
            this.db.exec(sql, (err) => {
                if (err) reject(err);
                else resolve();
            });
        });
    }

    close() {
        this.db.close();
    }
}

export class DeviceManager {
    constructor() {
        // Ensure data directory exists
        const dataDir = join(__dirname, '../data');
        if (!fs.existsSync(dataDir)) {
            fs.mkdirSync(dataDir, { recursive: true });
        }

        // Initialize database
        this.db = new Database(DB_PATH);

        // In-memory device connections (WebSocket)
        this.connections = new Map(); // deviceId -> { ws, lastSeen, metadata }

        console.log('✓ DeviceManager initialized');
        console.log(`  Database: ${DB_PATH}`);
    }

    /**
     * Initialize database schema
     */
    async initDatabase() {
        // Devices table
        await this.db.exec(`
            CREATE TABLE IF NOT EXISTS devices (
                device_id TEXT PRIMARY KEY,
                public_key TEXT NOT NULL UNIQUE,
                registered_at INTEGER NOT NULL,
                last_seen INTEGER,
                firmware_version TEXT,
                total_steps INTEGER DEFAULT 0,
                total_submissions INTEGER DEFAULT 0,
                status TEXT DEFAULT 'active',
                sui_device_object_id TEXT
            )
        `);

        // Step data table
        await this.db.exec(`
            CREATE TABLE IF NOT EXISTS step_data (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                device_id TEXT NOT NULL,
                step_count INTEGER NOT NULL,
                timestamp INTEGER NOT NULL,
                raw_samples TEXT,
                battery_percent INTEGER,
                signature TEXT NOT NULL,
                verified BOOLEAN DEFAULT 0,
                received_at INTEGER NOT NULL,
                submitted_to_chain BOOLEAN DEFAULT 0,
                tx_digest TEXT,
                FOREIGN KEY (device_id) REFERENCES devices(device_id)
            )
        `);

        // Create indexes
        await this.db.exec(`
            CREATE INDEX IF NOT EXISTS idx_step_data_device ON step_data(device_id);
            CREATE INDEX IF NOT EXISTS idx_step_data_submitted ON step_data(submitted_to_chain);
        `);

        console.log('✓ Database schema initialized');
    }

    /**
     * Register new device
     */
    async registerDevice(deviceId, publicKeyHex) {
        const now = Date.now();

        try {
            const result = await this.db.run(
                `INSERT INTO devices (device_id, public_key, registered_at, last_seen, status)
                 VALUES (?, ?, ?, ?, 'active')`,
                [deviceId, publicKeyHex, now, now]
            );

            const device = await this.getDevice(deviceId);
            console.log(`✓ Device registered: ${deviceId}`);
            return device;

        } catch (err) {
            if (err.message.includes('UNIQUE constraint failed')) {
                // Device already exists, update last_seen
                await this.db.run(
                    `UPDATE devices SET last_seen = ? WHERE device_id = ?`,
                    [now, deviceId]
                );
                return await this.getDevice(deviceId);
            }
            throw err;
        }
    }

    /**
     * Get device by ID
     */
    async getDevice(deviceId) {
        return await this.db.get(
            `SELECT * FROM devices WHERE device_id = ?`,
            [deviceId]
        );
    }

    /**
     * Get all devices
     */
    async getAllDevices() {
        return await this.db.all(`SELECT * FROM devices ORDER BY registered_at DESC`);
    }

    /**
     * Update device's Sui object ID (after blockchain registration)
     */
    async updateDeviceObjectId(deviceId, objectId) {
        await this.db.run(
            `UPDATE devices SET sui_device_object_id = ? WHERE device_id = ?`,
            [objectId, deviceId]
        );
        console.log(`✓ Updated Sui object ID for ${deviceId}: ${objectId}`);
    }

    /**
     * Store step data submission
     */
    async storeStepData(deviceId, stepData) {
        const {
            stepCount,
            timestamp,
            rawAccSamples,
            batteryPercent,
            signature,
            verified
        } = stepData;

        const now = Date.now();
        const rawSamplesJson = JSON.stringify(rawAccSamples || []);

        const result = await this.db.run(
            `INSERT INTO step_data (
                device_id, step_count, timestamp, raw_samples,
                battery_percent, signature, verified, received_at
            ) VALUES (?, ?, ?, ?, ?, ?, ?, ?)`,
            [
                deviceId,
                stepCount,
                timestamp,
                rawSamplesJson,
                batteryPercent || 100,
                signature,
                verified ? 1 : 0,
                now
            ]
        );

        // Update device stats
        await this.db.run(
            `UPDATE devices
             SET total_steps = total_steps + ?,
                 last_seen = ?
             WHERE device_id = ?`,
            [stepCount, now, deviceId]
        );

        console.log(`✓ Step data stored: ${deviceId} (${stepCount} steps)`);
        return result.lastID;
    }

    /**
     * Get pending step data (not submitted to blockchain)
     */
    async getPendingStepData(deviceId = null) {
        let sql = `
            SELECT * FROM step_data
            WHERE submitted_to_chain = 0 AND verified = 1
        `;
        const params = [];

        if (deviceId) {
            sql += ` AND device_id = ?`;
            params.push(deviceId);
        }

        sql += ` ORDER BY received_at ASC`;

        return await this.db.all(sql, params);
    }

    /**
     * Mark step data as submitted to blockchain
     */
    async markAsSubmitted(dataIds, txDigest) {
        const placeholders = dataIds.map(() => '?').join(',');
        const params = [txDigest, ...dataIds];

        await this.db.run(
            `UPDATE step_data
             SET submitted_to_chain = 1, tx_digest = ?
             WHERE id IN (${placeholders})`,
            params
        );

        // Update device submission count
        const devices = await this.db.all(
            `SELECT DISTINCT device_id FROM step_data WHERE id IN (${placeholders})`,
            dataIds
        );

        for (const device of devices) {
            await this.db.run(
                `UPDATE devices
                 SET total_submissions = total_submissions + 1
                 WHERE device_id = ?`,
                [device.device_id]
            );
        }

        console.log(`✓ Marked ${dataIds.length} records as submitted (TX: ${txDigest})`);
    }

    /**
     * Register WebSocket connection
     */
    registerConnection(deviceId, ws, metadata = {}) {
        this.connections.set(deviceId, {
            ws,
            lastSeen: Date.now(),
            metadata
        });
        console.log(`✓ Device connected: ${deviceId}`);
    }

    /**
     * Unregister WebSocket connection
     */
    unregisterConnection(deviceId) {
        this.connections.delete(deviceId);
        console.log(`✗ Device disconnected: ${deviceId}`);
    }

    /**
     * Get connected device
     */
    getConnection(deviceId) {
        return this.connections.get(deviceId);
    }

    /**
     * Get all connected devices
     */
    getConnectedDevices() {
        return Array.from(this.connections.keys());
    }

    /**
     * Update last seen timestamp
     */
    async updateLastSeen(deviceId) {
        const now = Date.now();
        await this.db.run(
            `UPDATE devices SET last_seen = ? WHERE device_id = ?`,
            [now, deviceId]
        );

        // Update in-memory connection
        const conn = this.connections.get(deviceId);
        if (conn) {
            conn.lastSeen = now;
        }
    }

    /**
     * Get database statistics
     */
    async getStats() {
        const deviceCount = await this.db.get(
            `SELECT COUNT(*) as count FROM devices`
        );

        const totalSteps = await this.db.get(
            `SELECT SUM(total_steps) as total FROM devices`
        );

        const pendingCount = await this.db.get(
            `SELECT COUNT(*) as count FROM step_data
             WHERE submitted_to_chain = 0`
        );

        const submissionCount = await this.db.get(
            `SELECT SUM(total_submissions) as total FROM devices`
        );

        return {
            total_devices: deviceCount.count || 0,
            total_steps: totalSteps.total || 0,
            pending_submissions: pendingCount.count || 0,
            total_submissions: submissionCount.total || 0,
            connected_devices: this.connections.size
        };
    }

    /**
     * Close database connection
     */
    close() {
        this.db.close();
        console.log('✓ Database connection closed');
    }
}
