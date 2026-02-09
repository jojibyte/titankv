# TitanKV

[![npm](https://img.shields.io/npm/v/titankv)](https://www.npmjs.com/package/titankv)
[![Node.js CI](https://github.com/jojibyte/titankv/actions/workflows/ci.yml/badge.svg)](https://github.com/jojibyte/titankv/actions)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)

**Ultra-fast in-process key-value store for Node.js with Redis-like API.**

- **832K reads/sec, 321K writes/sec, 3.1M has/sec** (single-threaded)
- C++ core with Zstd compression, NAPI binding
- Redis-compatible: Strings, Lists, Sets, Sorted Sets, Hashes
- Pub/Sub, Transactions (MULTI/EXEC), TTL, EXPIRE
- Optional WAL persistence with compaction
- JSON import/export, streaming support
- Zero runtime dependencies (native addon)

## Installation

```bash
npm install titankv
```

Requires CMake and a C++ compiler (MSVC / GCC / Clang).

## Quick Start

```js
const { TitanKV } = require('titankv');

// In-memory (fastest)
const db = new TitanKV();

// With persistence
const db = new TitanKV('./data', { sync: 'sync' });
```

## Core Operations

```js
db.put('user:1', 'Alice');
db.get('user:1');       // 'Alice'
db.has('user:1');       // true
db.del('user:1');       // true
db.size();              // 0
db.clear();
```

## TTL & EXPIRE

```js
// Set with TTL at creation
db.put('session', 'token', 60000);    // expires in 60s

// Add TTL to existing key
db.expire('user:1', 30000);           // true
db.ttl('user:1');                     // remaining ms (or -1 no TTL, -2 missing)
db.persist('user:1');                 // remove TTL
```

## Atomic Counters

```js
db.incr('views');         // 1
db.incr('views', 10);    // 11
db.decr('views');         // 10
```

## Lists

```js
db.lpush('queue', 'a', 'b', 'c');   // 3 (multi-value)
db.rpush('queue', 'x', 'y');        // 5
db.lpop('queue');                    // 'c'
db.rpop('queue');                    // 'y'
db.lrange('queue', 0, -1);          // ['b', 'a', 'x']
db.llen('queue');                    // 3
db.lindex('queue', 0);              // 'b'
db.lset('queue', 0, 'B');           // true
```

## Sets

```js
db.sadd('tags', 'node', 'fast', 'kv');   // 3 (multi-member)
db.sismember('tags', 'node');            // true
db.smembers('tags');                     // ['node', 'fast', 'kv']
db.scard('tags');                        // 3
db.srem('tags', 'kv');                   // true
```

## Hashes

```js
db.hset('user:1', 'name', 'joji');       // 1
db.hmset('user:1', { age: '25', city: 'istanbul' });
db.hget('user:1', 'name');               // 'joji'
db.hgetall('user:1');                    // { name: 'joji', age: '25', city: 'istanbul' }
db.hdel('user:1', 'city');               // 1
db.hexists('user:1', 'name');            // true
db.hkeys('user:1');                      // ['name', 'age']
db.hvals('user:1');                      // ['joji', '25']
db.hlen('user:1');                       // 2
db.hincrby('user:1', 'age', 1);         // 26
```

## Sorted Sets

```js
db.zadd('leaderboard', 100, 'alice', 200, 'bob', 50, 'charlie');   // 3
db.zscore('leaderboard', 'alice');                                  // 100
db.zrank('leaderboard', 'charlie');                                 // 0 (lowest score)
db.zrange('leaderboard', 0, -1);                                   // ['charlie', 'alice', 'bob']
db.zrange('leaderboard', 0, 1, { withScores: true });
// [{ member: 'charlie', score: 50 }, { member: 'alice', score: 100 }]

db.zrevrange('leaderboard', 0, 0);                                 // ['bob'] (highest)
db.zrangebyscore('leaderboard', 50, 150);                           // ['charlie', 'alice']
db.zincrby('leaderboard', 500, 'charlie');                          // 550
db.zcount('leaderboard', 100, 300);                                 // 2
db.zrem('leaderboard', 'bob');                                      // 1
db.zcard('leaderboard');                                            // 2
```

## Queries & Pattern Matching

```js
db.keys();                    // all keys (limit: 1000)
db.scan('user:');             // prefix scan → [[key, value], ...]
db.range('user:1', 'user:5'); // range query
db.countPrefix('user:');      // count by prefix

// Glob pattern matching (like Redis KEYS command)
db.keysMatch('user:*');       // all user keys
db.keysMatch('post:?');       // single-char wildcard
db.keysMatch('*:admin');      // suffix match
```

## Cursor-Based Iteration

```js
// Manual cursor scan (for large datasets)
let { cursor, entries, done } = db.sscan('user:', 0, 100);
while (!done) {
    for (const [key, val] of entries) { /* process */ }
    ({ cursor, entries, done } = db.sscan('user:', cursor, 100));
}

// Generator-based iteration
for (const [key, value] of db.iterate('user:', 100)) {
    console.log(key, value);
}
```

## Batch Operations

```js
db.putBatch([['a', '1'], ['b', '2'], ['c', '3']]);
db.getBatch(['a', 'b', 'missing']); // ['1', '2', null]
```

## Transactions (MULTI/EXEC)

```js
const tx = db.multi();
tx.put('tx:1', 'val1');
tx.put('tx:2', 'val2');
tx.incr('tx:counter');
tx.sadd('tx:set', 'a', 'b');
const results = tx.exec();   // [undefined, undefined, 1, 2]

// Discard all queued commands
const tx2 = db.multi();
tx2.put('x', 'y');
tx2.discard();                // clears queue
```

## Pub/Sub

```js
// Subscribe to a channel
db.subscribe('chat:general', (message, channel) => {
    console.log(`[${channel}] ${message}`);
});

// Pattern subscribe (glob wildcards)
db.subscribe('chat:*', (message, channel) => {
    console.log(`Pattern match on ${channel}: ${message}`);
});

// Publish
db.publish('chat:general', 'Hello!');   // returns listener count

// Unsubscribe
db.unsubscribe('chat:general');
```

## JSON Import / Export

```js
// Import object-style JSON { key: value, ... }
db.importJSON('./series.json', { prefix: 'series:' });

// Import array-style JSON [{ id: ..., ... }, ...]
db.importJSON('./users.json', { prefix: 'user:', idField: 'id' });

// Streaming import (for large files)
await db.importJSONStream('./huge.json', { prefix: 'data:', batchSize: 10000 });

// Export to file
db.exportJSON('./backup.json', { prefix: 'series:', limit: 5000 });

// Export to object (no file)
const data = db.exportJSON(null, { prefix: 'user:' });
```

## Persistence & WAL

```js
const db = new TitanKV('./mydb', { sync: 'sync' });

db.put('key', 'value');     // auto-persisted via WAL
db.flush();                 // force WAL flush
db.compact();               // rewrite WAL (remove dead entries)

// Recover on restart
const db2 = new TitanKV('./mydb', { sync: 'sync' });
db2.get('key');             // 'value'
```

## Statistics

```js
const stats = db.stats();
// {
//   totalOps: 1500,
//   hits: 1200,
//   misses: 300,
//   hitRate: 0.8,
//   keyCount: 50000,
//   rawBytes: 12158361,
//   compressedBytes: 8905420,
//   compressionRatio: 0.73
// }
```

## Benchmarks

```
Sequential Write (100K)         321,634 ops/s
Sequential Read (100K)          832,715 ops/s
Batch Write (100K)              400,908 ops/s
Batch Read (100K)             1,079,397 ops/s
Has Check (100K)              3,128,246 ops/s
Delete (100K)                 2,053,329 ops/s
Incr (100K)                     869,615 ops/s
JSON Import (21.5 MB)            10,803 ops/s
Recovery (10K entries)        1,982,514 ops/s
```

Run benchmarks:
```bash
npm run benchmark
```

## TypeScript

Full type definitions included out of the box:

```ts
import { TitanKV, Transaction, ZMember, ScanResult } from 'titankv';

const db = new TitanKV('./data');
db.zadd('scores', 100, 'alice');
const top: ZMember[] = db.zrange('scores', 0, 9, { withScores: true }) as ZMember[];
```

## Why TitanKV?

|                 | TitanKV         | Redis           | better-sqlite3  |
|-----------------|-----------------|-----------------|-----------------|
| Architecture    | In-process      | Client-server   | In-process      |
| Network         | None            | TCP/TLS         | None            |
| Serialization   | None            | Required        | Required        |
| Data Structures | Yes             | Yes             | SQL only        |
| Compression     | Zstd per-entry  | No              | No              |
| TTL             | Yes             | Yes             | Manual          |
| Pub/Sub         | Yes             | Yes             | No              |
| Transactions    | Yes             | Yes             | Yes             |
| Persistence     | WAL             | RDB/AOF         | WAL             |

## License

MIT © [jojibyte](https://github.com/jojibyte)

