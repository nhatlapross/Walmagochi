import Database from 'better-sqlite3';

const db = new Database('./database/pets.db');

console.log('📊 Checking pets database...\n');

const pets = db.prepare('SELECT id, device_id, pet_name, food, energy, pet_object_id FROM pets').all();

console.log(`Found ${pets.length} pet(s):\n`);

pets.forEach(pet => {
    console.log(`Pet ID: ${pet.id}`);
    console.log(`  Device: ${pet.device_id}`);
    console.log(`  Name: ${pet.pet_name}`);
    console.log(`  Food: ${pet.food} (type: ${typeof pet.food})`);
    console.log(`  Energy: ${pet.energy} (type: ${typeof pet.energy})`);
    console.log(`  Object ID: ${pet.pet_object_id || 'NOT SET'}`);
    console.log('');
});

db.close();
