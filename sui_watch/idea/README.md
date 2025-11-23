# SUI Watch - Hardware Witness Planning Documentation

## 📋 Tổng quan

Thư mục này chứa toàn bộ kế hoạch và tài liệu để nâng cấp **sui-watch** thành một **Hardware Witness / Trust Oracle** cho dữ liệu vật lý (step counter) trên Sui blockchain.

**Ý tưởng cốt lõi**: ESP32 không chỉ đếm bước chân, mà còn **ký điện tử** mỗi batch dữ liệu ngay tại phần cứng, biến thiết bị thành một "nhân chứng phần cứng" đáng tin cậy.

---

## 📁 Cấu trúc thư mục

```
idea/
├── README.md                      # File này - tổng quan
├── idea1.md                       # Ý tưởng gốc (đã có)
├── ARCHITECTURE.md                # Kiến trúc hệ thống chi tiết
├── IMPLEMENTATION_PLAN.md         # Timeline và phân chia giai đoạn
├── ESP32_TASKS.md                 # Tasks phát triển firmware
├── BACKEND_TASKS.md               # Tasks phát triển backend server
├── SMART_CONTRACT_TASKS.md        # Tasks phát triển smart contract
├── API_SPECIFICATION.md           # API documentation
└── DATA_STRUCTURES.md             # Data format specifications
```

---

## 🚀 Bắt đầu nhanh

### 1. Đọc theo thứ tự

Nếu bạn mới bắt đầu, đọc theo thứ tự sau:

1. **idea1.md** - Hiểu ý tưởng gốc
2. **ARCHITECTURE.md** - Hiểu kiến trúc tổng thể
3. **IMPLEMENTATION_PLAN.md** - Hiểu timeline và milestone
4. Chọn role của bạn:
   - **Embedded Developer** → ESP32_TASKS.md
   - **Backend Developer** → BACKEND_TASKS.md
   - **Blockchain Developer** → SMART_CONTRACT_TASKS.md

### 2. Tài liệu tham khảo

- **API_SPECIFICATION.md** - Khi cần tích hợp API
- **DATA_STRUCTURES.md** - Khi cần hiểu data format

---

## 🎯 Mục tiêu dự án

### Core Features

1. ⌚ **Watch Face** - Hiển thị thời gian thực (NTP sync)
2. 👟 **Step Counter với Hardware Signing** - Đếm bước + ký điện tử tại ESP32
3. 💰 **SUI Wallet** - Quản lý balance, transaction history
4. 🔐 **Trust Oracle** - Submit dữ liệu đã ký lên blockchain

### Điểm độc đáo

- **Hardware Witness**: Mỗi batch dữ liệu được ký bằng Ed25519 tại ESP32
- **Provenance Proof**: Chứng minh dữ liệu từ thiết bị thật, không phải giả lập
- **On-chain Verification**: Smart contract có thể verify signatures
- **Achievement NFTs**: Mint NFT khi đạt milestone (1k, 10k, 100k steps)

---

## 🏗️ Kiến trúc hệ thống

```
┌─────────────────────────────────────────────────────────────┐
│                  ESP32-S3 (Hardware Witness)                 │
│  - QMI8658 IMU sensor (step detection)                      │
│  - Ed25519 signing (MicroSui library)                       │
│  - LVGL UI (watch face, wallet, stats)                      │
│  - WiFi + NTP (time sync)                                   │
└────────────────────┬────────────────────────────────────────┘
                     │ HTTPS/JSON
                     ▼
┌─────────────────────────────────────────────────────────────┐
│              Backend Server (sui-watch-server)               │
│  - Express.js API                                            │
│  - Ed25519 signature verification                           │
│  - SQLite database                                           │
│  - Batch aggregator (daily at 2 AM)                         │
└────────────────────┬────────────────────────────────────────┘
                     │ Sui SDK
                     ▼
┌─────────────────────────────────────────────────────────────┐
│                  Sui Blockchain (Testnet)                    │
│  - Trust Oracle smart contract                              │
│  - Device registry                                           │
│  - Step data records                                         │
│  - Achievement NFTs                                          │
└─────────────────────────────────────────────────────────────┘
```

---

## 📊 Timeline

### Phase 1: Foundation (Week 1-2)
- ESP32: Time management, cryptographic module, hardware signing
- Backend: Project setup, device management, signature verification
- Blockchain: Smart contract development, testnet deployment

### Phase 2: Feature Development (Week 2-3)
- ESP32: Watch face UI, enhanced step counter, data queue
- Backend: Step data API, aggregator, database
- Blockchain: Oracle submission, on-chain verification

### Phase 3: Integration (Week 3-4)
- End-to-end testing
- Bug fixes
- Performance optimization
- Documentation

### Phase 4: Polish & Deploy (Week 4+)
- UI/UX improvements
- Security audit
- Production deployment

---

## 👥 Team Roles

### Embedded Developer
**Responsibilities**:
- ESP32 firmware development
- IMU sensor integration
- Cryptographic signing
- LVGL UI development

**Files to focus on**: ESP32_TASKS.md

---

### Backend Developer
**Responsibilities**:
- Node.js server development
- API design & implementation
- Database schema
- Signature verification
- Blockchain integration

**Files to focus on**: BACKEND_TASKS.md, API_SPECIFICATION.md

---

### Blockchain Developer
**Responsibilities**:
- Move smart contract development
- Testing & deployment
- Gas optimization
- Security audit

**Files to focus on**: SMART_CONTRACT_TASKS.md

---

## 🔧 Tech Stack

### ESP32 Side
- **Platform**: ESP32-S3 with PSRAM
- **Framework**: Arduino / ESP-IDF
- **UI**: LVGL 8.x + SquareLine Studio
- **Crypto**: MicroSui library (Ed25519)
- **Sensor**: QMI8658 (6-axis IMU)

### Backend Side
- **Runtime**: Node.js 18+
- **Framework**: Express.js
- **Database**: SQLite3
- **Blockchain SDK**: @mysten/sui
- **Crypto**: tweetnacl (Ed25519 verification)

### Blockchain Side
- **Network**: Sui Testnet → Mainnet
- **Language**: Move
- **Tools**: Sui CLI, Sui Explorer

---

## 📖 Key Concepts

### Hardware Witness
Thiết bị ESP32 đóng vai trò là "nhân chứng phần cứng", chứng minh rằng dữ liệu xuất phát từ cảm biến thật chứ không phải từ máy tính giả lập.

### Digital Signature Flow
```
1. ESP32 detects steps
2. Build payload: { deviceId, stepCount, timestamp, rawData }
3. Hash payload with SHA256
4. Sign hash with Ed25519 private key
5. Send { payload, signature } to backend
6. Backend verifies signature with public key
7. Store verified data
8. Submit to blockchain (daily batch)
```

### Trust Oracle
Smart contract trên Sui blockchain nhận và lưu trữ dữ liệu đã được xác thực, tạo thành "bằng chứng không thể chối cãi" (immutable proof) về hoạt động thể chất.

---

## 🔐 Security Considerations

### Key Management
- Private key stored in ESP32 NVS (encrypted)
- Never transmitted over network
- Public key registered with backend
- Device ID derived from public key hash

### Data Integrity
- Every submission cryptographically signed
- Backend verifies before accepting
- Optional on-chain verification (gas-intensive)
- Timestamps prevent replay attacks

### Threat Mitigation
- **Key extraction**: Use ESP32 secure boot + flash encryption
- **MITM attack**: HTTPS only, certificate pinning
- **Replay attack**: Timestamp + nonce validation
- **Sybil attack**: Device registration + rate limiting

---

## 🧪 Testing Strategy

### Unit Tests
- ESP32: Step detection accuracy
- Backend: Signature verification
- Blockchain: Contract functions

### Integration Tests
- End-to-end data flow
- Offline queue synchronization
- Blockchain submission

### Load Tests
- 100+ concurrent devices
- 1000+ submissions per day
- Database performance

---

## 📈 Success Metrics

### Technical
- Step detection accuracy: > 95%
- Signature generation: < 100ms
- Backend throughput: 100 req/sec
- Battery life: > 24 hours
- Blockchain confirmation: < 5 seconds

### Product
- 10+ pilot devices
- 1000+ on-chain submissions
- 0 security incidents
- 99% uptime

---

## 🚢 Deployment

### Testnet (Development)
```bash
# Backend
cd sui-watch-server
npm install
npm start

# Frontend (optional)
cd sui-watch-dashboard
npm install
npm run dev
```

### Mainnet (Production)
**Checklist before mainnet**:
- [ ] Security audit passed
- [ ] All tests passing
- [ ] Gas costs optimized
- [ ] Documentation complete
- [ ] Backup & recovery plan ready
- [ ] Monitoring setup

---

## 🤝 Contributing

### Git Workflow
1. Create feature branch: `git checkout -b feature/time-manager`
2. Implement following task file
3. Write tests
4. Create PR with description
5. Code review
6. Merge to main

### Coding Standards
- **ESP32**: Arduino style, comments in English
- **Backend**: ESLint + Prettier
- **Blockchain**: Move formatter
- **Docs**: Markdown with clear structure

---

## 📚 External Resources

### Documentation
- [Sui Documentation](https://docs.sui.io/)
- [Move Book](https://move-book.com/)
- [ESP32-S3 Datasheet](https://www.espressif.com/sites/default/files/documentation/esp32-s3_datasheet_en.pdf)
- [LVGL Documentation](https://docs.lvgl.io/)
- [Ed25519 Specification](https://ed25519.cr.yp.to/)

### Tools
- [SquareLine Studio](https://squareline.io/)
- [Sui Explorer](https://suiexplorer.com/)
- [PlatformIO](https://platformio.org/)

### Community
- [Sui Discord](https://discord.gg/sui)
- [GitHub Issues](https://github.com/[org]/sui-watch/issues)

---

## 📞 Contact

- **Technical Lead**: [Your Name]
- **Repository**: https://github.com/[org]/sui-watch
- **Email**: dev@sui-watch.io
- **Discord**: https://discord.gg/sui-watch

---

## 📄 License

MIT License - See LICENSE file

---

## 🎉 Acknowledgments

- **Sui Foundation** - Blockchain infrastructure
- **MicroSui Team** - Cryptographic library
- **LVGL Team** - UI framework
- **QST Corporation** - QMI8658 IMU sensor

---

**Last Updated**: 2024-01-30
**Version**: 1.0.0
**Status**: Planning Phase
