const { TitanKV } = require('../lib');
const fs = require('fs');
const path = require('path');

const DB_PATH = path.join(__dirname, 'bench_db');

// Cleanup
try { fs.rmSync(DB_PATH, { recursive: true, force: true }); } catch { }

console.log('Generating 30MB JSON Data...');
const bigObject = [];
// Generate repetitive data to simulate real-world JSON with high compression potential
const template = {
    id: "0000-0000-0000-0000",
    name: "TitanKV Benchmark Item",
    description: "This is a repetitive description to test Zstd compression capabilities on large JSON blobs.",
    tags: ["database", "fast", "compression", "nodejs", "cpp"],
    meta: {
        created: Date.now(),
        updated: Date.now(),
        version: 1,
        active: true
    },
    payload: "X".repeat(1000) // Compressible padding
};

// Target ~30MB
for (let i = 0; i < 20000; i++) {
    const item = { ...template, id: `item-${i}` };
    bigObject.push(item);
}

const jsonString = JSON.stringify(bigObject);
const jsonSizeMB = jsonString.length / 1024 / 1024;
console.log(`Original JSON Size: ${jsonSizeMB.toFixed(2)} MB`);

// Benchmark
console.log('\nRunning TitanKV Compression Benchmark...');
const db = new TitanKV(DB_PATH, { compressionLevel: 15 }); // High compression

const start = process.hrtime.bigint();
db.put('result', jsonString);
const end = process.hrtime.bigint();

const durationMs = Number(end - start) / 1e6;
console.log(`Write Time: ${durationMs.toFixed(2)} ms`);

// Check Disk Size
const walPath = path.join(DB_PATH, 'titan.t');
const stats = fs.statSync(walPath);
const diskSizeMB = stats.size / 1024 / 1024;

console.log(`\nDisk Size (.t): ${diskSizeMB.toFixed(2)} MB`);
console.log(`Compression Ratio: ${(jsonSizeMB / diskSizeMB).toFixed(2)}x`);
console.log(`Space Savings: ${(100 * (1 - diskSizeMB / jsonSizeMB)).toFixed(2)}%`);

if (diskSizeMB < 2.0) {
    console.log('\n✅ PASS: Extreme compression achieved (< 2MB)');
} else {
    console.log('\n⚠️ WARN: Compression target missed (Target < 2MB)');
}

// Read Verification
console.log('\nVerifying Read...');
const readStart = process.hrtime.bigint();
const readVal = db.get('result');
const readEnd = process.hrtime.bigint();

if (readVal === jsonString) {
    console.log(`Read Verified! (${(Number(readEnd - readStart) / 1e6).toFixed(2)} ms)`);
} else {
    console.error('❌ FAIL: Data corruption detected!');
    process.exit(1);
}
