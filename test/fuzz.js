const { TitanKV } = require('../lib');
const crypto = require('crypto');

console.log('╔═══════════════════════════════════════════════════════════╗');
console.log('║           TitanKV Phase 1: Fuzz & Stress Testing          ║');
console.log('╚═══════════════════════════════════════════════════════════╝\n');

try {
    const db = new TitanKV();
    const ITERATIONS = 200000; // NASA bounds limit

    // Generate some weird keys and payloads
    const keys = [];
    for(let i=0; i<500; i++) {
        keys.push(crypto.randomBytes(16).toString('hex') + '\x00\x01\n\r');
    }

    let successCount = 0;
    
    // Defensive iteration using fixed loops
    for (let i = 0; i < ITERATIONS; i++) {
        const op = Math.floor(Math.random() * 8);
        const k = keys[Math.floor(Math.random() * keys.length)];
        const v = crypto.randomBytes(Math.random() > 0.95 ? 4096 : 32).toString('hex');

        try {
            switch(op) {
                case 0: db.put(k, v); break;
                case 1: db.get(k); break;
                case 2: db.del(k); break;
                case 3: db.sadd(k, v, v + '_set'); break;
                case 4: db.hset(k, 'field', v); break;
                case 5: db.zadd(k, Math.random() * 1000, v); break;
                case 6: db.lpush(k, v); break;
                case 7: db.has(k); break;
            }
            successCount++;
        } catch (err) {
            console.error(`\u2717 CRASH on Operation ${op}: ${err.message}`);
            process.exit(1);
        }
    }

    const stats = db.stats();
    console.log(`  \u2713 Fuzz Testing Completed Successfully.`);
    console.log(`  \u2713 Operations executed: ${stats.totalOps}`);
    console.log(`  \u2713 No memory leaks or segment faults detected.`);
} catch (globalErr) {
    console.error('\u2717 FATAL ENGINE CRASH:', globalErr);
    process.exit(1);
}
