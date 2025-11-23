/**
 * Sui Client
 * Handles all Sui blockchain interactions for Trust Oracle
 */

import { SuiClient as Client, getFullnodeUrl } from '@mysten/sui/client';
import { Transaction } from '@mysten/sui/transactions';
import { Ed25519Keypair } from '@mysten/sui/keypairs/ed25519';
import { decodeSuiPrivateKey } from '@mysten/sui/cryptography';

export class SuiClient {
    constructor(network, packageId, registryId, privateKey) {
        this.network = network || 'testnet';
        this.packageId = packageId;
        this.registryId = registryId;

        // Initialize Sui client
        this.client = new Client({ url: getFullnodeUrl(this.network) });

        // Initialize keypair from private key (supports both bech32 and hex formats)
        if (privateKey) {
            if (privateKey.startsWith('suiprivkey')) {
                // Bech32 encoded private key (from sui keytool export)
                const decoded = decodeSuiPrivateKey(privateKey);
                this.keypair = Ed25519Keypair.fromSecretKey(decoded.secretKey);
            } else {
                // Hex encoded private key (legacy support)
                const cleanKey = privateKey.startsWith('0x') ? privateKey.slice(2) : privateKey;
                const keyBytes = Uint8Array.from(Buffer.from(cleanKey, 'hex'));
                this.keypair = Ed25519Keypair.fromSecretKey(keyBytes);
            }
            this.address = this.keypair.getPublicKey().toSuiAddress();
        }

        console.log('✓ SuiClient initialized');
        console.log(`  Network: ${this.network}`);
        console.log(`  Package: ${this.packageId}`);
        console.log(`  Registry: ${this.registryId}`);
        if (this.address) {
            console.log(`  Address: ${this.address}`);
        }
    }

    /**
     * Register device on blockchain
     * @param {string} deviceId - Device ID (UTF-8 string)
     * @param {string} publicKeyHex - Device public key
     * @returns {object} Transaction result
     */
    async registerDevice(deviceId, publicKeyHex) {
        if (!this.keypair) {
            throw new Error('Wallet not initialized');
        }

        try {
            const tx = new Transaction();

            // Convert deviceId to bytes
            const deviceIdBytes = Array.from(new TextEncoder().encode(deviceId));

            // Convert public key hex to bytes (remove 0x prefix if present)
            const publicKeyClean = publicKeyHex.startsWith('0x')
                ? publicKeyHex.slice(2)
                : publicKeyHex;
            const publicKeyBytes = Array.from(Buffer.from(publicKeyClean, 'hex'));

            tx.moveCall({
                target: `${this.packageId}::trust_oracle::register_device`,
                arguments: [
                    tx.object(this.registryId),
                    tx.pure.vector('u8', deviceIdBytes),
                    tx.pure.vector('u8', publicKeyBytes),
                ],
            });

            // Execute transaction
            const result = await this.client.signAndExecuteTransaction({
                signer: this.keypair,
                transaction: tx,
                options: {
                    showEffects: true,
                    showObjectChanges: true,
                    showEvents: true,
                },
            });

            console.log(`✓ Device registered on-chain: ${deviceId}`);
            console.log(`  TX: ${result.digest}`);

            // Extract Device object ID from created objects
            const deviceObject = result.objectChanges?.find(
                change => change.type === 'created' &&
                         change.objectType?.includes('::Device')
            );

            return {
                success: true,
                txDigest: result.digest,
                deviceObjectId: deviceObject?.objectId,
                result
            };

        } catch (error) {
            console.error('✗ Failed to register device:', error.message);
            throw error;
        }
    }

    /**
     * Submit step data to blockchain
     * @param {string} deviceObjectId - Device object ID
     * @param {number} stepCount - Total step count
     * @param {number[]} timestamps - Array of timestamps (ms)
     * @param {string[]} signaturesHex - Array of signatures in hex
     * @returns {object} Transaction result
     */
    async submitStepData(deviceObjectId, stepCount, timestamps, signaturesHex) {
        if (!this.keypair) {
            throw new Error('Wallet not initialized');
        }

        try {
            const tx = new Transaction();

            // Convert signatures to bytes
            const signatures = signaturesHex.map(sigHex => {
                const clean = sigHex.startsWith('0x') ? sigHex.slice(2) : sigHex;
                return Array.from(Buffer.from(clean, 'hex'));
            });

            tx.moveCall({
                target: `${this.packageId}::trust_oracle::submit_step_data`,
                arguments: [
                    tx.object(this.registryId),
                    tx.object(deviceObjectId),
                    tx.pure.u64(stepCount),
                    tx.pure.vector('u64', timestamps),
                    tx.pure.vector('vector<u8>', signatures),
                ],
            });

            // Execute transaction
            const result = await this.client.signAndExecuteTransaction({
                signer: this.keypair,
                transaction: tx,
                options: {
                    showEffects: true,
                    showObjectChanges: true,
                    showEvents: true,
                },
            });

            console.log(`✓ Step data submitted on-chain`);
            console.log(`  TX: ${result.digest}`);
            console.log(`  Steps: ${stepCount}`);

            return {
                success: true,
                txDigest: result.digest,
                stepCount,
                result
            };

        } catch (error) {
            console.error('✗ Failed to submit step data:', error.message);
            throw error;
        }
    }

    /**
     * Batch submit step data for multiple devices
     * @param {Array} submissions - Array of {deviceObjectId, stepCount, timestamps, signatures}
     * @returns {Array} Results for each submission
     */
    async batchSubmit(submissions) {
        const results = [];

        for (const submission of submissions) {
            try {
                const result = await this.submitStepData(
                    submission.deviceObjectId,
                    submission.stepCount,
                    submission.timestamps,
                    submission.signatures
                );
                results.push({ ...submission, ...result });
            } catch (error) {
                results.push({
                    ...submission,
                    success: false,
                    error: error.message
                });
            }
        }

        return results;
    }

    /**
     * Get device object data
     * @param {string} deviceObjectId - Device object ID
     * @returns {object} Device data
     */
    async getDevice(deviceObjectId) {
        try {
            const object = await this.client.getObject({
                id: deviceObjectId,
                options: {
                    showContent: true,
                    showType: true,
                },
            });

            return object.data;
        } catch (error) {
            console.error('✗ Failed to get device:', error.message);
            throw error;
        }
    }

    /**
     * Get registry statistics
     * @returns {object} Registry stats
     */
    async getRegistryStats() {
        try {
            const object = await this.client.getObject({
                id: this.registryId,
                options: {
                    showContent: true,
                },
            });

            const fields = object.data?.content?.fields;

            return {
                total_devices: fields?.total_devices || 0,
                total_submissions: fields?.total_submissions || 0,
                total_steps_recorded: fields?.total_steps_recorded || 0,
            };
        } catch (error) {
            console.error('✗ Failed to get registry stats:', error.message);
            throw error;
        }
    }

    /**
     * Get events for device
     * @param {string} deviceId - Device ID string
     * @returns {Array} Events array
     */
    async getDeviceEvents(deviceId) {
        try {
            const events = await this.client.queryEvents({
                query: {
                    MoveEventType: `${this.packageId}::trust_oracle::StepDataSubmitted`,
                },
                limit: 50,
            });

            // Filter by device ID
            return events.data.filter(event =>
                event.parsedJson?.device_id === deviceId
            );
        } catch (error) {
            console.error('✗ Failed to get events:', error.message);
            throw error;
        }
    }

    /**
     * Check balance
     * @returns {string} Balance in SUI
     */
    async getBalance() {
        if (!this.address) {
            throw new Error('Wallet not initialized');
        }

        try {
            const balance = await this.client.getBalance({
                owner: this.address,
            });

            const sui = (parseInt(balance.totalBalance) / 1_000_000_000).toFixed(9);
            return sui;
        } catch (error) {
            console.error('✗ Failed to get balance:', error.message);
            throw error;
        }
    }

    // ============================================
    // Virtual Pet Functions
    // ============================================

    /**
     * Create a new virtual pet on-chain
     * @param {string} name - Pet name
     * @param {string} deviceId - Device ID
     * @param {string} color - Pet color
     * @returns {object} Transaction result with pet object ID
     */
    async createPet(name, deviceId, color = 'blue') {
        if (!this.keypair) {
            throw new Error('Wallet not initialized');
        }

        try {
            const tx = new Transaction();

            // Convert strings to bytes
            const nameBytes = Array.from(new TextEncoder().encode(name));
            const deviceIdBytes = Array.from(new TextEncoder().encode(deviceId));
            const colorBytes = Array.from(new TextEncoder().encode(color));

            // Get Clock object (0x6 is the shared Clock object on Sui)
            const clockId = '0x6';

            // Call create_pet function and get the VirtualPet object
            const [pet] = tx.moveCall({
                target: `${this.packageId}::virtual_pet::create_pet`,
                arguments: [
                    tx.pure.vector('u8', nameBytes),
                    tx.pure.vector('u8', deviceIdBytes),
                    tx.pure.vector('u8', colorBytes),
                    tx.object(clockId),
                ],
            });

            // Transfer pet to the device owner (for now, to the server wallet)
            tx.transferObjects([pet], this.address);

            // Execute transaction
            const result = await this.client.signAndExecuteTransaction({
                signer: this.keypair,
                transaction: tx,
                options: {
                    showEffects: true,
                    showObjectChanges: true,
                    showEvents: true,
                },
            });

            console.log(`✓ Virtual Pet created on-chain: ${name}`);
            console.log(`  TX: ${result.digest}`);

            // Extract Pet object ID from created objects
            const petObject = result.objectChanges?.find(
                change => change.type === 'created' &&
                         change.objectType?.includes('::VirtualPet')
            );

            return {
                success: true,
                txDigest: result.digest,
                petObjectId: petObject?.objectId,
                result
            };

        } catch (error) {
            console.error('✗ Failed to create pet:', error.message);
            throw error;
        }
    }

    /**
     * Claim resources from steps
     * @param {string} petObjectId - Pet object ID
     * @param {number} steps - Number of steps to claim
     * @returns {object} Transaction result with resources gained
     */
    async claimResources(petObjectId, steps) {
        if (!this.keypair) {
            throw new Error('Wallet not initialized');
        }

        try {
            const tx = new Transaction();

            tx.moveCall({
                target: `${this.packageId}::virtual_pet::claim_resources`,
                arguments: [
                    tx.object(petObjectId),
                    tx.pure.u64(steps),
                ],
            });

            // Execute transaction
            const result = await this.client.signAndExecuteTransaction({
                signer: this.keypair,
                transaction: tx,
                options: {
                    showEffects: true,
                    showEvents: true,
                },
            });

            console.log(`✓ Resources claimed from ${steps} steps`);
            console.log(`  TX: ${result.digest}`);

            // Extract ResourcesClaimed event
            const claimEvent = result.events?.find(
                event => event.type.includes('::ResourcesClaimed')
            );

            if (claimEvent) {
                console.log(`  Food gained: ${claimEvent.parsedJson.food_gained}`);
                console.log(`  Energy gained: ${claimEvent.parsedJson.energy_gained}`);
            }

            return {
                success: true,
                txDigest: result.digest,
                foodGained: claimEvent?.parsedJson?.food_gained,
                energyGained: claimEvent?.parsedJson?.energy_gained,
                newFood: claimEvent?.parsedJson?.new_food,
                newEnergy: claimEvent?.parsedJson?.new_energy,
                result
            };

        } catch (error) {
            console.error('✗ Failed to claim resources:', error.message);
            throw error;
        }
    }

    /**
     * Feed pet using food resource
     * @param {string} petObjectId - Pet object ID
     * @returns {object} Transaction result
     */
    async feedPet(petObjectId) {
        if (!this.keypair) {
            throw new Error('Wallet not initialized');
        }

        try {
            const tx = new Transaction();
            const clockId = '0x6';

            tx.moveCall({
                target: `${this.packageId}::virtual_pet::feed_pet`,
                arguments: [
                    tx.object(petObjectId),
                    tx.object(clockId),
                ],
            });

            // Execute transaction
            const result = await this.client.signAndExecuteTransaction({
                signer: this.keypair,
                transaction: tx,
                options: {
                    showEffects: true,
                    showEvents: true,
                },
            });

            console.log(`✓ Pet fed on-chain (used 1 food, +10 XP)`);
            console.log(`  TX: ${result.digest}`);

            // Extract evolution event if pet evolved
            const evolvedEvent = result.events?.find(
                event => event.type.includes('::PetEvolved')
            );

            if (evolvedEvent) {
                console.log(`  🎉 Pet evolved to level ${evolvedEvent.parsedJson.new_level}!`);
            }

            return {
                success: true,
                txDigest: result.digest,
                evolved: !!evolvedEvent,
                newLevel: evolvedEvent?.parsedJson?.new_level,
                result
            };

        } catch (error) {
            console.error('✗ Failed to feed pet:', error.message);
            throw error;
        }
    }

    /**
     * Play with pet using energy resource
     * @param {string} petObjectId - Pet object ID
     * @returns {object} Transaction result
     */
    async playWithPet(petObjectId) {
        if (!this.keypair) {
            throw new Error('Wallet not initialized');
        }

        try {
            const tx = new Transaction();
            const clockId = '0x6';

            tx.moveCall({
                target: `${this.packageId}::virtual_pet::play_with_pet`,
                arguments: [
                    tx.object(petObjectId),
                    tx.object(clockId),
                ],
            });

            // Execute transaction
            const result = await this.client.signAndExecuteTransaction({
                signer: this.keypair,
                transaction: tx,
                options: {
                    showEffects: true,
                    showEvents: true,
                },
            });

            console.log(`✓ Played with pet on-chain (used 1 energy, +5 XP, +3 HP)`);
            console.log(`  TX: ${result.digest}`);

            return {
                success: true,
                txDigest: result.digest,
                result
            };

        } catch (error) {
            console.error('✗ Failed to play with pet:', error.message);
            throw error;
        }
    }

    /**
     * Update pet status (time-based degradation)
     * @param {string} petObjectId - Pet object ID
     * @returns {object} Transaction result
     */
    async updatePetStatus(petObjectId) {
        if (!this.keypair) {
            throw new Error('Wallet not initialized');
        }

        try {
            const tx = new Transaction();
            const clockId = '0x6';

            tx.moveCall({
                target: `${this.packageId}::virtual_pet::update_pet_status`,
                arguments: [
                    tx.object(petObjectId),
                    tx.object(clockId),
                ],
            });

            // Execute transaction
            const result = await this.client.signAndExecuteTransaction({
                signer: this.keypair,
                transaction: tx,
                options: {
                    showEffects: true,
                },
            });

            console.log(`✓ Pet status updated on-chain`);
            console.log(`  TX: ${result.digest}`);

            return {
                success: true,
                txDigest: result.digest,
                result
            };

        } catch (error) {
            console.error('✗ Failed to update pet status:', error.message);
            throw error;
        }
    }

    /**
     * Give accessory to pet
     * @param {string} petObjectId - Pet object ID
     * @param {string} accessory - Accessory name
     * @returns {object} Transaction result
     */
    async giveAccessory(petObjectId, accessory) {
        if (!this.keypair) {
            throw new Error('Wallet not initialized');
        }

        try {
            const tx = new Transaction();
            const accessoryBytes = Array.from(new TextEncoder().encode(accessory));

            tx.moveCall({
                target: `${this.packageId}::virtual_pet::give_accessory`,
                arguments: [
                    tx.object(petObjectId),
                    tx.pure.vector('u8', accessoryBytes),
                ],
            });

            // Execute transaction
            const result = await this.client.signAndExecuteTransaction({
                signer: this.keypair,
                transaction: tx,
                options: {
                    showEffects: true,
                },
            });

            console.log(`✓ Gave accessory "${accessory}" to pet on-chain`);
            console.log(`  TX: ${result.digest}`);

            return {
                success: true,
                txDigest: result.digest,
                accessory,
                result
            };

        } catch (error) {
            console.error('✗ Failed to give accessory:', error.message);
            throw error;
        }
    }

    /**
     * Get pet object data
     * @param {string} petObjectId - Pet object ID
     * @returns {object} Pet data
     */
    async getPet(petObjectId) {
        try {
            const object = await this.client.getObject({
                id: petObjectId,
                options: {
                    showContent: true,
                    showType: true,
                },
            });

            const fields = object.data?.content?.fields;

            if (!fields) {
                return null;
            }

            // Parse pet data from on-chain fields
            return {
                id: petObjectId,
                name: fields.name,
                device_id: fields.device_id,
                level: parseInt(fields.level),
                experience: parseInt(fields.experience),
                total_steps_fed: parseInt(fields.total_steps_fed),
                happiness: parseInt(fields.happiness),
                hunger: parseInt(fields.hunger),
                health: parseInt(fields.health),
                food: parseInt(fields.food),
                energy: parseInt(fields.energy),
                birth_time: parseInt(fields.birth_time),
                last_fed_time: parseInt(fields.last_fed_time),
                last_play_time: parseInt(fields.last_play_time),
                color: fields.color,
                accessory: fields.accessory,
            };
        } catch (error) {
            console.error('✗ Failed to get pet:', error.message);
            throw error;
        }
    }

    /**
     * Get pet events
     * @param {string} petObjectId - Pet object ID
     * @returns {Array} Pet events
     */
    async getPetEvents(petObjectId) {
        try {
            const events = await this.client.queryEvents({
                query: {
                    MoveModule: {
                        package: this.packageId,
                        module: 'virtual_pet',
                    },
                },
                limit: 100,
            });

            // Filter by pet ID
            return events.data.filter(event =>
                event.parsedJson?.pet_id === petObjectId
            );
        } catch (error) {
            console.error('✗ Failed to get pet events:', error.message);
            throw error;
        }
    }
}
