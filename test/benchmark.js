'use strict';

const { TitanKV } = require('../lib');
const fs = require('fs');
const path = require('path');

const ITERATIONS = 100000;
const BATCH_SIZE = 1000;

function formatNum(n) {
    return n.toString().replace(/\B(?=(\d{3})+(?!\d))/g, ',');
}

function formatBytes(b) {
    if (b < 1024) return b + ' B';
    if (b < 1024 * 1024) return (b / 1024).toFixed(1) + ' KB';
    return (b / (1024 * 1024)).toFixed(2) + ' MB';
}

function bench(label, fn) {
    const start = process.hrtime.bigint();
    const result = fn();
    const elapsed = Number(process.hrtime.bigint() - start) / 1e6;
    const opsPerSec = Math.round((result.ops / elapsed) * 1000);
    console.log(`  ${label.padEnd(28)} ${String(elapsed.toFixed(1) + 'ms').padStart(10)}  ${formatNum(opsPerSec).padStart(12)} ops/s`);
    return { elapsed, opsPerSec, ...result };
}

function runBenchmarks() {
    console.log('╔═══════════════════════════════════════════════════════════════════╗');
    console.log('║                   TitanKV Benchmark Suite                         ║');
    console.log('╚═══════════════════════════════════════════════════════════════════╝');
    console.log(`  Iterations: ${formatNum(ITERATIONS)} | Batch: ${formatNum(BATCH_SIZE)}\n`);

    const tmpDir = path.join(__dirname, 'bench-data');
    try { fs.rmSync(tmpDir, { recursive: true, force: true }); } catch {}

    // -- Sequential PUT --
    console.log('┌───────────────────────────────────────────────────────────────────┐');
    console.log('│ Sequential Write                                                 │');
    console.log('└───────────────────────────────────────────────────────────────────┘');

    const db = new TitanKV();

    bench('put (100K)', () => {
        for (let i = 0; i < ITERATIONS; i++) {
            db.put(`key:${i}`, `value-${i}-${'x'.repeat(100)}`);
        }
        return { ops: ITERATIONS };
    });

    // -- Sequential GET --
    console.log('\n┌───────────────────────────────────────────────────────────────────┐');
    console.log('│ Sequential Read                                                  │');
    console.log('└───────────────────────────────────────────────────────────────────┘');

    bench('get (100K)', () => {
        for (let i = 0; i < ITERATIONS; i++) {
            db.get(`key:${i}`);
        }
        return { ops: ITERATIONS };
    });

    bench('get miss (100K)', () => {
        for (let i = 0; i < ITERATIONS; i++) {
            db.get(`miss:${i}`);
        }
        return { ops: ITERATIONS };
    });

    // -- Batch --
    console.log('\n┌───────────────────────────────────────────────────────────────────┐');
    console.log('│ Batch Operations                                                 │');
    console.log('└───────────────────────────────────────────────────────────────────┘');

    const db2 = new TitanKV();
    bench('putBatch (100K, batch=1000)', () => {
        for (let i = 0; i < ITERATIONS; i += BATCH_SIZE) {
            const pairs = [];
            for (let j = i; j < i + BATCH_SIZE && j < ITERATIONS; j++) {
                pairs.push([`bk:${j}`, `bv-${j}-${'y'.repeat(100)}`]);
            }
            db2.putBatch(pairs);
        }
        return { ops: ITERATIONS };
    });

    bench('getBatch (100K, batch=1000)', () => {
        for (let i = 0; i < ITERATIONS; i += BATCH_SIZE) {
            const keys = [];
            for (let j = i; j < i + BATCH_SIZE && j < ITERATIONS; j++) {
                keys.push(`bk:${j}`);
            }
            db2.getBatch(keys);
        }
        return { ops: ITERATIONS };
    });

    // -- has / del / incr --
    console.log('\n┌───────────────────────────────────────────────────────────────────┐');
    console.log('│ Auxiliary Operations                                             │');
    console.log('└───────────────────────────────────────────────────────────────────┘');

    bench('has (100K)', () => {
        for (let i = 0; i < ITERATIONS; i++) {
            db.has(`key:${i}`);
        }
        return { ops: ITERATIONS };
    });

    bench('incr (100K)', () => {
        const db3 = new TitanKV();
        for (let i = 0; i < ITERATIONS; i++) {
            db3.incr('counter');
        }
        return { ops: ITERATIONS };
    });

    bench('del (100K)', () => {
        for (let i = 0; i < ITERATIONS; i++) {
            db.del(`key:${i}`);
        }
        return { ops: ITERATIONS };
    });

    // -- Data Structures --
    console.log('\n┌───────────────────────────────────────────────────────────────────┐');
    console.log('│ Data Structures (10K ops each)                                   │');
    console.log('└───────────────────────────────────────────────────────────────────┘');

    const STRUCT_OPS = 10000;
    const db4 = new TitanKV();

    bench('lpush (10K)', () => {
        for (let i = 0; i < STRUCT_OPS; i++) {
            db4.lpush('bench:list', `item-${i}`);
        }
        return { ops: STRUCT_OPS };
    });

    bench('lrange full (10K items)', () => {
        db4.lrange('bench:list', 0, -1);
        return { ops: 1 };
    });

    bench('sadd (10K)', () => {
        for (let i = 0; i < STRUCT_OPS; i++) {
            db4.sadd('bench:set', `member-${i}`);
        }
        return { ops: STRUCT_OPS };
    });

    bench('hset (10K)', () => {
        for (let i = 0; i < STRUCT_OPS; i++) {
            db4.hset('bench:hash', `field-${i}`, `val-${i}`);
        }
        return { ops: STRUCT_OPS };
    });

    bench('zadd (10K)', () => {
        for (let i = 0; i < STRUCT_OPS; i++) {
            db4.zadd('bench:zset', Math.random() * 10000, `player-${i}`);
        }
        return { ops: STRUCT_OPS };
    });

    bench('zrange (10K sorted set)', () => {
        db4.zrange('bench:zset', 0, -1);
        return { ops: 1 };
    });

    // -- Compression --
    console.log('\n┌───────────────────────────────────────────────────────────────────┐');
    console.log('│ Compression Stats                                                │');
    console.log('└───────────────────────────────────────────────────────────────────┘');

    const dbComp = new TitanKV();
    const sampleValue = JSON.stringify({
        id: 1,
        name: 'benchmark test entry',
        description: 'This is a test entry for benchmarking compression performance.',
        tags: ['performance', 'test', 'benchmark', 'titankv'],
        metadata: { created: Date.now(), version: '1.0.0', platform: process.platform },
    });

    const COMP_N = 50000;
    for (let i = 0; i < COMP_N; i++) {
        dbComp.put(`comp:${i}`, sampleValue);
    }

    const stats = dbComp.stats();
    const rawMB = stats.rawBytes / (1024 * 1024);
    const compMB = stats.compressedBytes / (1024 * 1024);
    const ratio = stats.rawBytes / stats.compressedBytes;

    console.log(`  Keys:              ${formatNum(stats.keyCount)}`);
    console.log(`  Raw size:          ${formatBytes(stats.rawBytes)}`);
    console.log(`  Compressed size:   ${formatBytes(stats.compressedBytes)}`);
    console.log(`  Compression ratio: ${ratio.toFixed(1)}x`);
    console.log(`  Space saved:       ${((1 - compMB / rawMB) * 100).toFixed(1)}%`);

    // -- Persistence --
    console.log('\n┌───────────────────────────────────────────────────────────────────┐');
    console.log('│ Persistence (Write + Recover)                                    │');
    console.log('└───────────────────────────────────────────────────────────────────┘');

    try { fs.rmSync(tmpDir, { recursive: true, force: true }); } catch {}
    const PERSIST_N = 10000;

    bench(`write ${formatNum(PERSIST_N)} + flush`, () => {
        const pdb = new TitanKV(tmpDir, { sync: 'sync' });
        for (let i = 0; i < PERSIST_N; i++) {
            pdb.put(`p:${i}`, sampleValue);
        }
        pdb.flush();
        return { ops: PERSIST_N };
    });

    bench(`recover ${formatNum(PERSIST_N)}`, () => {
        const pdb2 = new TitanKV(tmpDir, { sync: 'sync' });
        const val = pdb2.get('p:0');
        if (!val) throw new Error('Recovery failed');
        return { ops: PERSIST_N };
    });

    // WAL file size
    const walPath = path.join(tmpDir, 'titan.t');
    if (fs.existsSync(walPath)) {
        const walSize = fs.statSync(walPath).size;
        console.log(`  WAL file size:     ${formatBytes(walSize)}`);
    }

    // -- JSON Import --
    console.log('\n┌───────────────────────────────────────────────────────────────────┐');
    console.log('│ JSON Import                                                      │');
    console.log('└───────────────────────────────────────────────────────────────────┘');

    const jsonPath = path.join(__dirname, 'series.json');
    if (fs.existsSync(jsonPath)) {
        const fileSize = fs.statSync(jsonPath).size;
        console.log(`  File: series.json (${formatBytes(fileSize)})`);

        bench('importJSON (series.json)', () => {
            const jdb = new TitanKV();
            const count = jdb.importJSON(jsonPath, { prefix: 'series:' });
            return { ops: count };
        });
    } else {
        console.log('  series.json not found, skipping');
    }

    // -- Cleanup --
    try { fs.rmSync(tmpDir, { recursive: true, force: true }); } catch {}

    console.log('\n╔═══════════════════════════════════════════════════════════════════╗');
    console.log('║                       Benchmark Complete                          ║');
    console.log('╚═══════════════════════════════════════════════════════════════════╝\n');
}

runBenchmarks();
