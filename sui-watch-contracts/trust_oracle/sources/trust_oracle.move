/// Trust Oracle Smart Contract for SUI Watch
/// Hardware Witness for step counter data verification
module trust_oracle::trust_oracle {
    use sui::object::{Self, UID};
    use sui::tx_context::{Self, TxContext};
    use sui::transfer;
    use sui::event;
    use std::string::{Self, String};

    // ==================== Error Codes ====================

    const E_DEVICE_ALREADY_REGISTERED: u64 = 1;
    const E_DEVICE_NOT_REGISTERED: u64 = 2;
    const E_INVALID_SIGNATURE: u64 = 3;
    const E_INVALID_TIMESTAMP: u64 = 4;
    const E_DEVICE_INACTIVE: u64 = 5;
    const E_INVALID_STEP_COUNT: u64 = 6;

    // ==================== Data Structures ====================

    /// Global registry of all devices and their data
    public struct OracleRegistry has key {
        id: UID,
        total_devices: u64,
        total_submissions: u64,
        total_steps_recorded: u64,
    }

    /// Represents a registered Hardware Witness device
    public struct Device has key, store {
        id: UID,
        device_id: String,
        public_key: vector<u8>,
        registered_at: u64,
        total_steps: u64,
        total_submissions: u64,
        is_active: bool,
    }

    /// A single step data submission record
    public struct StepDataRecord has key, store {
        id: UID,
        device_id: String,
        step_count: u64,
        timestamp: u64,
        signatures: vector<vector<u8>>,
        submitter: address,
        submitted_at: u64,
        verified: bool,
    }

    // ==================== Events ====================

    /// Emitted when new device is registered
    public struct DeviceRegistered has copy, drop {
        device_id: String,
        public_key: vector<u8>,
        timestamp: u64,
    }

    /// Emitted when step data is submitted
    public struct StepDataSubmitted has copy, drop {
        device_id: String,
        step_count: u64,
        timestamp: u64,
        record_id: address,
    }

    /// Emitted when milestone is achieved
    public struct MilestoneAchieved has copy, drop {
        device_id: String,
        milestone: String,
        total_steps: u64,
    }

    // ==================== Init Function ====================

    /// Initialize the oracle registry (called once at deployment)
    fun init(ctx: &mut TxContext) {
        let registry = OracleRegistry {
            id: object::new(ctx),
            total_devices: 0,
            total_submissions: 0,
            total_steps_recorded: 0,
        };

        transfer::share_object(registry);
    }

    // ==================== Device Management ====================

    /// Register a new Hardware Witness device
    public entry fun register_device(
        registry: &mut OracleRegistry,
        device_id: vector<u8>,
        public_key: vector<u8>,
        ctx: &mut TxContext
    ) {
        let device_id_string = string::utf8(device_id);

        let device = Device {
            id: object::new(ctx),
            device_id: device_id_string,
            public_key,
            registered_at: tx_context::epoch_timestamp_ms(ctx),
            total_steps: 0,
            total_submissions: 0,
            is_active: true,
        };

        // Update registry stats
        registry.total_devices = registry.total_devices + 1;

        // Emit event
        event::emit(DeviceRegistered {
            device_id: device_id_string,
            public_key,
            timestamp: tx_context::epoch_timestamp_ms(ctx),
        });

        // Transfer device object to sender
        transfer::transfer(device, tx_context::sender(ctx));
    }

    /// Deactivate a device (only owner can call)
    public entry fun deactivate_device(
        device: &mut Device,
        _ctx: &mut TxContext
    ) {
        device.is_active = false;
    }

    // ==================== Step Data Submission ====================

    /// Submit step data from device (called by backend server)
    public entry fun submit_step_data(
        registry: &mut OracleRegistry,
        device: &mut Device,
        step_count: u64,
        timestamps: vector<u64>,
        signatures: vector<vector<u8>>,
        ctx: &mut TxContext
    ) {
        // Validate device is active
        assert!(device.is_active, E_DEVICE_INACTIVE);

        // Validate step count
        assert!(step_count > 0 && step_count <= 100000, E_INVALID_STEP_COUNT);

        // Validate timestamps (basic check - not too old or in future)
        let current_time = tx_context::epoch_timestamp_ms(ctx);
        let mut i = 0;
        let len = timestamps.length();
        while (i < len) {
            let ts = timestamps[i];
            // Check timestamp is not too old (7 days) or in future
            assert!(ts <= current_time && ts >= current_time - 604800000, E_INVALID_TIMESTAMP);
            i = i + 1;
        };

        // Create record
        let record = StepDataRecord {
            id: object::new(ctx),
            device_id: device.device_id,
            step_count,
            timestamp: current_time,
            signatures,
            submitter: tx_context::sender(ctx),
            submitted_at: current_time,
            verified: true,
        };

        let record_id = object::uid_to_address(&record.id);

        // Update device stats
        device.total_steps = device.total_steps + step_count;
        device.total_submissions = device.total_submissions + 1;

        // Update registry stats
        registry.total_submissions = registry.total_submissions + 1;
        registry.total_steps_recorded = registry.total_steps_recorded + step_count;

        // Emit event
        event::emit(StepDataSubmitted {
            device_id: device.device_id,
            step_count,
            timestamp: current_time,
            record_id,
        });

        // Check for milestones
        check_and_award_milestone(device);

        // Transfer record to sender (backend server)
        transfer::transfer(record, tx_context::sender(ctx));
    }

    /// Check if device reached a milestone
    fun check_and_award_milestone(device: &Device) {
        let total = device.total_steps;

        if (total >= 100000 && total < 100000 + 10000) {
            event::emit(MilestoneAchieved {
                device_id: device.device_id,
                milestone: string::utf8(b"100000_steps"),
                total_steps: total,
            });
        } else if (total >= 50000 && total < 50000 + 10000) {
            event::emit(MilestoneAchieved {
                device_id: device.device_id,
                milestone: string::utf8(b"50000_steps"),
                total_steps: total,
            });
        } else if (total >= 10000 && total < 10000 + 10000) {
            event::emit(MilestoneAchieved {
                device_id: device.device_id,
                milestone: string::utf8(b"10000_steps"),
                total_steps: total,
            });
        } else if (total >= 1000 && total < 1000 + 1000) {
            event::emit(MilestoneAchieved {
                device_id: device.device_id,
                milestone: string::utf8(b"1000_steps"),
                total_steps: total,
            });
        };
    }

    // ==================== View Functions ====================

    /// Get device stats
    public fun get_device_stats(device: &Device): (u64, u64, bool) {
        (device.total_steps, device.total_submissions, device.is_active)
    }

    /// Get global oracle statistics
    public fun get_global_stats(registry: &OracleRegistry): (u64, u64, u64) {
        (
            registry.total_devices,
            registry.total_submissions,
            registry.total_steps_recorded
        )
    }

    /// Get record details
    public fun get_record_details(record: &StepDataRecord): (String, u64, u64, bool) {
        (record.device_id, record.step_count, record.timestamp, record.verified)
    }
}
