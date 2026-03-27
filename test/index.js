const { TitanKV } = require('../lib');
const fs = require('fs');
const path = require('path');

async function runTests() {
    console.log('╔═══════════════════════════════════════════════════════════╗');
    console.log('║           TitanKV Test Suite v5 - All Features            ║');
    console.log('╚═══════════════════════════════════════════════════════════╝\n');

    const testDir = path.join(__dirname, 'test-data');
    try { fs.rmSync(testDir, { recursive: true, force: true }); } catch {}

    const db = new TitanKV();
    let passed = 0, failed = 0;

    function test(name, condition) {
        if (condition) { console.log(`  \u2713 ${name}`); passed++; }
        else { console.log(`  \u2717 ${name}`); failed++; }
    }

    function section(title) {
        console.log(`\n\u250C${'─'.repeat(57)}\u2510`);
        console.log(`\u2502 ${title.padEnd(56)}\u2502`);
        console.log(`\u2514${'─'.repeat(57)}\u2518`);
    }

    // === Core ===
    section('Core Operations');

    db.put('user:1', 'jojibyte');
    test('put/get', db.get('user:1') === 'jojibyte');
    test('has existing', db.has('user:1') === true);
    test('has missing', db.has('user:999') === false);
    test('get missing', db.get('user:999') === null);
    test('size', db.size() === 1);
    test('delete', db.del('user:1') === true);
    test('delete again', db.del('user:1') === false);

    // === Async ===
    section('Async Operations');

    db.clear();
    let asyncErr = null;
    let asyncVal = null;
    try {
        await db.putAsync('async:1', 'ok');
        asyncVal = await db.getAsync('async:1');
    } catch (err) {
        asyncErr = err;
    }
    test('putAsync does not throw', asyncErr === null);
    test('getAsync returns written value', asyncVal === 'ok');

    // === Atomic ===
    section('Atomic Operations');

    db.clear();
    test('incr new', db.incr('counter') === 1);
    test('incr existing', db.incr('counter') === 2);
    test('incr delta', db.incr('counter', 10) === 12);
    test('decr', db.decr('counter') === 11);
    test('decr delta', db.decr('counter', 5) === 6);

    // === Batch ===
    section('Batch Operations');

    db.clear();
    db.putBatch([['a', '1'], ['b', '2'], ['c', '3']]);
    test('batch put', db.size() === 3);
    const batch = db.getBatch(['a', 'b', 'missing', 'c']);
    test('batch get', batch[0] === '1' && batch[1] === '2' && batch[2] === null && batch[3] === '3');

    section('Async Batch Operations');

    db.clear();
    let asyncBatchErr = null;
    let asyncBatch = [];
    try {
        await db.putBatchAsync([['ab:1', '10'], ['ab:2', '20'], ['ab:3', '30']]);
        asyncBatch = await db.getBatchAsync(['ab:1', 'ab:2', 'ab:missing', 'ab:3']);
    } catch (err) {
        asyncBatchErr = err;
    }
    test('putBatchAsync does not throw', asyncBatchErr === null);
    test('getBatchAsync returns expected values', asyncBatch[0] === '10' && asyncBatch[1] === '20' && asyncBatch[2] === null && asyncBatch[3] === '30');

    // === Query ===
    section('Query Operations');

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

    section('Async Query Operations');

    let asyncQueryErr = null;
    let asyncKeys = [];
    let asyncScan = [];
    let asyncRange = [];
    let asyncCount = 0;
    try {
        asyncKeys = await db.keysAsync();
        asyncScan = await db.scanAsync('user:');
        asyncRange = await db.rangeAsync('user:1', 'user:2');
        asyncCount = await db.countPrefixAsync('user:');
    } catch (err) {
        asyncQueryErr = err;
    }

    test('async query methods do not throw', asyncQueryErr === null);
    test('keysAsync returns all keys', asyncKeys.length === 5);
    test('scanAsync prefix size', asyncScan.length === 3);
    test('rangeAsync query size', asyncRange.length === 2);
    test('countPrefixAsync result', asyncCount === 3);

    // === KEYS glob pattern ===
    section('KEYS Glob Pattern Matching');

    const userKeys = db.keysMatch('user:*');
    test('keysMatch user:*', userKeys.length === 3);
    const postKeys = db.keysMatch('post:*');
    test('keysMatch post:*', postKeys.length === 2);
    const allKeys = db.keysMatch('*');
    test('keysMatch * (all)', allKeys.length === 5);
    const single = db.keysMatch('user:1');
    test('keysMatch exact', single.length === 1 && single[0] === 'user:1');
    const q = db.keysMatch('?ser:?');
    test('keysMatch ? wildcard', q.length === 3);

    // === EXPIRE / TTL ===
    section('EXPIRE / TTL Commands');

    db.put('sess:1', 'data');
    test('ttl no-expire = -1', db.ttl('sess:1') === -1);
    test('ttl missing = -2', db.ttl('sess:nope') === -2);

    test('expire existing', db.expire('sess:1', 5000) === true);
    test('ttl after expire > 0', db.ttl('sess:1') > 0 && db.ttl('sess:1') <= 5000);

    test('persist', db.persist('sess:1') === true);
    test('ttl after persist = -1', db.ttl('sess:1') === -1);

    test('expire missing', db.expire('sess:nope', 1000) === false);

    // === Cursor-based scan ===
    section('Cursor-Based Scan');

    const scan1 = db.sscan('user:', 0, 2);
    test('sscan returns entries', scan1.entries.length === 2);
    test('sscan has cursor', typeof scan1.cursor === 'number');
    test('sscan not done', scan1.done === false);

    const scan2 = db.sscan('user:', scan1.cursor, 2);
    test('sscan page 2', scan2.entries.length === 1);
    test('sscan done', scan2.done === true);

    let iterCount = 0;
    for (const [k, v] of db.iterate('user:', 2)) { iterCount++; }
    test('iterate generator', iterCount === 3);

    // === MULTI/EXEC ===
    section('MULTI/EXEC Transactions');

    db.clear();
    const tx = db.multi();
    tx.put('tx:1', 'val1');
    tx.put('tx:2', 'val2');
    tx.incr('tx:counter');
    tx.incr('tx:counter', 5);
    test('tx queue length', tx.length === 4);

    const results = tx.exec();
    test('tx exec results', results.length === 4);
    test('tx data written', db.get('tx:1') === 'val1');
    test('tx incr result', db.get('tx:counter') === '6');

    const tx2 = db.multi();
    tx2.put('tx:discard', 'nope');
    tx2.discard();
    test('tx discard', tx2.length === 0);

    // Verify transaction return value structure
    const tx3 = db.multi();
    tx3.put('tx:check', 'ok');
    tx3.get('tx:check');
    tx3.incr('tx:inc');
    tx3.del('tx:check');
    const res3 = tx3.exec();

    test('tx returns correct structure',
        res3.length === 4 &&
        res3[0] === undefined &&
        res3[1] === 'ok' &&
        res3[2] === 1 &&
        res3[3] === true
    );

    // === Pub/Sub ===
    section('Pub/Sub');

    let received = null;
    const listener = (msg, ch) => { received = { msg, ch }; };
    db.subscribe('chat:general', listener);
    const count = db.publish('chat:general', 'hello world');
    test('publish returns count', count === 1);
    test('subscriber received', received !== null && received.msg === 'hello world');
    test('subscriber channel', received.ch === 'chat:general');

    let patternMsg = null;
    db.subscribe('chat:*', (msg, ch) => { patternMsg = { msg, ch }; });
    db.publish('chat:private', 'secret');
    test('pattern subscribe', patternMsg !== null && patternMsg.msg === 'secret');
    test('pattern channel', patternMsg.ch === 'chat:private');

    db.unsubscribe('chat:general', listener);
    received = null;
    db.publish('chat:general', 'after unsub');
    test('unsubscribe works', received === null);

    // === Lists ===
    section('List Operations (Redis-like)');

    db.clear();
    test('lpush', db.lpush('mylist', 'first') === 1);
    test('rpush', db.rpush('mylist', 'last') === 2);
    test('lpush 2', db.lpush('mylist', 'start') === 3);
    test('llen', db.llen('mylist') === 3);

    const list = db.lrange('mylist', 0, -1);
    test('lrange all', list.length === 3 && list[0] === 'start' && list[2] === 'last');
    test('lindex', db.lindex('mylist', 1) === 'first');
    test('lindex negative', db.lindex('mylist', -1) === 'last');
    test('lset', db.lset('mylist', 1, 'middle') === true);
    test('lset verify', db.lindex('mylist', 1) === 'middle');

    test('lpop', db.lpop('mylist') === 'start');
    test('rpop', db.rpop('mylist') === 'last');
    test('llen after pop', db.llen('mylist') === 1);

    // === Sets ===
    section('Set Operations (Redis-like)');

    test('sadd new', db.sadd('myset', 'a') === 1);
    test('sadd existing', db.sadd('myset', 'a') === 0);
    db.sadd('myset', 'b');
    db.sadd('myset', 'c');
    test('sadd multi', db.sadd('myset2', 'x', 'y', 'z') === 3);

    test('scard', db.scard('myset') === 3);
    test('sismember true', db.sismember('myset', 'a') === true);
    test('sismember false', db.sismember('myset', 'x') === false);

    const members = db.smembers('myset');
    test('smembers', members.length === 3);

    test('srem', db.srem('myset', 'a') === 1);
    test('scard after rem', db.scard('myset') === 2);

    // === Hashes ===
    section('Hash Operations (Redis-like)');

    test('hset new', db.hset('profile', 'name', 'joji') === 1);
    test('hset update', db.hset('profile', 'name', 'jojibyte') === 0);
    test('hget', db.hget('profile', 'name') === 'jojibyte');
    test('hget missing', db.hget('profile', 'nope') === null);

    db.hmset('profile', { age: '25', city: 'istanbul' });
    test('hmset + hgetall', db.hgetall('profile').city === 'istanbul');
    test('hlen', db.hlen('profile') === 3);
    test('hkeys', db.hkeys('profile').length === 3);
    test('hvals', db.hvals('profile').includes('istanbul'));
    test('hexists true', db.hexists('profile', 'name') === true);
    test('hexists false', db.hexists('profile', 'missing') === false);
    test('hdel', db.hdel('profile', 'city') === 1);
    test('hdel non-existent field', db.hdel('profile', 'non-existent') === 0);
    test('hdel multiple including non-existent', db.hdel('profile', 'name', 'missing') === 1);
    test('hlen after del', db.hlen('profile') === 1);
    test('hincrby', db.hincrby('profile', 'age', 1) === 26);

    // Resilience test for Hash
    // Inject corrupt JSON directly into DB using internal prefix
    db._db.put('\x00H:corrupt', '{badjson');
    const corruptRes = db.hgetall('corrupt');
    test('hgetall handles corrupt data', corruptRes !== null && Object.keys(corruptRes).length === 0);
    test('hget handles corrupt data', db.hget('corrupt', 'field') === null);

    // === Sorted Sets ===
    section('Sorted Set Operations (Redis-like)');

    test('zadd single', db.zadd('leaderboard', 100, 'alice') === 1);
    test('zadd multi', db.zadd('leaderboard', 200, 'bob', 50, 'charlie', 300, 'dave') === 3);
    test('zadd update (0 new)', db.zadd('leaderboard', 150, 'alice') === 0);

    test('zcard', db.zcard('leaderboard') === 4);
    test('zscore', db.zscore('leaderboard', 'alice') === 150);
    test('zscore missing', db.zscore('leaderboard', 'nobody') === null);
    test('zrank', db.zrank('leaderboard', 'charlie') === 0);
    test('zrank missing', db.zrank('leaderboard', 'nobody') === null);

    const top = db.zrange('leaderboard', 0, -1);
    test('zrange order', top[0] === 'charlie' && top[1] === 'alice' && top[2] === 'bob' && top[3] === 'dave');

    const topScores = db.zrange('leaderboard', 0, 1, { withScores: true });
    test('zrange withScores', topScores[0].member === 'charlie' && topScores[0].score === 50);

    const rev = db.zrevrange('leaderboard', 0, 1);
    test('zrevrange', rev[0] === 'dave' && rev[1] === 'bob');

    const byScore = db.zrangebyscore('leaderboard', 100, 250);
    test('zrangebyscore', byScore.length === 2);

    const byScoreInf = db.zrangebyscore('leaderboard', '-inf', '+inf');
    test('zrangebyscore -inf/+inf', byScoreInf.length === 4);

    test('zincrby', db.zincrby('leaderboard', 500, 'charlie') === 550);
    test('zincrby new', db.zincrby('leaderboard', 10, 'newbie') === 10);
    test('zcard after zincrby', db.zcard('leaderboard') === 5);

    test('zcount', db.zcount('leaderboard', 100, 300) === 3);

    test('zrem', db.zrem('leaderboard', 'newbie') === 1);
    test('zrem missing', db.zrem('leaderboard', 'nobody') === 0);
    test('zcard after zrem', db.zcard('leaderboard') === 4);

    // === JSON Import ===
    section('JSON Import/Export');

    const jdb = new TitanKV();
    const jsonPath = path.join(__dirname, 'series.json');
    if (fs.existsSync(jsonPath)) {
        const start = process.hrtime.bigint();
        const cnt = jdb.importJSON(jsonPath, { prefix: 'series:' });
        const elapsed = Number(process.hrtime.bigint() - start) / 1e6;
        test('import series.json', cnt > 0);
        console.log(`  \u2192 ${cnt} entries imported in ${elapsed.toFixed(1)}ms`);

        const first = jdb.get('series:tt35316779');
        test('read imported entry', first !== null);
        if (first) {
            const parsed = JSON.parse(first);
            test('parsed data correct', parsed.name === 'Salvador');
        }

        const exportOut = jdb.exportJSON(null, { prefix: 'series:', limit: 5 });
        test('export returns data', Object.keys(exportOut).length > 0);
    } else {
        console.log('  \u26a0 series.json not found, skipping import test');
    }

    // === TTL ===
    section('TTL (Time-To-Live)');

    db.put('session', 'token', 100);
    test('TTL before expire', db.get('session') === 'token');
    await new Promise(r => setTimeout(r, 150));
    test('TTL after expire', db.get('session') === null);

    // === Stats ===
    section('Statistics');

    const s = db.stats();
    test('stats.totalOps > 0', s.totalOps > 0);
    test('stats.hits > 0', s.hits > 0);
    test('stats.hitRate', s.hitRate >= 0 && s.hitRate <= 1);
    console.log(`  \u2192 ops: ${s.totalOps}, hits: ${s.hits}, misses: ${s.misses}, hitRate: ${(s.hitRate * 100).toFixed(1)}%`);

    // Test for stats drift
    const driftKey = 'drift:check';
    const valSize = 100;
    db.put(driftKey, 'A'.repeat(valSize));
    const s1 = db.stats();
    db.put(driftKey, 'A'.repeat(valSize)); // Overwrite with same size
    const s2 = db.stats();
    test('stats.rawBytes stable on overwrite', s2.rawBytes === s1.rawBytes);
    test('stats.walBytes is number', typeof s.walBytes === 'number');
    test('stats.writeAmplification is number', typeof s.writeAmplification === 'number');
    test('stats.spaceAmplification is number', typeof s.spaceAmplification === 'number');

    db.del(driftKey);
    const s3 = db.stats();
    test('stats.rawBytes reduces on delete', s3.rawBytes < s2.rawBytes);

    const ampDir = path.join(__dirname, 'amp-metrics-data');
    try { fs.rmSync(ampDir, { recursive: true, force: true }); } catch {}

    const ampDb = new TitanKV(ampDir, { sync: 'sync' });
    for (let i = 0; i < 24; i++) {
        ampDb.put(`amp:${i}`, 'payload-'.repeat(64));
    }
    for (let i = 0; i < 12; i++) {
        ampDb.del(`amp:${i}`);
    }
    ampDb.compact();
    const ampStats = ampDb.stats();

    test('compactionCount increments after compact', ampStats.compactionCount >= 1);
    test('logicalWriteBytes tracked', ampStats.logicalWriteBytes > 0);
    test('physicalWriteBytes tracked', ampStats.physicalWriteBytes > 0);
    test('writeAmplification finite', Number.isFinite(ampStats.writeAmplification));
    test('spaceAmplification finite', Number.isFinite(ampStats.spaceAmplification));
    ampDb.close();

    try { fs.rmSync(ampDir, { recursive: true, force: true }); } catch {}

    // === Persistence ===
    section('Persistence & Recovery');

    const db1 = new TitanKV(testDir, { sync: 'sync' });
    db1.put('persist:1', 'value1');
    db1.put('persist:2', 'value2');
    db1.incr('persist:counter');
    db1.flush();
    db1.close();

    const db2 = new TitanKV(testDir, { sync: 'sync' });
    test('recover put', db2.get('persist:1') === 'value1');
    test('recover incr', db2.get('persist:counter') === '1');

    let asyncPersistErr = null;
    try {
        await db2.flushAsync();
        await db2.compactAsync();
    } catch (err) {
        asyncPersistErr = err;
    }
    test('flushAsync/compactAsync do not throw', asyncPersistErr === null);

    db2.close();

    // === Resilience & Corruption ===
    section('Resilience & Corruption');

    const corruptKey = 'corrupt:list';
    // Manually inject invalid JSON with list prefix '\x00L:'
    db._db.put('\x00L:' + corruptKey, '{invalid_json');

    // _getList should catch the JSON.parse error and return []
    test('llen handles corrupt data', db.llen(corruptKey) === 0);
    const corruptList = db.lrange(corruptKey, 0, -1);
    test('lrange handles corrupt data', Array.isArray(corruptList) && corruptList.length === 0);

    // Ensure we can overwrite it
    db.rpush(corruptKey, 'recovered');
    test('can recover from corrupt data', db.llen(corruptKey) === 1 && db.lindex(corruptKey, 0) === 'recovered');

    // === v2.1.0 Bug Fixes ===
    section('v2.1.0 – hitRate Fix');

    const hrDb = new TitanKV();
    hrDb.put('hr:1', 'a');
    hrDb.put('hr:2', 'b');
    hrDb.put('hr:3', 'c');
    hrDb.get('hr:1');        // hit
    hrDb.get('hr:2');        // hit
    hrDb.get('hr:miss1');    // miss
    hrDb.get('hr:miss2');    // miss
    const hrStats = hrDb.stats();
    test('hitRate = hits/(hits+misses)', Math.abs(hrStats.hitRate - 0.5) < 0.01);
    test('hitRate not based on totalOps', hrStats.totalOps > (hrStats.hits + hrStats.misses));
    console.log(`  \u2192 hitRate: ${(hrStats.hitRate * 100).toFixed(1)}% (hits: ${hrStats.hits}, misses: ${hrStats.misses}, ops: ${hrStats.totalOps})`);

    section('v2.1.0 – srem Returns Number');

    const sremDb = new TitanKV();
    sremDb.sadd('sremtest', 'a', 'b', 'c', 'd');
    const sremResult = sremDb.srem('sremtest', 'a', 'b', 'missing');
    test('srem returns number (removed count)', sremResult === 2);
    test('srem type is number', typeof sremResult === 'number');
    const sremZero = sremDb.srem('sremtest', 'missing1', 'missing2');
    test('srem returns 0 for no matches', sremZero === 0);

    section('v2.1.0 – iterate Safety Counter');

    const iterDb = new TitanKV();
    iterDb.put('safe:1', 'a');
    iterDb.put('safe:2', 'b');
    let safeCount = 0;
    for (const [k, v] of iterDb.iterate('safe:', 1)) { safeCount++; }
    test('iterate works normally', safeCount === 2);

    section('v2.1.0 – Expired Key Lazy Delete');

    const expDb = new TitanKV();
    expDb.put('exp:1', 'alive');
    expDb.put('exp:2', 'short-lived', 50);
    expDb.put('exp:3', 'also-short', 50);
    test('before expire: size=3', expDb.size() === 3);
    await new Promise(r => setTimeout(r, 80));
    // Trigger lazy delete via get
    test('expired key returns null', expDb.get('exp:2') === null);
    test('expired key has=false', expDb.has('exp:3') === false);
    // After lazy delete, expired keys removed from storage
    test('after lazy delete: size=1', expDb.size() === 1);
    test('alive key still works', expDb.get('exp:1') === 'alive');

    section('v2.1.0 – Batch Overwrite Stats');

    const batchDb = new TitanKV();
    batchDb.putBatch([['bk:1', 'aaa'], ['bk:2', 'bbb']]);
    const batchS1 = batchDb.stats();
    batchDb.putBatch([['bk:1', 'xxx'], ['bk:2', 'yyy']]);
    const batchS2 = batchDb.stats();
    test('batch overwrite: rawBytes stable', batchS2.rawBytes === batchS1.rawBytes);
    batchDb.del('bk:1');
    const batchS3 = batchDb.stats();
    test('batch delete: rawBytes decreases', batchS3.rawBytes < batchS2.rawBytes);

    section('v2.1.0 – close() Method');

    const closeDb = new TitanKV();
    closeDb.put('cls:1', 'data');
    closeDb.subscribe('ch', () => {});
    test('close does not throw', (() => { try { closeDb.close(); return true; } catch { return false; } })());

    // === v2.2.0 New Commands ===
    section('v2.2.0 – Set Operations (sunion/sinter/sdiff)');

    const setDb = new TitanKV();
    setDb.sadd('setA', 'a', 'b', 'c', 'd');
    setDb.sadd('setB', 'c', 'd', 'e', 'f');
    setDb.sadd('setC', 'd', 'e', 'g');

    const union = setDb.sunion('setA', 'setB');
    test('sunion has all members', union.length === 6); // a,b,c,d,e,f
    test('sunion includes a', union.includes('a'));
    test('sunion includes f', union.includes('f'));

    const inter = setDb.sinter('setA', 'setB');
    test('sinter correct', inter.length === 2 && inter.includes('c') && inter.includes('d'));

    const inter3 = setDb.sinter('setA', 'setB', 'setC');
    test('sinter 3 sets', inter3.length === 1 && inter3[0] === 'd');

    const diff = setDb.sdiff('setA', 'setB');
    test('sdiff correct', diff.length === 2 && diff.includes('a') && diff.includes('b'));

    const diff3 = setDb.sdiff('setA', 'setB', 'setC');
    test('sdiff 3 sets', diff3.length === 2 && diff3.includes('a') && diff3.includes('b'));

    section('v2.2.0 – rename');

    const renDb = new TitanKV();
    renDb.put('old:key', 'hello');
    test('rename returns OK', renDb.rename('old:key', 'new:key') === 'OK');
    test('old key gone', renDb.get('old:key') === null);
    test('new key has value', renDb.get('new:key') === 'hello');

    // rename with TTL
    renDb.put('ttl:old', 'temp', 60000);
    renDb.rename('ttl:old', 'ttl:new');
    test('rename preserves value', renDb.get('ttl:new') === 'temp');

    // rename non-existent key throws
    let renameErr = false;
    try { renDb.rename('nonexistent', 'x'); } catch { renameErr = true; }
    test('rename missing key throws', renameErr);

    section('v2.2.0 – type');

    const typeDb = new TitanKV();
    typeDb.put('str:key', 'value');
    typeDb.lpush('list:key', 'a');
    typeDb.sadd('set:key', 'a');
    typeDb.hset('hash:key', 'f', 'v');
    typeDb.zadd('zset:key', 1, 'a');

    test('type string', typeDb.type('str:key') === 'string');
    test('type list', typeDb.type('list:key') === 'list');
    test('type set', typeDb.type('set:key') === 'set');
    test('type hash', typeDb.type('hash:key') === 'hash');
    test('type zset', typeDb.type('zset:key') === 'zset');
    test('type none', typeDb.type('missing') === 'none');

    section('v2.2.0 – randomkey / exists / dbsize');

    const miscDb = new TitanKV();
    miscDb.put('rk:1', 'a');
    miscDb.put('rk:2', 'b');
    miscDb.put('rk:3', 'c');

    test('exists true', miscDb.exists('rk:1') === true);
    test('exists false', miscDb.exists('missing') === false);
    test('dbsize', miscDb.dbsize() === 3);

    const rk = miscDb.randomkey();
    test('randomkey returns a key', rk !== null && rk.startsWith('rk:'));

    const emptyDb = new TitanKV();
    test('randomkey empty db', emptyDb.randomkey() === null);

    section('v2.2.0 – Background TTL Cleanup');

    const bgDb = new TitanKV(null, { cleanupIntervalMs: 50 });
    bgDb.put('bg:1', 'alive');
    bgDb.put('bg:2', 'short', 30);
    bgDb.expire('bg:2', 30);
    test('bg: before cleanup size', bgDb.size() === 2);
    await new Promise(r => setTimeout(r, 120));
    // background cleanup should have purged bg:2
    test('bg: after cleanup size', bgDb.size() <= 1);
    test('bg: alive key survives', bgDb.get('bg:1') === 'alive');
    bgDb.close();

    section('v3.0.0 – Spill to Disk (SSTable Read Path)');

    const spillDir = path.join(__dirname, 'spill-data');
    try { fs.rmSync(spillDir, { recursive: true, force: true }); } catch {}

    const spillDb = new TitanKV(spillDir, { sync: 'sync', maxMemoryBytes: 4096 });
    const spillVal = 'x'.repeat(256);
    const spillN = 80;

    for (let i = 0; i < spillN; i++) {
        spillDb.put(`spill:${i}`, spillVal);
    }

    test('spill size includes spilled keys', spillDb.size() === spillN);
    test('spill get from sstable', spillDb.get('spill:42') === spillVal);
    test('spill has from sstable', spillDb.has('spill:79') === true);
    test('spill scan prefix size', spillDb.scan('spill:').length === spillN);

    test('spill del existing', spillDb.del('spill:42') === true);
    test('spill del masks old sstable value', spillDb.get('spill:42') === null);
    test('spill size after del', spillDb.size() === spillN - 1);

    spillDb.close();
    try { fs.rmSync(spillDir, { recursive: true, force: true }); } catch {}

    section('v3.0.0 – Spill Restart Recovery');

    const spillRecoveryDir = path.join(__dirname, 'spill-recovery-data');
    try { fs.rmSync(spillRecoveryDir, { recursive: true, force: true }); } catch {}

    const spillDbA = new TitanKV(spillRecoveryDir, { sync: 'sync', maxMemoryBytes: 4096 });
    for (let i = 0; i < spillN; i++) {
        spillDbA.put(`spillr:${i}`, spillVal);
    }
    spillDbA.close();

    const walPath = path.join(spillRecoveryDir, 'titan.tkv');
    try { fs.rmSync(walPath, { force: true }); } catch {}

    const spillDbB = new TitanKV(spillRecoveryDir, { sync: 'sync', maxMemoryBytes: 4096 });
    test('spill restart get from sstable fallback', spillDbB.get('spillr:42') === spillVal);
    test('spill restart size from sstable fallback', spillDbB.size() === spillN);
    spillDbB.close();

    try { fs.rmSync(spillRecoveryDir, { recursive: true, force: true }); } catch {}

    section('v3.0.0 – Recovery Manifest & Corruption Modes');

    const integrityDir = path.join(__dirname, 'integrity-data');
    try { fs.rmSync(integrityDir, { recursive: true, force: true }); } catch {}

    const integrityA = new TitanKV(integrityDir, { sync: 'sync', maxMemoryBytes: 4096 });
    for (let i = 0; i < 48; i++) {
        integrityA.put(`manifest:${i}`, spillVal);
    }
    integrityA.put('mode:key', 'ok');
    integrityA.close();

    const manifestPath = path.join(integrityDir, 'titan.manifest');
    test('manifest file created', fs.existsSync(manifestPath));
    if (fs.existsSync(manifestPath)) {
        const manifestText = fs.readFileSync(manifestPath, 'utf8');
        test('manifest contains sstable entries', manifestText.includes('sst\t'));
    }

    const integrityWalPath = path.join(integrityDir, 'titan.tkv');
    fs.appendFileSync(integrityWalPath, Buffer.from([0xde, 0xad, 0xbe]));

    const permissiveDb = new TitanKV(integrityDir, { sync: 'sync', recoverMode: 'permissive' });
    test('permissive mode recovers valid prefix', permissiveDb.get('mode:key') === 'ok');
    permissiveDb.close();

    let strictError = false;
    try {
        const strictDb = new TitanKV(integrityDir, { sync: 'sync', recoverMode: 'strict' });
        strictDb.close();
    } catch {
        strictError = true;
    }
    test('strict mode rejects corrupted wal', strictError === true);

    try { fs.rmSync(integrityDir, { recursive: true, force: true }); } catch {}

    section('v3.0.0 – Bloom Filter Toggle');

    const bloomDir = path.join(__dirname, 'bloom-data');
    try { fs.rmSync(bloomDir, { recursive: true, force: true }); } catch {}

    const bloomOffDb = new TitanKV(bloomDir, { sync: 'sync', maxMemoryBytes: 4096, bloomFilter: false });
    for (let i = 0; i < 64; i++) {
        bloomOffDb.put(`bloom:${i}`, spillVal);
    }
    bloomOffDb.close();

    const bloomOffReadDb = new TitanKV(bloomDir, { sync: 'sync', bloomFilter: false });
    test('bloom off still reads existing key', bloomOffReadDb.get('bloom:42') === spillVal);
    test('bloom off missing key returns null', bloomOffReadDb.get('bloom:missing') === null);
    bloomOffReadDb.close();

    const bloomOnReadDb = new TitanKV(bloomDir, { sync: 'sync', bloomFilter: true });
    test('bloom on reads existing key', bloomOnReadDb.get('bloom:7') === spillVal);
    bloomOnReadDb.close();

    try { fs.rmSync(bloomDir, { recursive: true, force: true }); } catch {}

    section('v3.0.0 – Auto Compaction Policy');

    const compactDir = path.join(__dirname, 'auto-compact-data');
    try { fs.rmSync(compactDir, { recursive: true, force: true }); } catch {}

    const compactDb = new TitanKV(compactDir, {
        sync: 'sync',
        autoCompact: true,
        compactMinOps: 8,
        compactTombstoneRatio: 0.4,
        compactMinWalBytes: 1024,
    });

    const compactValue = 'v'.repeat(2048);
    for (let i = 0; i < 32; i++) {
        compactDb.put(`ac:${i}`, compactValue);
    }
    compactDb.flush();

    const compactWalPath = path.join(compactDir, 'titan.tkv');
    const walBefore = fs.existsSync(compactWalPath) ? fs.statSync(compactWalPath).size : 0;

    for (let i = 0; i < 24; i++) {
        compactDb.del(`ac:${i}`);
    }
    compactDb.flush();
    compactDb.close();

    const walAfter = fs.existsSync(compactWalPath) ? fs.statSync(compactWalPath).size : 0;
    test('auto compact shrinks wal after tombstone-heavy churn', walAfter > 0 && walAfter < walBefore);

    const compactReadDb = new TitanKV(compactDir, { sync: 'sync' });
    test('auto compact recovery keeps alive keys', compactReadDb.get('ac:31') === compactValue);
    test('auto compact recovery drops deleted keys', compactReadDb.get('ac:1') === null);
    compactReadDb.close();

    try { fs.rmSync(compactDir, { recursive: true, force: true }); } catch {}

    section('v3.0.0 – Compaction Interruption Recovery');

    const interruptionDir = path.join(__dirname, 'compaction-interruption-data');
    try { fs.rmSync(interruptionDir, { recursive: true, force: true }); } catch {}

    const interruptionDb = new TitanKV(interruptionDir, { sync: 'sync' });
    interruptionDb.put('int:key', 'stable');
    interruptionDb.flush();
    interruptionDb.close();

    const interruptionWalPath = path.join(interruptionDir, 'titan.tkv');
    const interruptionBakPath = interruptionWalPath + '.bak';
    const interruptionTmpPath = interruptionWalPath + '.tmp';

    fs.copyFileSync(interruptionWalPath, interruptionBakPath);
    fs.rmSync(interruptionWalPath, { force: true });

    const restoreDb = new TitanKV(interruptionDir, { sync: 'sync' });
    test('restores main wal from bak artifact', restoreDb.get('int:key') === 'stable');
    restoreDb.close();

    fs.copyFileSync(interruptionWalPath, interruptionTmpPath);
    const staleTempDb = new TitanKV(interruptionDir, { sync: 'sync' });
    staleTempDb.close();
    test('stale temp artifact cleaned on open', fs.existsSync(interruptionTmpPath) === false);

    try { fs.rmSync(interruptionDir, { recursive: true, force: true }); } catch {}

    // === Summary ===
    console.log(`\n\u2554${'═'.repeat(59)}\u2557`);
    console.log(`\u2551  Results: ${String(passed).padEnd(3)} passed, ${String(failed).padEnd(3)} failed${' '.repeat(34)}\u2551`);
    console.log(`\u255a${'═'.repeat(59)}\u255d`);

    if (failed > 0) process.exit(1);
}

runTests().catch(console.error);
