# TitanKV

[![npm](https://img.shields.io/npm/v/titankv)](https://www.npmjs.com/package/titankv)
[![Node.js CI](https://github.com/jojibyte/titankv/actions/workflows/ci.yml/badge.svg)](https://github.com/jojibyte/titankv/actions)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)

**Ultra-fast in-process key-value store for Node.js**

- 🚀 **73x faster** than LevelDB
- ⚡ **1.98M reads/sec**, **1.65M writes/sec**
- 📦 Zero dependencies at runtime
- 🔧 Redis-like API (Lists, Sets, TTL, Atomic ops)
- 💾 Optional WAL persistence

## Installation

```bash
npm install titankv
```

## Quick Start

```javascript
const { TitanKV } = require('titankv');

// In-memory (fastest)
const db = new TitanKV();

// With persistence
const db = new TitanKV('./data', { sync: 'async' });

// Basic ops
db.put('user:1', 'Alice');
db.get('user:1');      // 'Alice'
db.has('user:1');      // true
db.del('user:1');      // true

// TTL (auto-expire)
db.put('session', 'token', 60000); // 60s TTL

// Atomic counters
db.incr('views');      // 1
db.incr('views', 10);  // 11
db.decr('views');      // 10

// Lists
db.lpush('queue', 'job1');
db.rpush('queue', 'job2');
db.lpop('queue');      // 'job1'
db.lrange('queue', 0, -1);

// Sets
db.sadd('tags', 'nodejs');
db.sismember('tags', 'nodejs'); // true
db.smembers('tags');   // ['nodejs']

// Queries
db.scan('user:');      // Prefix scan
db.range('a', 'z');    // Range query
db.countPrefix('user:');

// Batch (fastest for bulk)
db.putBatch([['a', '1'], ['b', '2']]);
db.getBatch(['a', 'b']); // ['1', '2']

// Stats
db.stats(); // { hits, misses, hitRate, totalOps }
```

## Benchmarks

Real tests against popular databases:

| Database | Write | Read | vs TitanKV |
|----------|-------|------|-----------|
| **TitanKV** | 1.65M/s | 1.98M/s | 1x |
| LevelDB | 22.7K/s | 29.6K/s | **73x slower** |
| SQLite | 354K/s | 256K/s | **4x slower** |
| Redis (net) | ~100K/s | ~100K/s | **~15x slower** |

Run benchmarks yourself:
```bash
npm run benchmark
```

## API

### Core
- `put(key, value, [ttlMs])` - Set a key
- `get(key)` - Get a value
- `del(key)` - Delete a key
- `has(key)` - Check if key exists
- `size()` - Get entry count
- `clear()` - Clear all entries

### Atomic
- `incr(key, [delta])` - Increment counter
- `decr(key, [delta])` - Decrement counter

### Lists
- `lpush(key, value)` / `rpush(key, value)`
- `lpop(key)` / `rpop(key)`
- `lrange(key, start, stop)`
- `llen(key)`

### Sets
- `sadd(key, member)` / `srem(key, member)`
- `sismember(key, member)`
- `smembers(key)` / `scard(key)`

### Queries
- `keys()` - All keys
- `scan(prefix)` - Prefix scan
- `range(start, end)` - Range query
- `countPrefix(prefix)` - Count by prefix

### Batch
- `putBatch(pairs)` - Bulk insert
- `getBatch(keys)` - Bulk get

### Persistence
- `flush()` - Force WAL flush
- `stats()` - Get statistics

## Why TitanKV?

| | TitanKV | Redis | LevelDB |
|---|---------|-------|---------|
| Network | ❌ None | TCP/TLS | ❌ None |
| Process | In-process | External | In-process |
| Serialization | None | Required | Required |
| Sync/Async | Sync | Async | Async |

## License

MIT © jojibyte
