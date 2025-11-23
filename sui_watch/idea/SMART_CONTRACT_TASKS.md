# Smart Contract Development Tasks (Move)

## Overview
Tasks cho phát triển smart contract trên Sui blockchain để lưu trữ và xác thực dữ liệu từ Hardware Witness devices.

---

## Setup & Environment

### Task 0.1: Sui Development Environment Setup
**Priority**: Critical
**Estimated Time**: 2 hours
**Dependencies**: None

**Installation Steps**:
```bash
# Install Sui CLI
cargo install --locked --git https://github.com/MystenLabs/sui.git --branch main sui

# Verify installation
sui --version

# Create new Sui wallet
sui client new-address ed25519

# Get testnet SUI tokens
curl --location --request POST 'https://faucet.testnet.sui.io/gas' \
--header 'Content-Type: application/json' \
--data-raw '{"FixedAmountRequest": {"recipient": "YOUR_ADDRESS"}}'

# Check balance
sui client gas
```

**Subtasks**:
- [ ] Install Sui CLI
- [ ] Create wallet
- [ ] Request testnet tokens
- [ ] Configure network
- [ ] Test transaction

---

### Task 0.2: Project Initialization
**Priority**: Critical
**Estimated Time**: 1 hour
**Dependencies**: Task 0.1

**Project Structure**:
```bash
mkdir sui-watch-contracts
cd sui-watch-contracts
sui move new trust_oracle
```

**Move.toml**:
```toml
[package]
name = "trust_oracle"
version = "1.0.0"
edition = "2024"

[dependencies]
Sui = { git = "https://github.com/MystenLabs/sui.git", subdir = "crates/sui-framework/packages/sui-framework", rev = "framework/mainnet" }

[addresses]
trust_oracle = "0x0"
```

**Subtasks**:
- [ ] Create Move project
- [ ] Configure Move.toml
- [ ] Create sources directory
- [ ] Setup tests directory

---

## Module 1: Core Trust Oracle Contract

### Task 1.1: Define Data Structures
**Priority**: Critical
**Estimated Time**: 4 hours
**Dependencies**: Task 0.2

**Implementation**:
```move
// File: sources/trust_oracle.move
module trust_oracle::trust_oracle {
    use sui::object::{Self, UID};
    use sui::tx_context::{Self, TxContext};
    use sui::transfer;
    use sui::event;
    use std::vector;
    use std::string::{Self, String};

    // ==================== Data Structures ====================

    /// Global registry of all devices and their data
    struct OracleRegistry has key {
        id: UID,
        total_devices: u64,
        total_submissions: u64,
        total_steps_recorded: u64,
    }

    /// Represents a registered Hardware Witness device
    struct Device has key, store {
        id: UID,
        device_id: String,
        public_key: vector<u8>,
        registered_at: u64,
        total_steps: u64,
        total_submissions: u64,
        is_active: bool,
    }

    /// A single step data submission record
    struct StepDataRecord has key, store {
        id: UID,
        device_id: String,
        step_count: u64,
        timestamp: u64,
        signatures: vector<vector<u8>>,  // Multiple signatures for batch
        submitter: address,
        submitted_at: u64,
        verified: bool,
    }

    /// Achievement NFT for milestones
    struct AchievementNFT has key, store {
        id: UID,
        device_id: String,
        milestone: String,  // "1000_steps", "10000_steps", etc.
        achieved_at: u64,
        image_url: String,
    }

    // ==================== Events ====================

    /// Emitted when new device is registered
    struct DeviceRegistered has copy, drop {
        device_id: String,
        public_key: vector<u8>,
        timestamp: u64,
    }

    /// Emitted when step data is submitted
    struct StepDataSubmitted has copy, drop {
        device_id: String,
        step_count: u64,
        timestamp: u64,
        record_id: address,
    }

    /// Emitted when milestone is achieved
    struct MilestoneAchieved has copy, drop {
        device_id: String,
        milestone: String,
        total_steps: u64,
    }

    // ==================== Error Codes ====================

    const E_DEVICE_ALREADY_REGISTERED: u64 = 1;
    const E_DEVICE_NOT_REGISTERED: u64 = 2;
    const E_INVALID_SIGNATURE: u64 = 3;
    const E_INVALID_TIMESTAMP: u64 = 4;
    const E_DEVICE_INACTIVE: u64 = 5;
    const E_INVALID_STEP_COUNT: u64 = 6;

    // ==================== Init ====================

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
}
```

**Subtasks**:
- [ ] Define OracleRegistry struct
- [ ] Define Device struct
- [ ] Define StepDataRecord struct
- [ ] Define AchievementNFT struct
- [ ] Define events
- [ ] Define error codes
- [ ] Implement init function

**Test Cases**:
- [ ] Contract deploys successfully
- [ ] Registry is shared object
- [ ] Initial values are correct

---

### Task 1.2: Device Registration Functions
**Priority**: High
**Estimated Time**: 4 hours
**Dependencies**: Task 1.1

**Implementation**:
```move
// Continue in trust_oracle.move

/// Register a new Hardware Witness device
public entry fun register_device(
    registry: &mut OracleRegistry,
    device_id: vector<u8>,
    public_key: vector<u8>,
    ctx: &mut TxContext
) {
    let device_id_string = string::utf8(device_id);

    // TODO: Check if device already exists (would need TableVec or similar)

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
    ctx: &mut TxContext
) {
    // In production, add access control here
    device.is_active = false;
}

/// Get device stats (view function)
public fun get_device_stats(device: &Device): (u64, u64, bool) {
    (device.total_steps, device.total_submissions, device.is_active)
}
```

**Subtasks**:
- [ ] Implement register_device
- [ ] Add duplicate check
- [ ] Implement deactivate_device
- [ ] Add access control
- [ ] Implement get_device_stats
- [ ] Add device lookup by ID

**Test Cases**:
- [ ] Device registration succeeds
- [ ] Duplicate registration fails
- [ ] Deactivation works
- [ ] Stats query works

---

### Task 1.3: Step Data Submission Functions
**Priority**: Critical
**Estimated Time**: 6 hours
**Dependencies**: Task 1.2

**Implementation**:
```move
// Continue in trust_oracle.move

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

    // Validate timestamps (basic check)
    let current_time = tx_context::epoch_timestamp_ms(ctx);
    let i = 0;
    let len = vector::length(&timestamps);
    while (i < len) {
        let ts = *vector::borrow(&timestamps, i);
        // Check timestamp is not too old (7 days) or in future
        assert!(ts <= current_time && ts >= current_time - 604800000, E_INVALID_TIMESTAMP);
        i = i + 1;
    };

    // TODO: Verify signatures (would need crypto::ed25519_verify)

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
    check_and_award_milestone(device, ctx);

    // Transfer record to sender (backend server)
    transfer::transfer(record, tx_context::sender(ctx));
}

/// Check if device reached a milestone and award NFT
fun check_and_award_milestone(
    device: &Device,
    ctx: &mut TxContext
) {
    let total = device.total_steps;
    let milestone_opt = if (total >= 100000) {
        option::some(string::utf8(b"100000_steps"))
    } else if (total >= 50000) {
        option::some(string::utf8(b"50000_steps"))
    } else if (total >= 10000) {
        option::some(string::utf8(b"10000_steps"))
    } else if (total >= 1000) {
        option::some(string::utf8(b"1000_steps"))
    } else {
        option::none()
    };

    if (option::is_some(&milestone_opt)) {
        let milestone = option::destroy_some(milestone_opt);

        // Emit milestone event
        event::emit(MilestoneAchieved {
            device_id: device.device_id,
            milestone,
            total_steps: total,
        });

        // Mint NFT (implementation in next task)
        // mint_achievement_nft(device.device_id, milestone, ctx);
    };
}

/// Get record details (view function)
public fun get_record_details(record: &StepDataRecord): (String, u64, u64, bool) {
    (record.device_id, record.step_count, record.timestamp, record.verified)
}
```

**Subtasks**:
- [ ] Implement submit_step_data
- [ ] Add validation logic
- [ ] Signature verification (optional on-chain)
- [ ] Update stats
- [ ] Milestone detection
- [ ] Event emission

**Test Cases**:
- [ ] Valid submission succeeds
- [ ] Invalid timestamp fails
- [ ] Inactive device fails
- [ ] Stats update correctly
- [ ] Events emitted correctly

---

### Task 1.4: Signature Verification (Optional On-Chain)
**Priority**: Medium
**Estimated Time**: 4 hours
**Dependencies**: Task 1.3

**Implementation**:
```move
use sui::ed25519;
use sui::hash;

/// Verify Ed25519 signature on-chain (gas-intensive)
fun verify_signature(
    public_key: &vector<u8>,
    message: &vector<u8>,
    signature: &vector<u8>
): bool {
    // Hash message
    let message_hash = hash::sha2_256(*message);

    // Verify signature
    ed25519::ed25519_verify(signature, public_key, &message_hash)
}

/// Verify all signatures in a batch
fun verify_batch_signatures(
    device: &Device,
    data: &vector<u8>,
    signatures: &vector<vector<u8>>
): bool {
    let i = 0;
    let len = vector::length(signatures);

    while (i < len) {
        let sig = vector::borrow(signatures, i);
        if (!verify_signature(&device.public_key, data, sig)) {
            return false
        };
        i = i + 1;
    };

    true
}
```

**Note**: On-chain signature verification is expensive. Consider verifying off-chain (in backend) and only storing verified data on-chain.

**Subtasks**:
- [ ] Implement verify_signature
- [ ] Implement verify_batch_signatures
- [ ] Add to submit_step_data
- [ ] Benchmark gas cost

**Test Cases**:
- [ ] Valid signature passes
- [ ] Invalid signature fails
- [ ] Gas cost is acceptable (< 0.01 SUI)

---

## Module 2: Achievement NFT System

### Task 2.1: NFT Minting Functions
**Priority**: Medium
**Estimated Time**: 4 hours
**Dependencies**: Task 1.3

**Implementation**:
```move
// File: sources/step_nft.move
module trust_oracle::step_nft {
    use sui::object::{Self, UID};
    use sui::tx_context::{Self, TxContext};
    use sui::transfer;
    use sui::url::{Self, Url};
    use std::string::{Self, String};

    /// Achievement NFT
    struct StepAchievementNFT has key, store {
        id: UID,
        device_id: String,
        milestone: String,
        steps: u64,
        achieved_at: u64,
        image_url: Url,
        description: String,
    }

    /// Mint achievement NFT for milestone
    public fun mint_achievement(
        device_id: String,
        milestone: String,
        steps: u64,
        ctx: &mut TxContext
    ): StepAchievementNFT {
        let image_url = generate_nft_image_url(&milestone);
        let description = generate_description(&milestone, steps);

        StepAchievementNFT {
            id: object::new(ctx),
            device_id,
            milestone,
            steps,
            achieved_at: tx_context::epoch_timestamp_ms(ctx),
            image_url,
            description,
        }
    }

    /// Generate NFT image URL based on milestone
    fun generate_nft_image_url(milestone: &String): Url {
        // In production, host images on IPFS or CDN
        let base_url = b"https://sui-watch.io/nft/";
        let mut url_bytes = base_url;
        vector::append(&mut url_bytes, *string::bytes(milestone));
        vector::append(&mut url_bytes, b".png");

        url::new_unsafe_from_bytes(url_bytes)
    }

    /// Generate NFT description
    fun generate_description(milestone: &String, steps: u64): String {
        // Simple description
        let desc = b"Congratulations on reaching ";
        vector::append(&mut desc, *string::bytes(milestone));
        vector::append(&mut desc, b" steps!");

        string::utf8(desc)
    }

    /// Transfer NFT to recipient
    public entry fun transfer_nft(
        nft: StepAchievementNFT,
        recipient: address
    ) {
        transfer::transfer(nft, recipient);
    }

    /// Burn NFT (if user wants to remove it)
    public entry fun burn_nft(
        nft: StepAchievementNFT
    ) {
        let StepAchievementNFT {
            id,
            device_id: _,
            milestone: _,
            steps: _,
            achieved_at: _,
            image_url: _,
            description: _,
        } = nft;

        object::delete(id);
    }
}
```

**Subtasks**:
- [ ] Define NFT struct
- [ ] Implement mint_achievement
- [ ] Generate image URLs
- [ ] Generate descriptions
- [ ] Transfer function
- [ ] Burn function

**Test Cases**:
- [ ] NFT mints successfully
- [ ] Image URL is valid
- [ ] Description is correct
- [ ] Transfer works
- [ ] Burn works

---

### Task 2.2: Integrate NFT with Oracle
**Priority**: Low
**Estimated Time**: 2 hours
**Dependencies**: Task 2.1

**Implementation**:
```move
// Add to trust_oracle.move
use trust_oracle::step_nft;

/// Award achievement NFT when milestone reached
fun award_milestone_nft(
    device_id: String,
    milestone: String,
    total_steps: u64,
    recipient: address,
    ctx: &mut TxContext
) {
    let nft = step_nft::mint_achievement(
        device_id,
        milestone,
        total_steps,
        ctx
    );

    // Transfer to device owner
    step_nft::transfer_nft(nft, recipient);
}

// Update check_and_award_milestone to mint NFT
```

**Subtasks**:
- [ ] Import step_nft module
- [ ] Call mint in milestone check
- [ ] Transfer to device owner

---

## Module 3: Query & Analytics

### Task 3.1: View Functions
**Priority**: Medium
**Estimated Time**: 3 hours
**Dependencies**: Task 1.3

**Implementation**:
```move
// Add to trust_oracle.move

/// Get global oracle statistics
public fun get_global_stats(registry: &OracleRegistry): (u64, u64, u64) {
    (
        registry.total_devices,
        registry.total_submissions,
        registry.total_steps_recorded
    )
}

/// Get device leaderboard position (would need TableVec for full leaderboard)
public fun get_device_rank(device: &Device): u64 {
    device.total_steps
}

/// Calculate average steps per submission
public fun get_average_steps_per_submission(device: &Device): u64 {
    if (device.total_submissions == 0) {
        0
    } else {
        device.total_steps / device.total_submissions
    }
}
```

**Subtasks**:
- [ ] Implement get_global_stats
- [ ] Implement get_device_rank
- [ ] Implement get_average_steps_per_submission
- [ ] Add more analytics functions

---

## Testing

### Task 4.1: Unit Tests
**Priority**: High
**Estimated Time**: 6 hours
**Dependencies**: All previous tasks

**Test File**:
```move
// File: tests/trust_oracle_tests.move
#[test_only]
module trust_oracle::trust_oracle_tests {
    use trust_oracle::trust_oracle::{Self, OracleRegistry, Device};
    use sui::test_scenario;
    use std::string;

    #[test]
    fun test_device_registration() {
        let admin = @0xAD;
        let mut scenario = test_scenario::begin(admin);

        // Initialize oracle
        {
            trust_oracle::init(test_scenario::ctx(&mut scenario));
        };

        // Register device
        test_scenario::next_tx(&mut scenario, admin);
        {
            let mut registry = test_scenario::take_shared<OracleRegistry>(&scenario);
            let device_id = b"test_device_001";
            let public_key = b"0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";

            trust_oracle::register_device(
                &mut registry,
                device_id,
                public_key,
                test_scenario::ctx(&mut scenario)
            );

            test_scenario::return_shared(registry);
        };

        // Verify device was created
        test_scenario::next_tx(&mut scenario, admin);
        {
            let device = test_scenario::take_from_sender<Device>(&scenario);
            let (steps, submissions, active) = trust_oracle::get_device_stats(&device);

            assert!(steps == 0, 0);
            assert!(submissions == 0, 1);
            assert!(active == true, 2);

            test_scenario::return_to_sender(&scenario, device);
        };

        test_scenario::end(scenario);
    }

    #[test]
    fun test_step_data_submission() {
        let admin = @0xAD;
        let mut scenario = test_scenario::begin(admin);

        // Setup (init + register device)
        setup_test_environment(&mut scenario, admin);

        // Submit step data
        test_scenario::next_tx(&mut scenario, admin);
        {
            let mut registry = test_scenario::take_shared<OracleRegistry>(&scenario);
            let mut device = test_scenario::take_from_sender<Device>(&scenario);

            let step_count = 450;
            let timestamps = vector[1735492800000];
            let signatures = vector[b"fake_signature"];

            trust_oracle::submit_step_data(
                &mut registry,
                &mut device,
                step_count,
                timestamps,
                signatures,
                test_scenario::ctx(&mut scenario)
            );

            // Verify stats updated
            let (steps, submissions, _) = trust_oracle::get_device_stats(&device);
            assert!(steps == 450, 0);
            assert!(submissions == 1, 1);

            test_scenario::return_shared(registry);
            test_scenario::return_to_sender(&scenario, device);
        };

        test_scenario::end(scenario);
    }

    #[test]
    #[expected_failure(abort_code = trust_oracle::E_INVALID_STEP_COUNT)]
    fun test_invalid_step_count() {
        let admin = @0xAD;
        let mut scenario = test_scenario::begin(admin);

        setup_test_environment(&mut scenario, admin);

        test_scenario::next_tx(&mut scenario, admin);
        {
            let mut registry = test_scenario::take_shared<OracleRegistry>(&scenario);
            let mut device = test_scenario::take_from_sender<Device>(&scenario);

            // Try to submit invalid step count
            trust_oracle::submit_step_data(
                &mut registry,
                &mut device,
                200000,  // Too many steps
                vector[1735492800000],
                vector[b"fake_signature"],
                test_scenario::ctx(&mut scenario)
            );

            test_scenario::return_shared(registry);
            test_scenario::return_to_sender(&scenario, device);
        };

        test_scenario::end(scenario);
    }

    fun setup_test_environment(scenario: &mut test_scenario::Scenario, admin: address) {
        // Init
        trust_oracle::init(test_scenario::ctx(scenario));

        // Register device
        test_scenario::next_tx(scenario, admin);
        {
            let mut registry = test_scenario::take_shared<OracleRegistry>(scenario);
            trust_oracle::register_device(
                &mut registry,
                b"test_device_001",
                b"0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef",
                test_scenario::ctx(scenario)
            );
            test_scenario::return_shared(registry);
        };
    }
}
```

**Subtasks**:
- [ ] Test device registration
- [ ] Test step data submission
- [ ] Test invalid inputs
- [ ] Test milestone detection
- [ ] Test NFT minting
- [ ] Test access control

**Run Tests**:
```bash
sui move test
```

---

## Deployment

### Task 5.1: Testnet Deployment
**Priority**: Critical
**Estimated Time**: 2 hours
**Dependencies**: Task 4.1

**Deployment Steps**:
```bash
# Build package
sui move build

# Deploy to testnet
sui client publish --gas-budget 100000000

# Save package ID
export SUI_PACKAGE_ID="0x..."

# Test deployment
sui client call \
  --package $SUI_PACKAGE_ID \
  --module trust_oracle \
  --function register_device \
  --args "test_device" "abcd..." \
  --gas-budget 10000000
```

**Subtasks**:
- [ ] Build contract
- [ ] Deploy to testnet
- [ ] Verify deployment
- [ ] Save package ID
- [ ] Update backend .env
- [ ] Test all functions

---

### Task 5.2: Mainnet Deployment (Future)
**Priority**: Low
**Estimated Time**: 2 hours
**Dependencies**: Task 5.1, Full Testing

**Checklist**:
- [ ] Security audit passed
- [ ] All tests passing
- [ ] Gas costs optimized
- [ ] Documentation complete
- [ ] Backend integration tested
- [ ] Backup plan ready

---

## Documentation

### Task 6.1: Contract Documentation
**Priority**: Medium
**Estimated Time**: 4 hours
**Dependencies**: All tasks

**README.md**:
```markdown
# SUI Watch Trust Oracle Smart Contract

## Overview
Move smart contract for Hardware Witness step data verification and storage.

## Functions

### Public Entry Functions

#### `register_device(registry, device_id, public_key, ctx)`
Register a new Hardware Witness device.

**Parameters**:
- `registry: &mut OracleRegistry` - Global registry object
- `device_id: vector<u8>` - Unique device identifier
- `public_key: vector<u8>` - Ed25519 public key (32 bytes)
- `ctx: &mut TxContext` - Transaction context

**Example**:
```bash
sui client call \
  --package $PACKAGE_ID \
  --module trust_oracle \
  --function register_device \
  --args $REGISTRY_ID "test_device" "0xabcd..." \
  --gas-budget 10000000
```

... (continue for all functions)

## Events

### `DeviceRegistered`
Emitted when a new device is registered.

Fields:
- `device_id: String`
- `public_key: vector<u8>`
- `timestamp: u64`

... (continue for all events)

## Testing

Run tests:
```bash
sui move test
```

## Deployment

Deploy to testnet:
```bash
sui client publish --gas-budget 100000000
```

## Gas Costs

Estimated gas costs:
- `register_device`: ~0.001 SUI
- `submit_step_data`: ~0.005 SUI
- `mint_achievement`: ~0.003 SUI
```

**Subtasks**:
- [ ] Write README
- [ ] Document all functions
- [ ] Document events
- [ ] Add examples
- [ ] Gas cost estimates

---

## File Structure After Completion

```
sui-watch-contracts/
├── Move.toml
├── sources/
│   ├── trust_oracle.move       # Main oracle contract
│   └── step_nft.move           # NFT system
├── tests/
│   └── trust_oracle_tests.move # Unit tests
├── README.md
└── .env
```

---

## Completion Checklist

- [ ] All modules implemented
- [ ] Unit tests passing
- [ ] Deployed to testnet
- [ ] Backend integration tested
- [ ] Documentation complete
- [ ] Gas costs acceptable
- [ ] Security considerations reviewed
- [ ] Ready for mainnet (after audit)
