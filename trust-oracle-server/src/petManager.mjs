/**
 * Pet Manager - Handles virtual pet operations
 * - Pet creation and management
 * - Blockchain sync for pet NFTs
 * - Pet state updates
 */

import Database from 'better-sqlite3';
import path from 'path';
import { fileURLToPath } from 'url';

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);

export class PetManager {
    constructor(dbPath = path.join(__dirname, '../database/pets.db')) {
        this.dbPath = dbPath;
        this.db = null;
    }

    async initDatabase() {
        this.db = new Database(this.dbPath);

        // Create pets table
        this.db.exec(`
            CREATE TABLE IF NOT EXISTS pets (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                device_id TEXT NOT NULL,
                pet_name TEXT NOT NULL,
                pet_id TEXT UNIQUE,  -- Internal pet ID
                pet_object_id TEXT,  -- On-chain NFT object ID

                -- Pet stats
                level INTEGER DEFAULT 0,
                experience INTEGER DEFAULT 0,
                total_steps_fed INTEGER DEFAULT 0,

                -- Status (0-100)
                happiness INTEGER DEFAULT 50,
                hunger INTEGER DEFAULT 50,
                health INTEGER DEFAULT 100,

                -- Resources (earned from walking)
                food INTEGER DEFAULT 5,
                energy INTEGER DEFAULT 5,

                -- Timestamps
                created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
                last_fed_at DATETIME DEFAULT CURRENT_TIMESTAMP,
                last_played_at DATETIME DEFAULT CURRENT_TIMESTAMP,
                last_updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,

                -- Appearance
                color TEXT DEFAULT 'blue',
                accessory TEXT DEFAULT 'none',

                -- Blockchain
                on_chain BOOLEAN DEFAULT FALSE,
                tx_digest TEXT
            )
        `);

        // Create pet events table
        this.db.exec(`
            CREATE TABLE IF NOT EXISTS pet_events (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                pet_id TEXT NOT NULL,
                event_type TEXT NOT NULL,  -- 'fed', 'played', 'evolved', 'accessory'
                event_data TEXT,
                timestamp DATETIME DEFAULT CURRENT_TIMESTAMP,

                FOREIGN KEY (pet_id) REFERENCES pets(pet_id)
            )
        `);

        // Migration: Add food and energy columns if they don't exist
        try {
            // Check if columns exist by trying to select them
            this.db.prepare('SELECT food, energy FROM pets LIMIT 1').get();
            console.log('✓ Food and energy columns already exist');
        } catch (error) {
            // Columns don't exist, add them
            console.log('🔄 Adding food and energy columns...');
            this.db.exec(`
                ALTER TABLE pets ADD COLUMN food INTEGER DEFAULT 5;
                ALTER TABLE pets ADD COLUMN energy INTEGER DEFAULT 5;
            `);
            console.log('✓ Food and energy columns added');
        }

        // Migration: Fix NULL values in food and energy columns
        const nullResourcePets = this.db.prepare(
            'SELECT COUNT(*) as count FROM pets WHERE food IS NULL OR energy IS NULL'
        ).get();

        if (nullResourcePets.count > 0) {
            console.log(`🔄 Fixing ${nullResourcePets.count} pets with NULL resources...`);
            this.db.prepare('UPDATE pets SET food = 5 WHERE food IS NULL').run();
            this.db.prepare('UPDATE pets SET energy = 5 WHERE energy IS NULL').run();
            console.log('✓ Fixed NULL resources');
        }

        console.log('✓ Pet database initialized');
    }

    // Create or get pet for device
    async getOrCreatePet(deviceId, petName = null) {
        // Check if pet exists
        const existing = this.db.prepare(
            'SELECT * FROM pets WHERE device_id = ?'
        ).get(deviceId);

        if (existing) {
            return existing;
        }

        // Create new pet
        const name = petName || `Pet_${Date.now()}`;
        const result = this.db.prepare(`
            INSERT INTO pets (device_id, pet_name, pet_id)
            VALUES (?, ?, ?)
        `).run(deviceId, name, `pet_${deviceId}_${Date.now()}`);

        return this.getPetById(result.lastInsertRowid);
    }

    // Get pet by ID
    getPetById(id) {
        return this.db.prepare('SELECT * FROM pets WHERE id = ?').get(id);
    }

    // Get pet by device ID
    getPetByDeviceId(deviceId) {
        return this.db.prepare('SELECT * FROM pets WHERE device_id = ?').get(deviceId);
    }

    // Update pet stats
    updatePetStats(petId, stats) {
        const { happiness, hunger, health, experience, total_steps_fed, level, food, energy } = stats;

        return this.db.prepare(`
            UPDATE pets
            SET happiness = ?, hunger = ?, health = ?,
                experience = ?, total_steps_fed = ?, level = ?,
                food = ?, energy = ?,
                last_updated_at = CURRENT_TIMESTAMP
            WHERE pet_id = ?
        `).run(happiness, hunger, health, experience, total_steps_fed, level, food, energy, petId);
    }

    // Update pet resources (absolute values)
    updatePetResources(petId, food, energy) {
        return this.db.prepare(`
            UPDATE pets
            SET food = ?, energy = ?,
                last_updated_at = CURRENT_TIMESTAMP
            WHERE pet_id = ?
        `).run(food, energy, petId);
    }

    // Add pet resources (relative values)
    addPetResources(petId, foodToAdd, energyToAdd) {
        const pet = this.db.prepare('SELECT food, energy FROM pets WHERE pet_id = ?').get(petId);
        if (!pet) return null;

        const newFood = pet.food + foodToAdd;
        const newEnergy = pet.energy + energyToAdd;

        return this.db.prepare(`
            UPDATE pets
            SET food = ?, energy = ?,
                last_updated_at = CURRENT_TIMESTAMP
            WHERE pet_id = ?
        `).run(newFood, newEnergy, petId);
    }

    // Feed pet (old version with steps - kept for backwards compatibility)
    feedPet(petId, steps) {
        // Get current pet
        const pet = this.db.prepare('SELECT * FROM pets WHERE pet_id = ?').get(petId);
        if (!pet) return null;

        // Calculate nutrition
        const nutrition = Math.floor(steps / 100) * 10;
        const newHunger = Math.min(100, pet.hunger + nutrition);
        const newHappiness = Math.min(100, pet.happiness + 5);
        const newTotalSteps = pet.total_steps_fed + steps;
        const newExperience = pet.experience + steps;

        // Check for evolution
        let newLevel = pet.level;
        if (newTotalSteps >= 100000 && pet.level < 4) newLevel = 4;  // Master
        else if (newTotalSteps >= 50000 && pet.level < 3) newLevel = 3;  // Adult
        else if (newTotalSteps >= 10000 && pet.level < 2) newLevel = 2;  // Teen
        else if (newTotalSteps >= 1000 && pet.level < 1) newLevel = 1;   // Baby

        // Update pet
        this.db.prepare(`
            UPDATE pets
            SET hunger = ?, happiness = ?,
                total_steps_fed = ?, experience = ?, level = ?,
                last_fed_at = CURRENT_TIMESTAMP,
                last_updated_at = CURRENT_TIMESTAMP
            WHERE pet_id = ?
        `).run(newHunger, newHappiness, newTotalSteps, newExperience, newLevel, petId);

        // Log event
        this.logPetEvent(petId, 'fed', { steps, nutrition, newLevel });

        // Check if evolved
        if (newLevel > pet.level) {
            this.logPetEvent(petId, 'evolved', { from: pet.level, to: newLevel });
        }

        return this.db.prepare('SELECT * FROM pets WHERE pet_id = ?').get(petId);
    }

    // Feed pet using food resource (new version)
    feedPetLocal(petId) {
        const pet = this.db.prepare('SELECT * FROM pets WHERE pet_id = ?').get(petId);
        if (!pet) return null;
        if (pet.food <= 0) throw new Error('No food available');

        // Use 1 food
        const newFood = pet.food - 1;

        // +25 hunger, +5 happiness, +10 XP
        const newHunger = Math.min(100, pet.hunger + 25);
        const newHappiness = Math.min(100, pet.happiness + 5);
        const newExperience = pet.experience + 10;

        // Check for evolution based on XP
        let newLevel = pet.level;
        if (newExperience >= 5000 && pet.level < 4) newLevel = 4;  // Master
        else if (newExperience >= 2000 && pet.level < 3) newLevel = 3;  // Adult
        else if (newExperience >= 500 && pet.level < 2) newLevel = 2;  // Teen
        else if (newExperience >= 100 && pet.level < 1) newLevel = 1;   // Baby

        // Update pet
        this.db.prepare(`
            UPDATE pets
            SET food = ?, hunger = ?, happiness = ?, experience = ?, level = ?,
                last_fed_at = CURRENT_TIMESTAMP,
                last_updated_at = CURRENT_TIMESTAMP
            WHERE pet_id = ?
        `).run(newFood, newHunger, newHappiness, newExperience, newLevel, petId);

        // Log event
        this.logPetEvent(petId, 'fed', { foodUsed: 1, xpGained: 10, newLevel });

        // Check if evolved
        if (newLevel > pet.level) {
            this.logPetEvent(petId, 'evolved', { from: pet.level, to: newLevel });
        }

        return this.db.prepare('SELECT * FROM pets WHERE pet_id = ?').get(petId);
    }

    // Play with pet (old version - kept for backwards compatibility)
    playWithPet(petId) {
        const pet = this.db.prepare('SELECT * FROM pets WHERE pet_id = ?').get(petId);
        if (!pet) return null;

        const newHappiness = Math.min(100, pet.happiness + 20);

        this.db.prepare(`
            UPDATE pets
            SET happiness = ?,
                last_played_at = CURRENT_TIMESTAMP,
                last_updated_at = CURRENT_TIMESTAMP
            WHERE pet_id = ?
        `).run(newHappiness, petId);

        this.logPetEvent(petId, 'played', { newHappiness });

        return this.db.prepare('SELECT * FROM pets WHERE pet_id = ?').get(petId);
    }

    // Play with pet using energy resource (new version)
    playWithPetLocal(petId) {
        const pet = this.db.prepare('SELECT * FROM pets WHERE pet_id = ?').get(petId);
        if (!pet) return null;
        if (pet.energy <= 0) throw new Error('No energy available');

        // Use 1 energy
        const newEnergy = pet.energy - 1;

        // +15 happiness, +5 XP, +3 HP
        const newHappiness = Math.min(100, pet.happiness + 15);
        const newHealth = Math.min(100, pet.health + 3);
        const newExperience = pet.experience + 5;

        // Update pet
        this.db.prepare(`
            UPDATE pets
            SET energy = ?, happiness = ?, health = ?, experience = ?,
                last_played_at = CURRENT_TIMESTAMP,
                last_updated_at = CURRENT_TIMESTAMP
            WHERE pet_id = ?
        `).run(newEnergy, newHappiness, newHealth, newExperience, petId);

        this.logPetEvent(petId, 'played', { energyUsed: 1, xpGained: 5, hpGained: 3 });

        return this.db.prepare('SELECT * FROM pets WHERE pet_id = ?').get(petId);
    }

    // Give accessory
    giveAccessory(petId, accessory) {
        this.db.prepare(`
            UPDATE pets
            SET accessory = ?,
                last_updated_at = CURRENT_TIMESTAMP
            WHERE pet_id = ?
        `).run(accessory, petId);

        this.logPetEvent(petId, 'accessory', { accessory });

        return this.db.prepare('SELECT * FROM pets WHERE pet_id = ?').get(petId);
    }

    // Update time-based stats
    updateTimeBasedStats(petId) {
        const pet = this.db.prepare('SELECT * FROM pets WHERE pet_id = ?').get(petId);
        if (!pet) return null;

        const now = Date.now();
        const timeSinceFed = now - new Date(pet.last_fed_at).getTime();
        const timeSincePlayed = now - new Date(pet.last_played_at).getTime();

        // Decrease hunger over time (1 point per hour)
        let newHunger = pet.hunger;
        if (timeSinceFed > 3600000) {
            const hungerLoss = Math.floor(timeSinceFed / 3600000);
            newHunger = Math.max(0, pet.hunger - hungerLoss);
        }

        // Decrease happiness if not played (1 point per 2 hours)
        let newHappiness = pet.happiness;
        if (timeSincePlayed > 7200000) {
            const happinessLoss = Math.floor(timeSincePlayed / 7200000);
            newHappiness = Math.max(0, pet.happiness - happinessLoss);
        }

        // Health affected by hunger and happiness
        let newHealth = pet.health;
        if (newHunger < 20 || newHappiness < 20) {
            newHealth = Math.max(0, pet.health - 1);
        } else if (newHunger > 80 && newHappiness > 80) {
            newHealth = Math.min(100, pet.health + 1);
        }

        // Update if changed
        if (newHunger !== pet.hunger || newHappiness !== pet.happiness || newHealth !== pet.health) {
            this.db.prepare(`
                UPDATE pets
                SET hunger = ?, happiness = ?, health = ?,
                    last_updated_at = CURRENT_TIMESTAMP
                WHERE pet_id = ?
            `).run(newHunger, newHappiness, newHealth, petId);
        }

        return this.db.prepare('SELECT * FROM pets WHERE pet_id = ?').get(petId);
    }

    // Mark pet as on-chain
    markPetOnChain(petId, petObjectId, txDigest) {
        return this.db.prepare(`
            UPDATE pets
            SET on_chain = TRUE, pet_object_id = ?, tx_digest = ?,
                last_updated_at = CURRENT_TIMESTAMP
            WHERE pet_id = ?
        `).run(petObjectId, txDigest, petId);
    }

    // Log pet event
    logPetEvent(petId, eventType, eventData) {
        return this.db.prepare(`
            INSERT INTO pet_events (pet_id, event_type, event_data)
            VALUES (?, ?, ?)
        `).run(petId, eventType, JSON.stringify(eventData));
    }

    // Get pet events
    getPetEvents(petId, limit = 10) {
        return this.db.prepare(`
            SELECT * FROM pet_events
            WHERE pet_id = ?
            ORDER BY timestamp DESC
            LIMIT ?
        `).all(petId, limit);
    }

    // Get all pets needing attention
    getPetsNeedingAttention() {
        return this.db.prepare(`
            SELECT * FROM pets
            WHERE happiness < 30 OR hunger < 30 OR health < 50
        `).all();
    }

    // Get leaderboard
    getLeaderboard(limit = 10) {
        return this.db.prepare(`
            SELECT device_id, pet_name, level, total_steps_fed, happiness
            FROM pets
            ORDER BY total_steps_fed DESC
            LIMIT ?
        `).all(limit);
    }

    // Clean up old events
    cleanupOldEvents(daysToKeep = 30) {
        const cutoffDate = new Date();
        cutoffDate.setDate(cutoffDate.getDate() - daysToKeep);

        return this.db.prepare(`
            DELETE FROM pet_events
            WHERE timestamp < ?
        `).run(cutoffDate.toISOString());
    }
}