#!/usr/bin/env node
/**
 * Trust Oracle Backend Server
 * - WebSocket server for ESP32 devices
 * - Ed25519 signature verification
 * - Sui blockchain integration
 * - Automated batch submissions
 */

import express from 'express';
import cors from 'cors';
import { WebSocketServer } from 'ws';
import { createServer } from 'http';
import dotenv from 'dotenv';
import cron from 'node-cron';
import { DeviceManager } from './deviceManager.mjs';
import { cryptoManager } from './cryptoManager.mjs';
import { SuiClient } from './suiClient.mjs';
import { PetManager } from './petManager.mjs';

// Load environment variables
dotenv.config();

const app = express();
const PORT = process.env.PORT || 3001;
const WS_PORT = process.env.WS_PORT || 8080;

// Sui configuration
const SUI_NETWORK = process.env.SUI_NETWORK || 'testnet';
const SUI_PACKAGE_ID = process.env.SUI_PACKAGE_ID;
const SUI_REGISTRY_ID = process.env.SUI_REGISTRY_ID;
const SUI_PRIVATE_KEY = process.env.SUI_PRIVATE_KEY;

// Global instances
let deviceManager;
let petManager;
let suiClient;

// Initialize services
async function initializeServices() {
    console.log('\n🚀 Initializing Trust Oracle Backend Server...\n');

    // Initialize Device Manager
    deviceManager = new DeviceManager();
    await deviceManager.initDatabase();

    // Initialize Pet Manager
    petManager = new PetManager();
    await petManager.initDatabase();

    // Initialize Sui client
    if (SUI_PACKAGE_ID && SUI_REGISTRY_ID && SUI_PRIVATE_KEY) {
        try {
            suiClient = new SuiClient(SUI_NETWORK, SUI_PACKAGE_ID, SUI_REGISTRY_ID, SUI_PRIVATE_KEY);
            console.log('✓ Sui blockchain integration enabled');
        } catch (error) {
            console.warn('⚠️  Failed to initialize Sui client:', error.message);
            console.warn('   Backend will work in LOCAL MODE (no blockchain submissions)');
        }
    } else {
        console.warn('⚠️  Sui blockchain integration disabled (missing configuration)');
        if (!SUI_PACKAGE_ID) console.warn('   Missing: SUI_PACKAGE_ID');
        if (!SUI_REGISTRY_ID) console.warn('   Missing: SUI_REGISTRY_ID');
        if (!SUI_PRIVATE_KEY) console.warn('   Missing: SUI_PRIVATE_KEY');
        console.warn('   Backend will work in LOCAL MODE (no blockchain submissions)');
    }
}

// Middleware
app.use(cors());
app.use(express.json());

// Create HTTP server
const server = createServer(app);

// Create WebSocket server
const wss = new WebSocketServer({ server: createServer().listen(WS_PORT) });

console.log(`\n🌐 WebSocket server listening on port ${WS_PORT}`);

// =======================
// WebSocket Handler
// =======================

wss.on('connection', (ws, req) => {
    const clientIp = req.socket.remoteAddress;
    console.log(`\n📡 New WebSocket connection from ${clientIp}`);

    let deviceId = null;
    let authenticated = false;

    ws.on('message', async (data) => {
        let message = null;
        try {
            message = JSON.parse(data.toString());
            console.log(`📨 Received message type: ${message.type} from deviceId: ${deviceId || 'not set yet'}`);

            // Handle different message types
            switch (message.type) {
                case 'register':
                    await handleRegister(ws, message);
                    break;

                case 'authenticate':
                    const result = await handleAuthenticate(ws, message);
                    if (result.success) {
                        deviceId = message.deviceId;
                        authenticated = true;
                    }
                    break;

                case 'step_data':
                    if (!authenticated) {
                        ws.send(JSON.stringify({
                            type: 'error',
                            error: 'Not authenticated'
                        }));
                        return;
                    }
                    await handleStepData(ws, message, deviceId);
                    break;

                case 'ping':
                    ws.send(JSON.stringify({
                        type: 'pong',
                        timestamp: Date.now()
                    }));
                    break;

                // Virtual Pet messages - require authentication
                case 'getPet':
                    if (!authenticated) {
                        ws.send(JSON.stringify({
                            type: 'error',
                            error: 'Not authenticated'
                        }));
                        return;
                    }
                    await handleGetPet(ws, message, deviceId);
                    break;

                case 'updatePet':
                    if (!authenticated) {
                        ws.send(JSON.stringify({
                            type: 'error',
                            error: 'Not authenticated'
                        }));
                        return;
                    }
                    await handleUpdatePet(ws, message, deviceId);
                    break;

                case 'claimResources':
                    if (!authenticated) {
                        ws.send(JSON.stringify({
                            type: 'error',
                            error: 'Not authenticated'
                        }));
                        return;
                    }
                    await handleClaimResources(ws, message, deviceId);
                    break;

                case 'feedPet':
                    if (!authenticated) {
                        ws.send(JSON.stringify({
                            type: 'error',
                            error: 'Not authenticated'
                        }));
                        return;
                    }
                    await handleFeedPet(ws, message, deviceId);
                    break;

                case 'playWithPet':
                    if (!authenticated) {
                        ws.send(JSON.stringify({
                            type: 'error',
                            error: 'Not authenticated'
                        }));
                        return;
                    }
                    await handlePlayWithPet(ws, message, deviceId);
                    break;

                default:
                    ws.send(JSON.stringify({
                        type: 'error',
                        error: 'Unknown message type'
                    }));
            }

        } catch (error) {
            console.error(`❌ WebSocket message error (type: ${message.type || 'unknown'}):`, error.message);
            console.error(`   Stack trace:`, error.stack);
            ws.send(JSON.stringify({
                type: 'error',
                error: error.message
            }));
        }
    });

    ws.on('close', () => {
        if (deviceId) {
            deviceManager.unregisterConnection(deviceId);
        }
        console.log(`📡 WebSocket closed: ${deviceId || 'unknown'}`);
    });

    ws.on('error', (error) => {
        console.error('❌ WebSocket error:', error.message);
    });

    // Send welcome message
    ws.send(JSON.stringify({
        type: 'welcome',
        message: 'Connected to Trust Oracle Server',
        timestamp: Date.now()
    }));
});

/**
 * Handle device registration
 */
async function handleRegister(ws, message) {
    try {
        const { deviceId, publicKey } = message;

        if (!deviceId || !publicKey) {
            throw new Error('Missing deviceId or publicKey');
        }

        // Register device in database
        const device = await deviceManager.registerDevice(deviceId, publicKey);

        // Register on blockchain (if available)
        let blockchainResult = null;
        if (suiClient) {
            try {
                blockchainResult = await suiClient.registerDevice(deviceId, publicKey);
            } catch (error) {
                console.warn('⚠️  Blockchain registration failed:', error.message);
            }
        }

        ws.send(JSON.stringify({
            type: 'register_response',
            success: true,
            device,
            blockchainResult
        }));

        console.log(`✅ Device registered: ${deviceId}`);

    } catch (error) {
        ws.send(JSON.stringify({
            type: 'register_response',
            success: false,
            error: error.message
        }));
    }
}

/**
 * Handle device authentication
 */
async function handleAuthenticate(ws, message) {
    try {
        const { deviceId, challenge, signature } = message;

        if (!deviceId) {
            throw new Error('Missing deviceId');
        }

        // Get device from database
        const device = await deviceManager.getDevice(deviceId);
        if (!device) {
            throw new Error('Device not registered');
        }

        // Simple authentication for now (just check if device exists)
        // In production, implement challenge-response authentication

        // Register WebSocket connection
        deviceManager.registerConnection(deviceId, ws, {
            authenticatedAt: Date.now()
        });

        ws.send(JSON.stringify({
            type: 'auth_response',
            success: true,
            deviceId
        }));

        console.log(`✅ Device authenticated: ${deviceId}`);

        return { success: true, deviceId };

    } catch (error) {
        ws.send(JSON.stringify({
            type: 'auth_response',
            success: false,
            error: error.message
        }));

        return { success: false, error: error.message };
    }
}

/**
 * Handle step data submission
 */
async function handleStepData(ws, message, authenticatedDeviceId) {
    try {
        const { deviceId, stepCount, timestamp, batteryPercent, rawAccSamples, firmwareVersion, signature } = message;

        // Verify deviceId matches authenticated session
        if (deviceId !== authenticatedDeviceId) {
            throw new Error('Device ID mismatch');
        }

        // Get device
        const device = await deviceManager.getDevice(deviceId);
        if (!device) {
            throw new Error('Device not found');
        }

        // Build payload for verification (without signature)
        // MUST match exactly what client signed
        const payload = {
            deviceId,
            stepCount,
            timestamp,
            firmwareVersion: firmwareVersion || 100,
            batteryPercent: batteryPercent || 100,
            rawAccSamples: rawAccSamples || []
        };

        // Validate payload format
        const validation = cryptoManager.validatePayload({ ...payload, signature });
        if (!validation.valid) {
            throw new Error(`Validation failed: ${validation.errors.join(', ')}`);
        }

        // Verify signature
        const isValid = cryptoManager.verifySignature(
            payload,
            signature,
            device.public_key
        );

        if (!isValid) {
            throw new Error('Invalid signature');
        }

        // Store step data
        const dataId = deviceManager.storeStepData(deviceId, {
            stepCount,
            timestamp,
            rawAccSamples,
            batteryPercent,
            signature,
            verified: true
        });

        // Update firmware version
        if (firmwareVersion) {
            deviceManager.updateFirmwareVersion(deviceId, `v${Math.floor(firmwareVersion / 100)}.${firmwareVersion % 100}`);
        }

        // Send success response
        ws.send(JSON.stringify({
            type: 'step_data_response',
            success: true,
            dataId,
            stepCount,
            verified: true
        }));

        console.log(`✅ Step data received: ${deviceId} - ${stepCount} steps`);

    } catch (error) {
        ws.send(JSON.stringify({
            type: 'step_data_response',
            success: false,
            error: error.message
        }));

        console.error(`❌ Step data error (${authenticatedDeviceId || 'unknown'}):`, error.message);
    }
}

// =======================
// REST API Endpoints
// =======================

// Health check
app.get('/', (req, res) => {
    const stats = deviceManager.getStats();

    res.json({
        status: 'ok',
        service: 'Trust Oracle Backend Server',
        version: '1.0.0',
        network: SUI_NETWORK,
        stats
    });
});

// Get all devices
app.get('/api/devices', (req, res) => {
    try {
        const devices = deviceManager.getAllDevices();
        const connected = deviceManager.getConnectedDevices();

        const devicesWithStatus = devices.map(device => ({
            ...device,
            connected: connected.includes(device.device_id)
        }));

        res.json({
            success: true,
            count: devices.length,
            devices: devicesWithStatus
        });
    } catch (error) {
        res.status(500).json({
            success: false,
            error: error.message
        });
    }
});

// Get device by ID
app.get('/api/devices/:deviceId', (req, res) => {
    try {
        const { deviceId } = req.params;
        const device = deviceManager.getDevice(deviceId);

        if (!device) {
            return res.status(404).json({
                success: false,
                error: 'Device not found'
            });
        }

        res.json({
            success: true,
            device,
            connected: deviceManager.isConnected(deviceId)
        });
    } catch (error) {
        res.status(500).json({
            success: false,
            error: error.message
        });
    }
});

// Get pending step data
app.get('/api/step-data/pending', (req, res) => {
    try {
        const { deviceId } = req.query;
        const pending = deviceManager.getPendingStepData(deviceId);

        res.json({
            success: true,
            count: pending.length,
            data: pending
        });
    } catch (error) {
        res.status(500).json({
            success: false,
            error: error.message
        });
    }
});

// Manual batch submission to blockchain
app.post('/api/oracle/submit-batch', async (req, res) => {
    if (!suiClient) {
        return res.status(503).json({
            success: false,
            error: 'Blockchain integration disabled'
        });
    }

    try {
        const results = await submitBatchToBlockchain();

        res.json({
            success: true,
            submitted: results.length,
            results
        });
    } catch (error) {
        res.status(500).json({
            success: false,
            error: error.message
        });
    }
});

// Get registry stats from blockchain
app.get('/api/oracle/stats', async (req, res) => {
    if (!suiClient) {
        return res.status(503).json({
            success: false,
            error: 'Blockchain integration disabled'
        });
    }

    try {
        const stats = await suiClient.getRegistryStats();

        res.json({
            success: true,
            stats
        });
    } catch (error) {
        res.status(500).json({
            success: false,
            error: error.message
        });
    }
});

// Get server balance
app.get('/api/oracle/balance', async (req, res) => {
    if (!suiClient) {
        return res.status(503).json({
            success: false,
            error: 'Blockchain integration disabled'
        });
    }

    try {
        const balance = await suiClient.getBalance();

        res.json({
            success: true,
            balance,
            address: suiClient.address
        });
    } catch (error) {
        res.status(500).json({
            success: false,
            error: error.message
        });
    }
});

// =======================
// Batch Submission Logic
// =======================

/**
 * Submit pending step data to blockchain
 */
async function submitBatchToBlockchain() {
    if (!suiClient) {
        console.warn('⚠️  Blockchain submission skipped (not configured)');
        return [];
    }

    console.log('\n📦 Starting batch submission to blockchain...');

    try {
        // Get all pending data grouped by device
        const pending = deviceManager.getPendingStepData();

        if (pending.length === 0) {
            console.log('  No pending data to submit');
            return [];
        }

        console.log(`  Found ${pending.length} pending submissions`);

        // Group by device
        const byDevice = {};
        for (const data of pending) {
            if (!byDevice[data.device_id]) {
                byDevice[data.device_id] = [];
            }
            byDevice[data.device_id].push(data);
        }

        const results = [];

        // Submit for each device
        for (const [deviceId, dataList] of Object.entries(byDevice)) {
            try {
                // Get device to find object ID (assume stored during registration)
                const device = deviceManager.getDevice(deviceId);
                if (!device.sui_device_object_id) {
                    console.warn(`  ⚠️  Device ${deviceId} has no blockchain object ID, skipping`);
                    continue;
                }

                // Aggregate data
                const totalSteps = dataList.reduce((sum, d) => sum + d.step_count, 0);
                const timestamps = dataList.map(d => d.timestamp);
                const signatures = dataList.map(d => d.signature);

                // Submit to blockchain
                const result = await suiClient.submitStepData(
                    device.sui_device_object_id,
                    totalSteps,
                    timestamps,
                    signatures
                );

                if (result.success) {
                    // Mark as submitted
                    const dataIds = dataList.map(d => d.id);
                    deviceManager.markAsSubmitted(dataIds, result.txDigest);

                    results.push({
                        deviceId,
                        success: true,
                        totalSteps,
                        recordCount: dataList.length,
                        txDigest: result.txDigest
                    });

                    console.log(`  ✅ Submitted ${deviceId}: ${totalSteps} steps (TX: ${result.txDigest.substring(0, 12)}...)`);
                }

            } catch (error) {
                console.error(`  ❌ Failed to submit ${deviceId}:`, error.message);
                results.push({
                    deviceId,
                    success: false,
                    error: error.message
                });
            }
        }

        console.log(`\n✅ Batch submission complete: ${results.filter(r => r.success).length}/${results.length} successful`);

        return results;

    } catch (error) {
        console.error('❌ Batch submission failed:', error.message);
        throw error;
    }
}

// =======================
// Scheduled Tasks
// =======================

if (suiClient) {
    // Daily batch submission at 2 AM
    cron.schedule('0 2 * * *', async () => {
        console.log('\n⏰ Scheduled batch submission triggered');
        try {
            await submitBatchToBlockchain();
        } catch (error) {
            console.error('❌ Scheduled submission failed:', error.message);
        }
    });

    console.log('⏰ Scheduled batch submission: Daily at 2:00 AM');
}

// =======================
// Start Server
// =======================

server.listen(PORT, '0.0.0.0', async () => {
    console.log('\n═══════════════════════════════════════════════════════════');
    console.log('🚀 Trust Oracle Backend Server v1.0');
    console.log('═══════════════════════════════════════════════════════════');
    console.log(`  HTTP Server: http://localhost:${PORT}`);
    console.log(`  WebSocket Server: ws://localhost:${WS_PORT}`);
    console.log(`  Network: ${SUI_NETWORK}`);
    console.log('');

    if (suiClient) {
        console.log('⛓️  Blockchain Integration: ENABLED');
        console.log(`  Package: ${SUI_PACKAGE_ID?.substring(0, 12)}...`);
        console.log(`  Registry: ${SUI_REGISTRY_ID?.substring(0, 12)}...`);
        console.log(`  Address: ${suiClient.address?.substring(0, 12)}...`);

        try {
            const balance = await suiClient.getBalance();
            console.log(`  Balance: ${balance} SUI`);
        } catch (error) {
            console.warn('  ⚠️  Failed to fetch balance');
        }
    } else {
        console.log('⛓️  Blockchain Integration: DISABLED');
    }

    console.log('');
    console.log('📡 REST API Endpoints:');
    console.log('  GET    /');
    console.log('  GET    /api/devices');
    console.log('  GET    /api/devices/:deviceId');
    console.log('  GET    /api/step-data/pending');
    console.log('  POST   /api/oracle/submit-batch');
    console.log('  GET    /api/oracle/stats');
    console.log('  GET    /api/oracle/balance');
    console.log('');
    console.log('🌐 WebSocket Protocol:');
    console.log('  register        - Register new device');
    console.log('  authenticate    - Authenticate device');
    console.log('  step_data       - Submit step data');
    console.log('  ping/pong       - Keep-alive');
    console.log('');

    const stats = deviceManager.getStats();
    console.log(`📊 Current Stats:`);
    console.log(`  Devices: ${stats.total_devices}`);
    console.log(`  Connected: ${stats.connected_devices}`);
    console.log(`  Total Steps: ${stats.total_steps}`);
    console.log(`  Pending: ${stats.pending_submissions}`);

    console.log('═══════════════════════════════════════════════════════════');
    console.log('');
});

/**
 * Pet WebSocket Handlers
 */

async function handleGetPet(ws, message, deviceId) {
    try {
        console.log(`🐾 handleGetPet called for device: ${deviceId}`);
        if (!deviceId) throw new Error('Not authenticated');

        let pet = await petManager.getPetByDeviceId(deviceId);
        console.log(`  Pet found in DB: ${pet ? 'YES' : 'NO'}`);

        if (!pet) {
            console.log(`  Creating new pet in database...`);
            // Create pet in database first
            const newPet = await petManager.getOrCreatePet(deviceId, message.petName || 'Tamagotchi');
            console.log(`  ✓ Pet created in DB with ID: ${newPet.pet_id}`);

            // Create pet on blockchain if Sui client is initialized
            if (suiClient && !newPet.on_chain) {
                console.log(`  Attempting to create pet on blockchain...`);
                try {
                    const result = await Promise.race([
                        suiClient.createPet(
                            newPet.pet_name,
                            deviceId,
                            newPet.color || 'blue'
                        ),
                        new Promise((_, reject) =>
                            setTimeout(() => reject(new Error('Blockchain timeout after 30s')), 30000)
                        )
                    ]);

                    if (result.success && result.petObjectId) {
                        await petManager.markPetOnChain(
                            newPet.pet_id,
                            result.petObjectId,
                            result.txDigest
                        );
                        newPet.pet_object_id = result.petObjectId;
                        newPet.on_chain = true;
                        console.log(`✓ Pet created on-chain with ID: ${result.petObjectId}`);
                    }
                } catch (error) {
                    console.warn(`  ⚠️  Failed to create pet on blockchain: ${error.message}`);
                    console.warn(`  Pet will still work in offline mode`);
                }
            }

            console.log(`  Sending pet_data response to ESP32...`);
            ws.send(JSON.stringify({
                type: 'pet_data',
                success: true,
                pet: newPet
            }));
            console.log(`  ✓ pet_data sent`);
        } else {
            // Update time-based stats locally
            const updatedPet = await petManager.updateTimeBasedStats(pet.pet_id);

            // Sync with blockchain if pet is on-chain
            if (suiClient && updatedPet.pet_object_id) {
                try {
                    // Get latest on-chain data
                    const onChainPet = await suiClient.getPet(updatedPet.pet_object_id);
                    if (onChainPet) {
                        // Merge on-chain data with local data
                        updatedPet.happiness = onChainPet.happiness;
                        updatedPet.hunger = onChainPet.hunger;
                        updatedPet.health = onChainPet.health;
                        updatedPet.level = onChainPet.level;
                        updatedPet.total_steps_fed = onChainPet.total_steps_fed;
                    }
                } catch (error) {
                    console.warn('Failed to sync with blockchain:', error.message);
                }
            }

            ws.send(JSON.stringify({
                type: 'pet_data',
                success: true,
                pet: updatedPet
            }));
        }
    } catch (error) {
        ws.send(JSON.stringify({
            type: 'pet_error',
            success: false,
            error: error.message
        }));
    }
}

async function handleUpdatePet(ws, message, deviceId) {
    try {
        if (!deviceId) throw new Error('Not authenticated');

        const pet = await petManager.getPetByDeviceId(deviceId);
        if (!pet) throw new Error('Pet not found');

        const { happiness, hunger, health, experience, total_steps_fed, level } = message;
        await petManager.updatePetStats(pet.pet_id, {
            happiness, hunger, health, experience, total_steps_fed, level
        });

        ws.send(JSON.stringify({
            type: 'pet_updated',
            success: true,
            pet_id: pet.pet_id
        }));

        console.log(`🐾 Pet updated: ${pet.pet_name} (Device: ${deviceId})`);
    } catch (error) {
        ws.send(JSON.stringify({
            type: 'pet_error',
            success: false,
            error: error.message
        }));
    }
}

async function handleClaimResources(ws, message, deviceId) {
    try {
        if (!deviceId) throw new Error('Not authenticated');

        const pet = await petManager.getPetByDeviceId(deviceId);
        if (!pet) throw new Error('Pet not found');

        const { steps } = message;
        if (!steps || steps < 100) throw new Error('Insufficient steps (minimum 100)');

        // Calculate resources: 100 steps = 1 food, 150 steps = 2 energy
        const foodGained = Math.floor(steps / 100);
        const energyGained = Math.floor(steps / 150) * 2;

        console.log(`💰 Claiming resources from ${steps} steps: +${foodGained} food, +${energyGained} energy`);

        // Claim resources on blockchain if pet exists there
        if (suiClient && pet.pet_object_id) {
            try {
                const result = await suiClient.claimResources(pet.pet_object_id, steps);

                if (result.success) {
                    console.log(`✓ Resources claimed on blockchain`);
                    console.log(`  Food: ${result.foodGained}, Energy: ${result.energyGained}`);

                    // Update local database with on-chain values
                    await petManager.updatePetResources(pet.pet_id, result.newFood, result.newEnergy);
                }
            } catch (error) {
                console.warn('Failed to claim resources on blockchain:', error.message);
                // Update locally even if blockchain fails
                await petManager.addPetResources(pet.pet_id, foodGained, energyGained);
            }
        } else {
            // No blockchain, update locally only
            await petManager.addPetResources(pet.pet_id, foodGained, energyGained);
        }

        // Get updated pet
        const updatedPet = await petManager.getPetByDeviceId(deviceId);

        ws.send(JSON.stringify({
            type: 'resources_claimed',
            success: true,
            foodGained,
            energyGained,
            pet: updatedPet
        }));

        console.log(`💰 Resources claimed for ${pet.pet_name}`);
    } catch (error) {
        ws.send(JSON.stringify({
            type: 'pet_error',
            success: false,
            error: error.message
        }));
    }
}

async function handleFeedPet(ws, message, deviceId) {
    try {
        console.log(`🍔 handleFeedPet called for device: ${deviceId}`);
        if (!deviceId) throw new Error('Not authenticated');

        const pet = await petManager.getPetByDeviceId(deviceId);
        console.log(`  Pet found: ${pet ? 'YES' : 'NO'}`);
        if (!pet) throw new Error('Pet not found');

        console.log(`  Pet has ${pet.food} food, ${pet.energy} energy`);
        console.log(`  Pet object ID: ${pet.pet_object_id || 'NOT SET'}`);

        // Check if pet has food
        if (pet.food <= 0) throw new Error('No food available');

        console.log(`🍔 Feeding pet (uses 1 food, +10 XP)`);

        // Feed pet on blockchain if it exists there
        if (suiClient && pet.pet_object_id) {
            try {
                const result = await suiClient.feedPet(pet.pet_object_id);

                if (result.success) {
                    console.log(`✓ Pet fed on blockchain`);

                    if (result.evolved) {
                        console.log(`🎉 Pet evolved on-chain to level ${result.newLevel}!`);
                    }

                    // Fetch updated on-chain state
                    const onchainPet = await suiClient.getPet(pet.pet_object_id);

                    // Update local database to match on-chain
                    await petManager.updatePetStats(pet.pet_id, {
                        level: onchainPet.level,
                        happiness: onchainPet.happiness,
                        hunger: onchainPet.hunger,
                        health: onchainPet.health,
                        experience: onchainPet.experience,
                        food: onchainPet.food,
                        energy: onchainPet.energy
                    });
                }
            } catch (error) {
                console.warn('Failed to feed pet on blockchain:', error.message);
                // Update locally even if blockchain fails
                await petManager.feedPetLocal(pet.pet_id);
            }
        } else {
            // No blockchain, update locally only
            await petManager.feedPetLocal(pet.pet_id);
        }

        // Get updated pet
        const updatedPet = await petManager.getPetByDeviceId(deviceId);

        ws.send(JSON.stringify({
            type: 'pet_fed',
            success: true,
            pet: updatedPet,
            evolved: updatedPet.level > pet.level
        }));

        console.log(`🍔 Pet fed: ${pet.pet_name}`);
    } catch (error) {
        ws.send(JSON.stringify({
            type: 'pet_error',
            success: false,
            error: error.message
        }));
    }
}

async function handlePlayWithPet(ws, message, deviceId) {
    try {
        if (!deviceId) throw new Error('Not authenticated');

        const pet = await petManager.getPetByDeviceId(deviceId);
        if (!pet) throw new Error('Pet not found');

        // Check if pet has energy
        if (pet.energy <= 0) throw new Error('No energy available');

        console.log(`🎮 Playing with pet (uses 1 energy, +5 XP, +3 HP)`);

        // Play with pet on blockchain if it exists there
        if (suiClient && pet.pet_object_id) {
            try {
                const result = await suiClient.playWithPet(pet.pet_object_id);

                if (result.success) {
                    console.log(`✓ Played with pet on blockchain`);

                    // Fetch updated on-chain state
                    const onchainPet = await suiClient.getPet(pet.pet_object_id);

                    // Update local database to match on-chain
                    await petManager.updatePetStats(pet.pet_id, {
                        level: onchainPet.level,
                        happiness: onchainPet.happiness,
                        hunger: onchainPet.hunger,
                        health: onchainPet.health,
                        experience: onchainPet.experience,
                        food: onchainPet.food,
                        energy: onchainPet.energy
                    });
                }
            } catch (error) {
                console.warn('Failed to play with pet on blockchain:', error.message);
                // Update locally even if blockchain fails
                await petManager.playWithPetLocal(pet.pet_id);
            }
        } else {
            // No blockchain, update locally only
            await petManager.playWithPetLocal(pet.pet_id);
        }

        // Get updated pet
        const updatedPet = await petManager.getPetByDeviceId(deviceId);

        ws.send(JSON.stringify({
            type: 'pet_played',
            success: true,
            pet: updatedPet
        }));

        console.log(`🎮 Played with pet: ${pet.pet_name}`);
    } catch (error) {
        ws.send(JSON.stringify({
            type: 'pet_error',
            success: false,
            error: error.message
        }));
    }
}

// Graceful shutdown
process.on('SIGINT', () => {
    console.log('\n\n⏹️  Shutting down gracefully...');
    if (deviceManager) deviceManager.close();
    wss.close();
    server.close();
    process.exit(0);
});

// Start server
initializeServices().catch(err => {
    console.error('Failed to initialize services:', err);
    process.exit(1);
});
