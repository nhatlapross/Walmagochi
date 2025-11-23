/**
 * Test script to simulate ESP32 feeding pet
 * This will test the full flow: WebSocket -> Backend -> Blockchain
 */

import WebSocket from 'ws';

const DEVICE_ID = 'esp32_watch_001';
const WS_URL = 'ws://localhost:8080';

let ws;
let authenticated = false;

console.log('🧪 Starting Feed Test...\n');

// Connect to WebSocket
ws = new WebSocket(WS_URL);

ws.on('open', () => {
    console.log('✓ Connected to backend');

    // Step 1: Register (skip if already registered)
    console.log('\n📝 Step 1: Authenticating...');
    const authMessage = {
        type: 'authenticate',
        deviceId: DEVICE_ID,
        challenge: 'test',
        signature: 'test'
    };
    ws.send(JSON.stringify(authMessage));
});

ws.on('message', (data) => {
    const message = JSON.parse(data.toString());
    console.log('\n📨 Received:', message.type);

    switch (message.type) {
        case 'welcome':
            console.log('   Message:', message.message);
            break;

        case 'auth_response':
            if (message.success) {
                console.log('✓ Authenticated successfully');
                authenticated = true;

                // Step 2: Get pet data
                console.log('\n📝 Step 2: Getting pet data...');
                ws.send(JSON.stringify({ type: 'getPet', deviceId: DEVICE_ID }));
            } else {
                console.error('✗ Authentication failed:', message.error);
                process.exit(1);
            }
            break;

        case 'pet_data':
            if (message.success) {
                const pet = message.pet;
                console.log('✓ Pet data received:');
                console.log(`   Name: ${pet.pet_name}`);
                console.log(`   Food: ${pet.food}`);
                console.log(`   Energy: ${pet.energy}`);
                console.log(`   Pet Object ID: ${pet.pet_object_id || 'NOT SET'}`);

                if (pet.food > 0) {
                    // Step 3: Feed pet
                    console.log('\n📝 Step 3: Feeding pet...');
                    ws.send(JSON.stringify({
                        type: 'feedPet',
                        deviceId: DEVICE_ID
                    }));
                } else {
                    console.log('\n⚠️  Pet has no food! Cannot test feed.');
                    ws.close();
                }
            }
            break;

        case 'pet_fed':
            console.log('✓ Pet fed successfully!');
            if (message.pet) {
                console.log(`   New Food: ${message.pet.food}`);
                console.log(`   New Hunger: ${message.pet.hunger}`);
                console.log(`   New Happiness: ${message.pet.happiness}`);
                console.log(`   New XP: ${message.pet.experience}`);
            }

            if (message.txDigest) {
                console.log(`\n🎉 BLOCKCHAIN TRANSACTION CREATED!`);
                console.log(`   TX Digest: ${message.txDigest}`);
                console.log(`   View on explorer: https://testnet.suivision.xyz/txblock/${message.txDigest}`);
            } else {
                console.log('\n⚠️  No blockchain transaction (local only)');
            }

            console.log('\n✅ Test completed successfully!');
            ws.close();
            break;

        case 'pet_error':
            console.error('✗ Pet error:', message.error);
            ws.close();
            break;

        case 'error':
            console.error('✗ Error:', message.error);
            ws.close();
            break;
    }
});

ws.on('error', (error) => {
    console.error('❌ WebSocket error:', error.message);
    process.exit(1);
});

ws.on('close', () => {
    console.log('\n🔌 Connection closed');
    process.exit(0);
});

// Timeout after 30 seconds
setTimeout(() => {
    console.error('\n⏱️  Test timeout!');
    ws.close();
    process.exit(1);
}, 30000);
