import Database from 'better-sqlite3';

const db = new Database('./database/pets.db');

console.log('🔧 Fixing pets database...\n');

// Update all pets with NULL resources
const result = db.prepare('UPDATE pets SET food = 5, energy = 5 WHERE food IS NULL OR energy IS NULL').run();

console.log(`✓ Updated ${result.changes} pet(s)\n`);

// Verify
const pets = db.prepare('SELECT id, device_id, pet_name, food, energy FROM pets').all();

console.log(`Verification - Found ${pets.length} pet(s):\n`);

pets.forEach(pet => {
    console.log(`Pet ID: ${pet.id}`);
    console.log(`  Device: ${pet.device_id}`);
    console.log(`  Name: ${pet.pet_name}`);
    console.log(`  Food: ${pet.food}`);
    console.log(`  Energy: ${pet.energy}`);
    console.log('');
});

db.close();

console.log('✅ Database fixed!');
