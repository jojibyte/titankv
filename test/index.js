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

    test('srem', db.srem('myset', 'a') === true);
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
    test('hlen after del', db.hlen('profile') === 2);
    test('hincrby', db.hincrby('profile', 'age', 1) === 26);

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

    // === Persistence ===
    section('Persistence & Recovery');

    const db1 = new TitanKV(testDir, { sync: 'sync' });
    db1.put('persist:1', 'value1');
    db1.put('persist:2', 'value2');
    db1.incr('persist:counter');
    db1.flush();

    const db2 = new TitanKV(testDir, { sync: 'sync' });
    test('recover put', db2.get('persist:1') === 'value1');
    test('recover incr', db2.get('persist:counter') === '1');

    // === Summary ===
    console.log(`\n\u2554${'═'.repeat(59)}\u2557`);
    console.log(`\u2551  Results: ${String(passed).padEnd(3)} passed, ${String(failed).padEnd(3)} failed${' '.repeat(34)}\u2551`);
    console.log(`\u255a${'═'.repeat(59)}\u255d`);

    if (failed > 0) process.exit(1);
}

runTests().catch(console.error);
