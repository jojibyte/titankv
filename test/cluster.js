const { TitanKV } = require('../lib');
const { spawn } = require('child_process');
const path = require('path');
const fs = require('fs');

const testDir = path.join(__dirname, 'test-cluster-data');
try { fs.rmSync(testDir, { recursive: true, force: true }); } catch {}

if (process.argv[2] === 'worker') {
    // Worker tries to open the same DB
    try {
        const db = new TitanKV(testDir);
        console.log('WORKER_OPENED'); // This should not happen
        setTimeout(() => db.close(), 500);
    } catch (e) {
        console.log('WORKER_ERROR: ' + e.message);
    }
    return;
}

console.log('╔═══════════════════════════════════════════════════════════╗');
console.log('║        TitanKV Phase 1: IPC File Locking test             ║');
console.log('╚═══════════════════════════════════════════════════════════╝\n');

// Master opens the DB first
const db = new TitanKV(testDir);
console.log('  \u2192 Master Process Opened DB natively');

const worker = spawn(process.execPath, [__filename, 'worker']);
let out = '';
worker.stdout.on('data', d => out += d);

worker.on('close', () => {
    db.close();
    
    if (out.includes('WORKER_ERROR: failed to acquire exclusive database lock')) {
        console.log('  \u2713 IPC File Locking Verified: Secondary process blocked successfully!');
    } else {
        console.error('  \u2717 IPC File Locking FAILED! Output:', out);
        process.exit(1);
    }
});
