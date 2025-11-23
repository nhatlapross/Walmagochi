# Trust Oracle - Deployment Information

## Deployment Date
**Date**: 2025-11-19
**Network**: Sui Testnet
**Epoch**: 923

---

## 📦 Package Information

**Package ID**: `0x53b6975e1e950a1fe3e9dd67b09eb1781b897b77c382ff60d102fbbc2d28fd99`

**Transaction Digest**: `9R8e35P6LGsRnHtLBGabfAHWTPAETRFbauQzX3YoTwKJ`

**Deployer Address**: `0x1d03494b8bc5cdf3d77e21c85cba89e9ce6afdc4a854e297170b54995e612408`

---

## 🔑 Important Object IDs

### OracleRegistry (Shared Object)
```
ID: 0x3f21ee2cbf9b70659f8d6c42a7f7aad9e315b11500830ab3e178aff95cc659ce
Owner: Shared
Version: 655544031
Type: trust_oracle::OracleRegistry
```

**Usage**: This is the global registry that must be passed to all functions like `register_device()` and `submit_step_data()`.

### UpgradeCap (Owned Object)
```
ID: 0x2341a89d6d3085b7da9decb9e6a30ce94f372b52d72d37f7af74d677744d5ea0
Owner: 0x1d03494b8bc5cdf3d77e21c85cba89e9ce6afdc4a854e297170b54995e612408
Version: 655544031
Type: 0x2::package::UpgradeCap
```

**Usage**: Required for upgrading the package in the future.

---

## 💰 Gas Cost

- **Storage Cost**: 23.7804 SUI
- **Computation Cost**: 0.001 SUI
- **Storage Rebate**: 0.00097812 SUI
- **Total Cost**: ~0.02380228 SUI

---

## 🔗 Explorer Links

### Package
https://testnet.suivision.xyz/package/0x53b6975e1e950a1fe3e9dd67b09eb1781b897b77c382ff60d102fbbc2d28fd99

### Transaction
https://testnet.suivision.xyz/txblock/9R8e35P6LGsRnHtLBGabfAHWTPAETRFbauQzX3YoTwKJ

### OracleRegistry Object
https://testnet.suivision.xyz/object/0x3f21ee2cbf9b70659f8d6c42a7f7aad9e315b11500830ab3e178aff95cc659ce

---

## 📋 Contract Functions

### 1. register_device
Register a new Hardware Witness device.

**Function Signature**:
```move
public entry fun register_device(
    registry: &mut OracleRegistry,
    device_id: vector<u8>,
    public_key: vector<u8>,
    ctx: &mut TxContext
)
```

**Example Call (sui CLI)**:
```bash
sui client call \
  --package 0x53b6975e1e950a1fe3e9dd67b09eb1781b897b77c382ff60d102fbbc2d28fd99 \
  --module trust_oracle \
  --function register_device \
  --args \
    0x3f21ee2cbf9b70659f8d6c42a7f7aad9e315b11500830ab3e178aff95cc659ce \
    "[97,49,98,50,99,51,100,52,101,53,102,54,103,55,104,56]" \
    "[1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,32]" \
  --gas-budget 10000000
```

### 2. submit_step_data
Submit step data from device (called by backend server).

**Function Signature**:
```move
public entry fun submit_step_data(
    registry: &mut OracleRegistry,
    device: &mut Device,
    step_count: u64,
    timestamps: vector<u64>,
    signatures: vector<vector<u8>>,
    ctx: &mut TxContext
)
```

**Example Call (sui CLI)**:
```bash
sui client call \
  --package 0x53b6975e1e950a1fe3e9dd67b09eb1781b897b77c382ff60d102fbbc2d28fd99 \
  --module trust_oracle \
  --function submit_step_data \
  --args \
    0x3f21ee2cbf9b70659f8d6c42a7f7aad9e315b11500830ab3e178aff95cc659ce \
    <DEVICE_OBJECT_ID> \
    450 \
    "[1735492800000,1735496400000,1735500000000]" \
    "[[18,52,86,120,...],[18,52,86,120,...]]" \
  --gas-budget 10000000
```

### 3. deactivate_device
Deactivate a device (only owner can call).

**Function Signature**:
```move
public entry fun deactivate_device(
    device: &mut Device,
    _ctx: &mut TxContext
)
```

---

## 🧪 Testing Commands

### View OracleRegistry
```bash
sui client object 0x3f21ee2cbf9b70659f8d6c42a7f7aad9e315b11500830ab3e178aff95cc659ce
```

### Get Registry Stats (via view function - future)
```bash
sui client call \
  --package 0x53b6975e1e950a1fe3e9dd67b09eb1781b897b77c382ff60d102fbbc2d28fd99 \
  --module trust_oracle \
  --function get_global_stats \
  --args 0x3f21ee2cbf9b70659f8d6c42a7f7aad9e315b11500830ab3e178aff95cc659ce
```

---

## 🔧 Backend Integration

### Environment Variables (.env)
```bash
SUI_NETWORK=testnet
SUI_PACKAGE_ID=0x53b6975e1e950a1fe3e9dd67b09eb1781b897b77c382ff60d102fbbc2d28fd99
SUI_REGISTRY_ID=0x3f21ee2cbf9b70659f8d6c42a7f7aad9e315b11500830ab3e178aff95cc659ce
SUI_MNEMONIC=your-wallet-mnemonic-here
```

### JavaScript/TypeScript SDK Example
```typescript
import { SuiClient } from '@mysten/sui/client';
import { Transaction } from '@mysten/sui/transactions';
import { Ed25519Keypair } from '@mysten/sui/keypairs/ed25519';

const client = new SuiClient({ url: 'https://fullnode.testnet.sui.io' });
const packageId = '0x53b6975e1e950a1fe3e9dd67b09eb1781b897b77c382ff60d102fbbc2d28fd99';
const registryId = '0x3f21ee2cbf9b70659f8d6c42a7f7aad9e315b11500830ab3e178aff95cc659ce';

// Register device
async function registerDevice(deviceId: string, publicKey: Uint8Array) {
    const tx = new Transaction();

    tx.moveCall({
        target: `${packageId}::trust_oracle::register_device`,
        arguments: [
            tx.object(registryId),
            tx.pure.vector('u8', Array.from(new TextEncoder().encode(deviceId))),
            tx.pure.vector('u8', Array.from(publicKey)),
        ],
    });

    const keypair = Ed25519Keypair.deriveKeypair(process.env.SUI_MNEMONIC!);
    const result = await client.signAndExecuteTransaction({
        signer: keypair,
        transaction: tx,
    });

    return result;
}

// Submit step data
async function submitStepData(
    deviceObjectId: string,
    stepCount: number,
    timestamps: bigint[],
    signatures: Uint8Array[]
) {
    const tx = new Transaction();

    tx.moveCall({
        target: `${packageId}::trust_oracle::submit_step_data`,
        arguments: [
            tx.object(registryId),
            tx.object(deviceObjectId),
            tx.pure.u64(stepCount),
            tx.pure.vector('u64', timestamps),
            tx.pure(
                bcs.vector(bcs.vector(bcs.u8())).serialize(signatures)
            ),
        ],
    });

    const keypair = Ed25519Keypair.deriveKeypair(process.env.SUI_MNEMONIC!);
    const result = await client.signAndExecuteTransaction({
        signer: keypair,
        transaction: tx,
    });

    return result;
}
```

---

## 📊 Initial State

**OracleRegistry Initial State**:
```json
{
  "total_devices": 0,
  "total_submissions": 0,
  "total_steps_recorded": 0
}
```

---

## 🚀 Next Steps

1. **Backend Development**:
   - Create Node.js server based on `/home/alvin/Esp32-s3/src/sui-watch/idea/BACKEND_TASKS.md`
   - Implement device registration API
   - Implement step data verification and submission
   - Setup scheduled batch submissions

2. **ESP32 Integration**:
   - Follow `/home/alvin/Esp32-s3/src/sui-watch/idea/ESP32_TASKS.md`
   - Implement Ed25519 signing for step data
   - Add HTTP client to communicate with backend
   - Test end-to-end flow

3. **Testing**:
   - Register test device from backend
   - Submit test step data
   - Verify on Sui Explorer
   - Monitor gas costs

4. **Documentation**:
   - API documentation for backend endpoints
   - Integration guide for ESP32
   - Deployment runbook for production

---

## ⚠️ Important Notes

1. **OracleRegistry is a Shared Object**: Multiple transactions can access it concurrently
2. **Device objects are Owned**: Each device object belongs to the creator (backend server wallet)
3. **Gas Optimization**: Consider batching multiple device submissions in one transaction
4. **Security**: Ensure backend server wallet's private key is securely stored
5. **Upgrades**: Keep the UpgradeCap safe for future contract upgrades

---

## 📝 Changelog

### v1.0.0 - 2025-11-19
- Initial deployment to Sui Testnet
- Core features implemented:
  - Device registration
  - Step data submission with validation
  - Milestone tracking (1k, 10k, 50k, 100k steps)
  - Global statistics
- Deployed successfully at epoch 923

---

**Maintained by**: sui-watch team
**Last Updated**: 2025-11-19
