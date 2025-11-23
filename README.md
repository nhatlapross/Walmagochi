# Walmagotchi - Virtual Pet NFT on Sui Blockchain

A blockchain-powered virtual pet game running on ESP32-S3 smartwatch. Feed, play, and care for your Walrus pet while earning rewards through physical activity. All interactions are synchronized with Sui blockchain as NFT transactions.

![Walmagotchi](docs/walmagotchi-banner.png)

## Features

### 🐾 Virtual Pet System
- **5 Growth Stages**: Egg → Baby → Teen → Adult → Master
- **Pet Stats**: Happiness, Hunger, Health, and Experience
- **Interactive Actions**: Feed and Play with real-time animations
- **Automatic State Management**: Pet evolves based on care and activity

### 🏃 Step Counter Integration
- **QMI8658 IMU Sensor**: Accurate step detection with orientation awareness
- **Activity Rewards**: Earn food and energy from walking
  - 100 steps = 1 food
  - 150 steps = 2 energy
- **Claim System**: Convert steps to resources on-chain

### ⛓️ Blockchain Integration
- **Sui Network**: All pet interactions are NFT transactions
- **Real-time Sync**: Feed, play, and claim actions recorded on-chain
- **Wallet Integration**: View your SUI balance and pet NFT address
- **Trust Oracle**: Hardware-signed step data verification

### 🎨 User Interface
- **4 Interactive Screens**:
  1. **Pet Display**: View your pet's status and animations
  2. **Feed & Play**: Interact with your pet using resources
  3. **Steps & Claim**: Track activity and claim rewards
  4. **Wallet & Sync**: View blockchain status and balance
- **Loading Overlays**: Visual feedback for blockchain operations
- **Splash Screen**: "Walmagotchi" branding on startup

## Hardware Requirements

- **ESP32-S3** development board
- **1.28" Round LCD Display** (240x240, GC9A01 driver)
- **CST816S Touch Controller**
- **QMI8658 IMU Sensor** (accelerometer + gyroscope)
- **PSRAM**: Required for graphics buffer

## Software Stack

- **Arduino Framework** with ESP32 board support
- **LVGL v8.x**: Graphics library for UI
- **SquareLine Studio**: UI design tool
- **MicroSui Library**: Sui blockchain interaction
- **WebSocket Client**: Real-time connection to Trust Oracle backend
- **ArduinoJson**: JSON parsing and serialization

## Project Structure

```
Walmagochi/
├──--
│   ├── sui_watch/              # Main ESP32 firmware
│   │   ├── sui_watch.ino       # Main program
│   │   ├── VirtualPet.cpp/h    # Pet logic and state management
│   │   ├── TrustOracleClient.cpp/h  # Blockchain communication
│   │   ├── LoadingOverlay.cpp/h     # UI loading screens
│   │   ├── ui_handlers.cpp     # Button event handlers
│   │   ├── ui.c/h              # SquareLine Studio generated UI
│   │   └── QMI8658.cpp/h       # IMU sensor driver
│   │
│   └── ESP32S3_Squareline_UI/  # Reference implementation
│
├── trust-oracle-server/        # Backend server (Node.js)
│   ├── src/
│   │   ├── server.mjs          # WebSocket server
│   │   ├── petManager.mjs      # Database operations
│   │   └── suiBlockchain.mjs   # Blockchain transactions
│   ├── database/
│   │   └── pets.db             # SQLite database
│   └── package.json
│
├── sui-watch-contracts/        # Sui Move smart contracts
│   └── sources/
│       └── walrus_pet.move     # Pet NFT contract
│
└── docs/                       # Documentation
```

## Getting Started

### 1. Backend Setup

```bash
cd trust-oracle-server
npm install
cp .env.example .env
# Edit .env with your Sui wallet credentials
node src/server.mjs
```

The backend will:
- Start WebSocket server on port 8080
- Initialize SQLite database
- Connect to Sui testnet

### 2. ESP32 Configuration

Edit `src/sui_watch/sui_watch.ino`:

```cpp
// WiFi Configuration
char WIFI_SSID[33] = "YourWiFiSSID";
char WIFI_PASSWORD[65] = "YourWiFiPassword";

// Backend Server
const char* ORACLE_HOST = "192.168.1.11";  // Your server IP
const uint16_t ORACLE_PORT = 8080;

// Device Credentials
const char* DEVICE_ID = "esp32_watch_001";
const char* DEVICE_PRIVATE_KEY = "suiprivkey1...";  // Your device key
const char* DEVICE_WALLET_ADDRESS = "0x...";        // Your wallet
```

### 3. Upload Firmware

1. Install Arduino IDE with ESP32 board support
2. Install required libraries:
   - LVGL (v8.3.x)
   - ArduinoJson
   - WebSocketsClient
   - MicroSui
3. Select board: **ESP32S3 Dev Module**
4. Configure PSRAM: **OPI PSRAM**
5. Upload `src/sui_watch/sui_watch.ino`

### 4. Deploy Smart Contracts

```bash
cd sui-watch-contracts
sui client publish --gas-budget 100000000
```

Save the package ID and update backend `.env`:
```
SUI_PACKAGE_ID=0x...
```

## Usage

### Initial Setup
1. Power on the ESP32 watch
2. Watch connects to WiFi automatically
3. Device registers with backend server
4. Pet NFT is created on Sui blockchain

### Daily Care
1. **Feed Your Pet**: Tap Feed button (requires food resources)
2. **Play with Pet**: Tap Play button (requires energy resources)
3. **Earn Resources**: Walk to accumulate steps
4. **Claim Rewards**: Convert steps to food/energy on-chain

### Blockchain Interaction
- All actions (feed, play, claim) create Sui transactions
- Loading overlay shows during blockchain operations
- Transaction results displayed on screen
- View your pet NFT on Sui Explorer

## API Documentation

### WebSocket Messages

**Device → Server**:
```json
{
  "type": "authenticate",
  "deviceId": "esp32_watch_001"
}
```

```json
{
  "type": "feedPet",
  "deviceId": "esp32_watch_001"
}
```

**Server → Device**:
```json
{
  "type": "pet_fed",
  "success": true,
  "txDigest": "0x...",
  "food": 4,
  "happiness": 85
}
```

### REST Endpoints

- `GET /health` - Server health check
- `GET /devices/:deviceId/pet` - Get pet data
- `POST /devices/:deviceId/sync` - Sync pet state

## Blockchain Architecture

### Pet NFT Structure
```move
struct WalrusPet has key, store {
    id: UID,
    name: String,
    level: u8,
    happiness: u8,
    hunger: u8,
    health: u8,
    experience: u32,
    food: u8,
    energy: u8,
    created_at: u64
}
```

### On-Chain Actions
- **Feed**: Uses 1 food, increases happiness, adds XP
- **Play**: Uses 1 energy, increases happiness, adds XP
- **Claim**: Converts steps to food and energy resources
- **Level Up**: Automatic when XP threshold reached

## Troubleshooting

### Common Issues

**WiFi Connection Failed**
- Check SSID and password
- Ensure 2.4GHz WiFi (ESP32 doesn't support 5GHz)
- Verify router allows device connections

**Backend Connection Failed**
- Verify backend server is running
- Check IP address and port in firmware
- Ensure firewall allows port 8080

**Blockchain Transaction Failed**
- Check wallet has sufficient SUI balance
- Verify RPC URL is correct
- Ensure smart contracts are deployed

**Pet Not Responding**
- Check if loading overlay is showing (wait for completion)
- Verify pet has required resources (food/energy)
- Check Serial Monitor for error messages

### Debug Mode

Enable verbose logging in `sui_watch.ino`:
```cpp
// Set in setup()
Serial.setDebugOutput(true);
```

View logs:
```bash
# Arduino IDE: Tools → Serial Monitor (115200 baud)
# Or use platformio
pio device monitor -b 115200
```

## Development

### Building from Source

```bash
# Clone repository
git clone https://github.com/nhatlapross/walmagotchi.git
cd walmagotchi

# Install backend dependencies
cd trust-oracle-server
npm install

# Build contracts
cd ../sui-watch-contracts
sui move build
```

### Testing

**Backend Tests**:
```bash
cd trust-oracle-server
node test-feed.mjs
node check-db.mjs
```

**Hardware Tests**:
- IMU: Shake device and check Serial Monitor for step detection
- Touch: Tap screen to verify touch response
- Display: Check for artifacts or flickering

## Contributing

Contributions are welcome! Please:

1. Fork the repository
2. Create a feature branch
3. Make your changes
4. Add tests if applicable
5. Submit a pull request

## License

MIT License - See LICENSE file for details

## Acknowledgments

- **Sui Foundation** - Blockchain infrastructure
- **Waveshare** - ESP32-S3 hardware
- **LVGL** - Graphics library
- **SquareLine Studio** - UI design tool

## Support

- **Issues**: https://github.com/nhatlapross/walmagotchi/issues
- **Discord**: [Join our community]
- **Docs**: [Full documentation]

## Roadmap

- [ ] Multi-pet support
- [ ] Pet breeding system
- [ ] Marketplace for trading pets
- [ ] Leaderboard for most active users
- [ ] Mini-games for earning rewards
- [ ] Social features (visit friends' pets)

---

Built with ❤️ for the Sui ecosystem
