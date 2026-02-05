const { TitanKV } = require('../lib');
const { ClassicLevel } = require('classic-level');
const Database = require('better-sqlite3');
const Redis = require('ioredis');
const fs = require('fs');
const path = require('path');

// Upstash Redis - TLS connection
const REDIS_URL = 'rediss://default:ASpMAAIncDFhMDlhZjRhY2M5NjU0OTM1OWM3YjU5ZWQ0YzVjMWE5NnAxMTA4Mjg@romantic-aphid-10828.upstash.io:6379';

const REDIS_OPS = 1000; // less for network (upstash has limits)
const LOCAL_OPS = 100000;

async function cleanup() {
    const dirs = ['./bench-leveldb', './bench-sqlite'];
    for (const d of dirs) {
        try { fs.rmSync(d, { recursive: true, force: true }); } catch { }
    }
}

function formatOps(ops) {
    if (ops >= 1000000) return `${(ops / 1000000).toFixed(2)}M ops/sec`;
    if (ops >= 1000) return `${(ops / 1000).toFixed(2)}K ops/sec`;
    return `${ops.toFixed(0)} ops/sec`;
}

async function bench(name, fn, count) {
    const start = process.hrtime.bigint();
    await fn();
    const end = process.hrtime.bigint();
    const ms = Number(end - start) / 1e6;
    const opsPerSec = (count / ms) * 1000;
    console.log(`  ${name.padEnd(40)} ${formatOps(opsPerSec).padStart(15)}  (${ms.toFixed(0)}ms)`);
    return opsPerSec;
}

async function main() {
    console.log('╔═══════════════════════════════════════════════════════════════╗');
    console.log('║      TitanKV REAL Benchmark vs LevelDB, SQLite, Redis         ║');
    console.log('╚═══════════════════════════════════════════════════════════════╝\n');

    await cleanup();

    const results = {};

    // === TitanKV ===
    console.log('┌─────────────────────────────────────────────────────────────┐');
    console.log('│ TitanKV (in-process, C++ native)                            │');
    console.log('└─────────────────────────────────────────────────────────────┘');

    const titan = new TitanKV();

    results.titanWrite = await bench('Sequential Write', async () => {
        for (let i = 0; i < LOCAL_OPS; i++) titan.put(`key:${i}`, `value:${i}`);
    }, LOCAL_OPS);

    results.titanRead = await bench('Sequential Read', async () => {
        for (let i = 0; i < LOCAL_OPS; i++) titan.get(`key:${i}`);
    }, LOCAL_OPS);

    results.titanBatch = await bench('Batch Write (1000/batch)', async () => {
        for (let b = 0; b < LOCAL_OPS / 1000; b++) {
            const pairs = [];
            for (let i = 0; i < 1000; i++) pairs.push([`batch:${b}:${i}`, `v:${i}`]);
            titan.putBatch(pairs);
        }
    }, LOCAL_OPS);

    titan.clear();

    // === LevelDB ===
    console.log('\n┌─────────────────────────────────────────────────────────────┐');
    console.log('│ LevelDB (classic-level, C++ native)                         │');
    console.log('└─────────────────────────────────────────────────────────────┘');

    const level = new ClassicLevel('./bench-leveldb');
    await level.open();

    results.levelWrite = await bench('Sequential Write', async () => {
        for (let i = 0; i < LOCAL_OPS; i++) await level.put(`key:${i}`, `value:${i}`);
    }, LOCAL_OPS);

    results.levelRead = await bench('Sequential Read', async () => {
        for (let i = 0; i < LOCAL_OPS; i++) await level.get(`key:${i}`);
    }, LOCAL_OPS);

    results.levelBatch = await bench('Batch Write (1000/batch)', async () => {
        for (let b = 0; b < LOCAL_OPS / 1000; b++) {
            const ops = [];
            for (let i = 0; i < 1000; i++) ops.push({ type: 'put', key: `batch:${b}:${i}`, value: `v:${i}` });
            await level.batch(ops);
        }
    }, LOCAL_OPS);

    await level.close();

    // === SQLite ===
    console.log('\n┌─────────────────────────────────────────────────────────────┐');
    console.log('│ SQLite (better-sqlite3, synchronous)                        │');
    console.log('└─────────────────────────────────────────────────────────────┘');

    fs.mkdirSync('./bench-sqlite', { recursive: true });
    const sqlite = new Database('./bench-sqlite/test.db');
    sqlite.pragma('journal_mode = WAL');  // WAL mode for better perf
    sqlite.exec('CREATE TABLE IF NOT EXISTS kv (key TEXT PRIMARY KEY, value TEXT)');

    const insertStmt = sqlite.prepare('INSERT OR REPLACE INTO kv (key, value) VALUES (?, ?)');
    const selectStmt = sqlite.prepare('SELECT value FROM kv WHERE key = ?');

    // Use transaction for writes (otherwise SQLite is extremely slow)
    const insertMany = sqlite.transaction((items) => {
        for (const [k, v] of items) insertStmt.run(k, v);
    });

    results.sqliteBatchWrite = await bench('Batch Write (transaction)', async () => {
        const items = [];
        for (let i = 0; i < LOCAL_OPS; i++) items.push([`key:${i}`, `value:${i}`]);
        insertMany(items);
    }, LOCAL_OPS);

    results.sqliteRead = await bench('Sequential Read', async () => {
        for (let i = 0; i < LOCAL_OPS; i++) selectStmt.get(`key:${i}`);
    }, LOCAL_OPS);

    sqlite.close();

    // === Redis (Upstash) ===
    console.log('\n┌─────────────────────────────────────────────────────────────┐');
    console.log('│ Redis (Upstash, network - Frankfurt)                        │');
    console.log('└─────────────────────────────────────────────────────────────┘');

    let redis;
    try {
        redis = new Redis(REDIS_URL, {
            tls: { rejectUnauthorized: false },
            maxRetriesPerRequest: 3,
            connectTimeout: 10000
        });

        await redis.ping();
        console.log('  ✓ Connected to Upstash Redis\n');

        results.redisWrite = await bench(`Sequential Write (${REDIS_OPS} ops)`, async () => {
            for (let i = 0; i < REDIS_OPS; i++) await redis.set(`key:${i}`, `value:${i}`);
        }, REDIS_OPS);

        results.redisRead = await bench(`Sequential Read (${REDIS_OPS} ops)`, async () => {
            for (let i = 0; i < REDIS_OPS; i++) await redis.get(`key:${i}`);
        }, REDIS_OPS);

        results.redisPipeline = await bench(`Pipeline Write (${REDIS_OPS} ops)`, async () => {
            const pipeline = redis.pipeline();
            for (let i = 0; i < REDIS_OPS; i++) pipeline.set(`pipe:${i}`, `v:${i}`);
            await pipeline.exec();
        }, REDIS_OPS);

        // cleanup keys
        for (let i = 0; i < REDIS_OPS; i++) {
            await redis.del(`key:${i}`);
            await redis.del(`pipe:${i}`);
        }

        await redis.quit();
    } catch (err) {
        console.log(`  ⚠ Redis error: ${err.message}\n`);
    }

    // === Summary ===
    console.log('\n╔═══════════════════════════════════════════════════════════════╗');
    console.log('║                       RESULTS SUMMARY                          ║');
    console.log('╚═══════════════════════════════════════════════════════════════╝\n');

    console.log('┌──────────────────┬────────────────┬────────────────┬───────────┐');
    console.log('│ Database         │ Write          │ Read           │ vs Titan  │');
    console.log('├──────────────────┼────────────────┼────────────────┼───────────┤');
    console.log(`│ TitanKV          │ ${formatOps(results.titanWrite).padStart(14)} │ ${formatOps(results.titanRead).padStart(14)} │    1.0x   │`);

    if (results.levelWrite) {
        const ratio = (results.titanWrite / results.levelWrite).toFixed(0);
        console.log(`│ LevelDB          │ ${formatOps(results.levelWrite).padStart(14)} │ ${formatOps(results.levelRead).padStart(14)} │  ${ratio.padStart(5)}x   │`);
    }

    if (results.sqliteBatchWrite) {
        const ratio = (results.titanBatch / results.sqliteBatchWrite).toFixed(1);
        console.log(`│ SQLite (batch)   │ ${formatOps(results.sqliteBatchWrite).padStart(14)} │ ${formatOps(results.sqliteRead).padStart(14)} │  ${ratio.padStart(5)}x   │`);
    }

    if (results.redisWrite) {
        const ratio = (results.titanWrite / results.redisWrite).toFixed(0);
        console.log(`│ Redis (network)  │ ${formatOps(results.redisWrite).padStart(14)} │ ${formatOps(results.redisRead).padStart(14)} │  ${ratio.padStart(5)}x   │`);
    }

    console.log('└──────────────────┴────────────────┴────────────────┴───────────┘');

    console.log('\n✓ All benchmarks complete!');
    console.log('\n📊 Why TitanKV is faster:');
    console.log('   • Zero network latency (in-process)');
    console.log('   • No serialization overhead');
    console.log('   • Synchronous memory operations');
    console.log('   • Optimized FNV-1a hashing');

    await cleanup();
}

main().catch(console.error);
