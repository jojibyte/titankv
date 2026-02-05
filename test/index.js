const { TitanKV } = require('../lib');
const fs = require('fs');
const path = require('path');

async function runTests() {
    console.log('╔═══════════════════════════════════════════════════════════╗');
    console.log('║           TitanKV Test Suite v3 - Full Features           ║');
    console.log('╚═══════════════════════════════════════════════════════════╝\n');

    const testDir = path.join(__dirname, 'test-data');
    try { fs.rmSync(testDir, { recursive: true, force: true }); } catch { }

    const db = new TitanKV();
    let passed = 0, failed = 0;

    function test(name, condition) {
        if (condition) { console.log(`  ✓ ${name}`); passed++; }
        else { console.log(`  ✗ ${name}`); failed++; }
    }

    // === Core Ops ===
    console.log('\n┌─────────────────────────────────────────────────────────┐');
    console.log('│ Core Operations                                         │');
    console.log('└─────────────────────────────────────────────────────────┘');

    db.put('user:1', 'jojibyte');
    test('put/get', db.get('user:1') === 'jojibyte');
    test('has existing', db.has('user:1') === true);
    test('has missing', db.has('user:999') === false);
    test('get missing', db.get('user:999') === null);
    test('size', db.size() === 1);
    test('delete', db.del('user:1') === true);
    test('delete again', db.del('user:1') === false);

    // === Atomic ===
    console.log('\n┌─────────────────────────────────────────────────────────┐');
    console.log('│ Atomic Operations                                       │');
    console.log('└─────────────────────────────────────────────────────────┘');

    db.clear();
    test('incr new', db.incr('counter') === 1);
    test('incr existing', db.incr('counter') === 2);
    test('incr delta', db.incr('counter', 10) === 12);
    test('decr', db.decr('counter') === 11);
    test('decr delta', db.decr('counter', 5) === 6);

    // === Batch ===
    console.log('\n┌─────────────────────────────────────────────────────────┐');
    console.log('│ Batch Operations                                        │');
    console.log('└─────────────────────────────────────────────────────────┘');

    db.clear();
    db.putBatch([['a', '1'], ['b', '2'], ['c', '3']]);
    test('batch put', db.size() === 3);
    const batch = db.getBatch(['a', 'b', 'missing', 'c']);
    test('batch get', batch[0] === '1' && batch[1] === '2' && batch[2] === null && batch[3] === '3');

    // === Query ===
    console.log('\n┌─────────────────────────────────────────────────────────┐');
    console.log('│ Query Operations                                        │');
    console.log('└─────────────────────────────────────────────────────────┘');

    db.clear();
    db.put('user:1', 'alice');
    db.put('user:2', 'bob');
    db.put('user:3', 'charlie');
    db.put('post:1', 'hello');
    db.put('post:2', 'world');

    const scanned = db.scan('user:');
    test('scan prefix', scanned.length === 3);
    test('scan values', scanned.some(([k, v]) => k === 'user:1' && v === 'alice'));

    test('countPrefix', db.countPrefix('user:') === 3);
    test('countPrefix posts', db.countPrefix('post:') === 2);

    const ranged = db.range('user:1', 'user:2');
    test('range query', ranged.length === 2);

    test('keys', db.keys().length === 5);

    // === Lists ===
    console.log('\n┌─────────────────────────────────────────────────────────┐');
    console.log('│ List Operations (Redis-like)                            │');
    console.log('└─────────────────────────────────────────────────────────┘');

    test('lpush', db.lpush('mylist', 'first') === 1);
    test('rpush', db.rpush('mylist', 'last') === 2);
    test('lpush 2', db.lpush('mylist', 'start') === 3);
    test('llen', db.llen('mylist') === 3);

    const list = db.lrange('mylist', 0, -1);
    test('lrange all', list.length === 3 && list[0] === 'start' && list[2] === 'last');

    test('lpop', db.lpop('mylist') === 'start');
    test('rpop', db.rpop('mylist') === 'last');
    test('llen after pop', db.llen('mylist') === 1);

    // === Sets ===
    console.log('\n┌─────────────────────────────────────────────────────────┐');
    console.log('│ Set Operations (Redis-like)                             │');
    console.log('└─────────────────────────────────────────────────────────┘');

    test('sadd new', db.sadd('myset', 'a') === 1);
    test('sadd existing', db.sadd('myset', 'a') === 0);
    db.sadd('myset', 'b');
    db.sadd('myset', 'c');

    test('scard', db.scard('myset') === 3);
    test('sismember true', db.sismember('myset', 'a') === true);
    test('sismember false', db.sismember('myset', 'x') === false);

    const members = db.smembers('myset');
    test('smembers', members.length === 3);

    test('srem', db.srem('myset', 'a') === true);
    test('scard after rem', db.scard('myset') === 2);

    // === TTL ===
    console.log('\n┌─────────────────────────────────────────────────────────┐');
    console.log('│ TTL (Time-To-Live)                                      │');
    console.log('└─────────────────────────────────────────────────────────┘');

    db.put('session', 'token', 100);
    test('TTL before expire', db.get('session') === 'token');
    await new Promise(r => setTimeout(r, 150));
    test('TTL after expire', db.get('session') === null);

    // === Stats ===
    console.log('\n┌─────────────────────────────────────────────────────────┐');
    console.log('│ Statistics                                              │');
    console.log('└─────────────────────────────────────────────────────────┘');

    const stats = db.stats();
    test('stats.totalOps > 0', stats.totalOps > 0);
    test('stats.hits > 0', stats.hits > 0);
    test('stats.hitRate', stats.hitRate >= 0 && stats.hitRate <= 1);
    console.log(`  → ops: ${stats.totalOps}, hits: ${stats.hits}, misses: ${stats.misses}, hitRate: ${(stats.hitRate * 100).toFixed(1)}%`);

    // === Persistence ===
    console.log('\n┌─────────────────────────────────────────────────────────┐');
    console.log('│ Persistence & Recovery                                  │');
    console.log('└─────────────────────────────────────────────────────────┘');

    const db1 = new TitanKV(testDir, { sync: 'sync' });
    db1.put('persist:1', 'value1');
    db1.put('persist:2', 'value2');
    db1.incr('persist:counter');
    db1.flush();

    const db2 = new TitanKV(testDir, { sync: 'sync' });
    test('recover put', db2.get('persist:1') === 'value1');
    test('recover incr', db2.get('persist:counter') === '1');

    // Summary
    console.log('\n╔═══════════════════════════════════════════════════════════╗');
    console.log(`║  Results: ${passed} passed, ${failed} failed                              ║`);
    console.log('╚═══════════════════════════════════════════════════════════╝');

    if (failed > 0) process.exit(1);
}

runTests().catch(console.error);
