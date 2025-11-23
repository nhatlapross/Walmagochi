module trust_oracle::virtual_pet {
    use sui::object::{Self, UID};
    use sui::transfer;
    use sui::tx_context::{Self, TxContext};
    use sui::clock::{Self, Clock};
    use std::string::{Self, String};
    use std::option::{Self, Option};
    use sui::event;

    // ============================================
    // Constants
    // ============================================

    const MAX_HAPPINESS: u64 = 100;
    const MAX_HUNGER: u64 = 100;
    const MAX_HEALTH: u64 = 100;

    // Evolution levels
    const LEVEL_EGG: u8 = 0;
    const LEVEL_BABY: u8 = 1;
    const LEVEL_TEEN: u8 = 2;
    const LEVEL_ADULT: u8 = 3;
    const LEVEL_MASTER: u8 = 4;

    // Steps required for evolution
    const STEPS_TO_BABY: u64 = 1000;
    const STEPS_TO_TEEN: u64 = 10000;
    const STEPS_TO_ADULT: u64 = 50000;
    const STEPS_TO_MASTER: u64 = 100000;

    // Errors
    const EPetNotHungry: u64 = 1;
    const EInsufficientSteps: u64 = 2;
    const EPetUnhappy: u64 = 3;
    const EAlreadyMaxLevel: u64 = 4;
    const EInsufficientFood: u64 = 5;
    const EInsufficientEnergy: u64 = 6;

    // ============================================
    // Structs
    // ============================================

    /// Virtual Pet NFT
    public struct VirtualPet has key, store {
        id: UID,
        name: String,
        device_id: String,

        // Stats
        level: u8,
        experience: u64,
        total_steps_fed: u64,

        // Status (0-100)
        happiness: u64,
        hunger: u64,
        health: u64,

        // Resources (earned from walking)
        food: u64,
        energy: u64,

        // Timestamps
        birth_time: u64,
        last_fed_time: u64,
        last_play_time: u64,

        // Appearance
        color: String,
        accessory: String,
    }

    /// Pet Registry - tracks all pets for a device
    public struct PetRegistry has key {
        id: UID,
        device_id: String,
        active_pet_id: Option<ID>,
        total_pets: u64,
    }

    // ============================================
    // Events
    // ============================================

    public struct PetCreated has copy, drop {
        pet_id: ID,
        device_id: String,
        name: String,
    }

    public struct PetFed has copy, drop {
        pet_id: ID,
        steps_used: u64,
        new_hunger: u64,
        new_happiness: u64,
    }

    public struct PetEvolved has copy, drop {
        pet_id: ID,
        old_level: u8,
        new_level: u8,
        total_steps: u64,
    }

    public struct PetPlayed has copy, drop {
        pet_id: ID,
        new_happiness: u64,
        xp_gained: u64,
    }

    public struct ResourcesClaimed has copy, drop {
        pet_id: ID,
        steps_used: u64,
        food_gained: u64,
        energy_gained: u64,
        new_food: u64,
        new_energy: u64,
    }

    // ============================================
    // Public Functions
    // ============================================

    /// Create a new pet for a device
    public fun create_pet(
        name: vector<u8>,
        device_id: vector<u8>,
        color: vector<u8>,
        clock: &Clock,
        ctx: &mut TxContext
    ): VirtualPet {
        let pet = VirtualPet {
            id: object::new(ctx),
            name: string::utf8(name),
            device_id: string::utf8(device_id),

            level: LEVEL_EGG,
            experience: 0,
            total_steps_fed: 0,

            happiness: 50,
            hunger: 50,
            health: 100,

            food: 5,
            energy: 5,

            birth_time: clock::timestamp_ms(clock),
            last_fed_time: clock::timestamp_ms(clock),
            last_play_time: clock::timestamp_ms(clock),

            color: string::utf8(color),
            accessory: string::utf8(b"none"),
        };

        event::emit(PetCreated {
            pet_id: object::id(&pet),
            device_id: pet.device_id,
            name: pet.name,
        });

        pet
    }

    /// Create and transfer a new pet to sender
    public entry fun create_and_transfer_pet(
        name: vector<u8>,
        device_id: vector<u8>,
        color: vector<u8>,
        clock: &Clock,
        ctx: &mut TxContext
    ) {
        let pet = create_pet(name, device_id, color, clock, ctx);
        transfer::public_transfer(pet, tx_context::sender(ctx));
    }

    /// Claim resources from steps
    public fun claim_resources(
        pet: &mut VirtualPet,
        steps: u64,
    ) {
        assert!(steps >= 100, EInsufficientSteps);

        // Calculate resources: 100 steps = 1 food, 150 steps = 2 energy
        let food_gained = steps / 100;
        let energy_gained = (steps / 150) * 2;

        // Add resources to pet
        pet.food = pet.food + food_gained;
        pet.energy = pet.energy + energy_gained;

        event::emit(ResourcesClaimed {
            pet_id: object::id(pet),
            steps_used: steps,
            food_gained,
            energy_gained,
            new_food: pet.food,
            new_energy: pet.energy,
        });
    }

    /// Feed pet using food resource
    public fun feed_pet(
        pet: &mut VirtualPet,
        clock: &Clock,
    ) {
        assert!(pet.food > 0, EInsufficientFood);

        // Use 1 food
        pet.food = pet.food - 1;

        // Increase hunger by 25
        pet.hunger = if (pet.hunger + 25 > MAX_HUNGER) {
            MAX_HUNGER
        } else {
            pet.hunger + 25
        };

        // Feeding makes pet happy (+5)
        pet.happiness = if (pet.happiness + 5 > MAX_HAPPINESS) {
            MAX_HAPPINESS
        } else {
            pet.happiness + 5
        };

        // Add 10 XP for feeding
        pet.experience = pet.experience + 10;

        // Update timestamp
        pet.last_fed_time = clock::timestamp_ms(clock);

        // Check for evolution
        check_evolution(pet);

        event::emit(PetFed {
            pet_id: object::id(pet),
            steps_used: 0,  // No longer using steps directly
            new_hunger: pet.hunger,
            new_happiness: pet.happiness,
        });
    }

    /// Play with pet using energy resource
    public fun play_with_pet(
        pet: &mut VirtualPet,
        clock: &Clock,
    ) {
        assert!(pet.energy > 0, EInsufficientEnergy);

        // Use 1 energy
        pet.energy = pet.energy - 1;

        // Increase happiness by 15
        pet.happiness = if (pet.happiness + 15 > MAX_HAPPINESS) {
            MAX_HAPPINESS
        } else {
            pet.happiness + 15
        };

        // Add 5 XP for playing
        pet.experience = pet.experience + 5;

        // Restore 3 HP (health) when playing
        pet.health = if (pet.health + 3 > MAX_HEALTH) {
            MAX_HEALTH
        } else {
            pet.health + 3
        };

        // Update timestamp
        pet.last_play_time = clock::timestamp_ms(clock);

        event::emit(PetPlayed {
            pet_id: object::id(pet),
            new_happiness: pet.happiness,
            xp_gained: 5,
        });
    }

    /// Update pet status (called periodically)
    public fun update_pet_status(
        pet: &mut VirtualPet,
        clock: &Clock,
    ) {
        let current_time = clock::timestamp_ms(clock);
        let time_since_fed = current_time - pet.last_fed_time;
        let time_since_play = current_time - pet.last_play_time;

        // Decrease hunger over time (1 point per hour)
        if (time_since_fed > 3600000 && pet.hunger > 0) {
            let hunger_loss = time_since_fed / 3600000;
            pet.hunger = if (hunger_loss >= pet.hunger) {
                0
            } else {
                pet.hunger - hunger_loss
            };
        };

        // Decrease happiness if not played with (1 point per 2 hours)
        if (time_since_play > 7200000 && pet.happiness > 0) {
            let happiness_loss = time_since_play / 7200000;
            pet.happiness = if (happiness_loss >= pet.happiness) {
                0
            } else {
                pet.happiness - happiness_loss
            };
        };

        // Health affected by hunger and happiness
        if (pet.hunger < 20 || pet.happiness < 20) {
            if (pet.health > 0) {
                pet.health = pet.health - 1;
            }
        } else if (pet.hunger > 80 && pet.happiness > 80 && pet.health < MAX_HEALTH) {
            pet.health = pet.health + 1;
        }
    }

    /// Give accessory to pet
    public fun give_accessory(
        pet: &mut VirtualPet,
        accessory: vector<u8>,
    ) {
        pet.accessory = string::utf8(accessory);
    }

    /// Transfer pet to another owner
    public fun transfer_pet(
        pet: VirtualPet,
        recipient: address,
    ) {
        transfer::public_transfer(pet, recipient);
    }

    // ============================================
    // Internal Functions
    // ============================================

    fun check_evolution(pet: &mut VirtualPet) {
        let old_level = pet.level;

        // Evolution based on experience points
        // BABY: 100 XP (10 feeds or 20 plays)
        // TEEN: 500 XP
        // ADULT: 2000 XP
        // MASTER: 5000 XP
        if (pet.experience >= 5000 && pet.level < LEVEL_MASTER) {
            pet.level = LEVEL_MASTER;
        } else if (pet.experience >= 2000 && pet.level < LEVEL_ADULT) {
            pet.level = LEVEL_ADULT;
        } else if (pet.experience >= 500 && pet.level < LEVEL_TEEN) {
            pet.level = LEVEL_TEEN;
        } else if (pet.experience >= 100 && pet.level < LEVEL_BABY) {
            pet.level = LEVEL_BABY;
        };

        if (old_level != pet.level) {
            event::emit(PetEvolved {
                pet_id: object::id(pet),
                old_level,
                new_level: pet.level,
                total_steps: pet.total_steps_fed,
            });
        }
    }

    // ============================================
    // Getter Functions
    // ============================================

    public fun get_pet_stats(pet: &VirtualPet): (u64, u64, u64, u8, u64) {
        (pet.happiness, pet.hunger, pet.health, pet.level, pet.total_steps_fed)
    }

    public fun get_pet_info(pet: &VirtualPet): (String, String, String, String) {
        (pet.name, pet.device_id, pet.color, pet.accessory)
    }

    public fun is_pet_happy(pet: &VirtualPet): bool {
        pet.happiness >= 60
    }

    public fun is_pet_hungry(pet: &VirtualPet): bool {
        pet.hunger < 40
    }

    public fun needs_attention(pet: &VirtualPet): bool {
        pet.happiness < 30 || pet.hunger < 30 || pet.health < 50
    }

    public fun get_pet_resources(pet: &VirtualPet): (u64, u64) {
        (pet.food, pet.energy)
    }
}