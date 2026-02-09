const { TitanKV } = require('titankv');

// 1. Initialize Database
const db = new TitanKV('./my-npm-db');

console.log('🚀 TitanKV (from NPM) Started!');

// 2. Basic Operations
console.log('\n--- Basic Operations ---');
db.put('user:1', 'Alice from NPM');
console.log('User added:', db.get('user:1'));

// 3. Atomic Counters
console.log('\n--- Atomic Counters ---');
db.put('counter', '0');
db.incr('counter');
console.log('Counter:', db.get('counter')); // 1

// 4. Persistence Check
console.log('\n--- Checking Persistence ---');
if (db.has('prev-run')) {
    console.log('Found data from previous run:', db.get('prev-run'));
} else {
    console.log('First run, saving timestamp...');
    db.put('prev-run', new Date().toISOString());
}
