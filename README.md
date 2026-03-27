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
- No external service dependency (in-process native addon)

## Installation

```bash
npm install titankv
```

Requires CMake and a C++ compiler (MSVC / GCC / Clang).
Prebuilt binaries are used when available; build tooling is required only when no matching prebuild exists.

## Quick Start

```js
const { TitanKV } = require("titankv");

// In-memory (fastest)
const dbMemory = new TitanKV();

// With persistence (IPC safe - blocks secondary instances in PM2/Cluster)
const dbPersistent = new TitanKV("./data", { sync: "sync" });
```

## What You Can Build with TitanKV

TitanKV is designed for high-throughput, low-latency in-process workloads. Common production use cases:

- **Session storage** for Express apps (replace external Redis for many deployments)
- **Next.js fetch/ISR cache** with optional disk persistence
- **API response cache** with TTL and prefix-based invalidation
- **Rate limiting and counters** using atomic `incr`/`decr`
- **Leaderboards and rankings** using sorted sets (`zadd`, `zrange`, `zrevrange`)
- **Task queues and workers** with list operations (`lpush`, `rpop`, etc.)
- **Feature flags/config cache** with instant local reads
- **Realtime in-process messaging** via Pub/Sub channels

For local validation, see the example app in `examples/frontend-test`.

For a full product and developer capability overview, see [WHAT_YOU_CAN_BUILD.md](WHAT_YOU_CAN_BUILD.md).

## Performance-First Usage Guide

If raw speed is the priority, follow this order:

1. Start in-memory: `new TitanKV()` for the lowest latency path.
2. Add persistence only when needed: `new TitanKV("./data", { sync: "sync" })`.
3. Use batch APIs (`putBatch`, `getBatch`) for write/read-heavy workflows.
4. Keep keys short and value payloads practical to reduce memory and WAL overhead.
5. Prefer prefix-oriented key design for fast scans and targeted exports.
6. Use TTL aggressively for cache data to keep hot datasets compact.
7. Keep `bloomFilter: true` (default) for faster negative lookups across many SSTables.
8. Track `db.stats()` (`writeAmplification`, `spaceAmplification`, `walBytes`) and run `npm run benchmark` before/after workload changes.

## Upgrade and Compatibility

- Migration playbook: [MIGRATION_GUIDE.md](MIGRATION_GUIDE.md)
- Runtime/storage compatibility and breakage notes: [COMPATIBILITY_MATRIX.md](COMPATIBILITY_MATRIX.md)

## Ecosystem Adapters (New!)

TitanKV comes with built-in ecosystem adapters to seamlessly replace external Redis instances and achieve zero-network-latency caching in popular frameworks.

### Express.js Session Store

Use TitanKV as your drop-in replacement for Redis in Express applications. It runs directly inside your Node.js process.

```js
const express = require("express");
const session = require("express-session");
const TitanKVSessionStore = require("titankv/lib/express-session")(session);

const app = express();

app.use(
  session({
    store: new TitanKVSessionStore({
      prefix: "session:", // optional
      dir: "./titan-sessions", // optional persistence
    }),
    secret: "my-secret",
    resave: false,
    saveUninitialized: false,
  }),
);
```

### Next.js App Router Cache Handler

Use TitanKV as a Custom Cache Handler for Next.js to cache `fetch` requests and ISR completely in-memory (or disk) without needing Vercel KV or external Redis.

Edit your `next.config.js`:

```js
module.exports = {
  cacheHandler: require.resolve("titankv/lib/nextjs-cache"),
  cacheMaxMemorySize: 0, // Disable default memory cache if needed
};
```

You can customize the cache handler behavior by providing environment variables (e.g., `TITAN_CACHE_DIR` for persistence).

## Core Operations

```js
db.put("user:1", "Alice");
db.get("user:1"); // 'Alice'
db.has("user:1"); // true
db.del("user:1"); // true
db.size(); // 0
db.clear();
```

## Async Operations

```js
await db.putAsync("user:2", "Bob");
const v = await db.getAsync("user:2"); // 'Bob'

await db.putBatchAsync([
  ["user:3", "Carol"],
  ["user:4", "Dave"],
]);
const batchAsync = await db.getBatchAsync(["user:3", "user:missing", "user:4"]);
// ['Carol', null, 'Dave']

const asyncKeys = await db.keysAsync();
const asyncScan = await db.scanAsync("user:");
const asyncRange = await db.rangeAsync("user:1", "user:9");
const asyncCount = await db.countPrefixAsync("user:");
```

## TTL & EXPIRE

```js
// Set with TTL at creation
db.put("session", "token", 60000); // expires in 60s

// Add TTL to existing key
db.expire("user:1", 30000); // true
db.ttl("user:1"); // remaining ms (or -1 no TTL, -2 missing)
db.persist("user:1"); // remove TTL
```

## Atomic Counters

```js
db.incr("views"); // 1
db.incr("views", 10); // 11
db.decr("views"); // 10
```

## Lists

```js
db.lpush("queue", "a", "b", "c"); // 3 (multi-value)
db.rpush("queue", "x", "y"); // 5
db.lpop("queue"); // 'c'
db.rpop("queue"); // 'y'
db.lrange("queue", 0, -1); // ['b', 'a', 'x']
db.llen("queue"); // 3
db.lindex("queue", 0); // 'b'
db.lset("queue", 0, "B"); // true
```

## Sets

```js
db.sadd("tags", "node", "fast", "kv"); // 3 (multi-member)
db.sismember("tags", "node"); // true
db.smembers("tags"); // ['node', 'fast', 'kv']
db.scard("tags"); // 3
db.srem("tags", "kv"); // 1 (removed count)

// Set operations
db.sadd("s1", "a", "b", "c");
db.sadd("s2", "b", "c", "d");
db.sunion("s1", "s2"); // ['a', 'b', 'c', 'd']
db.sinter("s1", "s2"); // ['b', 'c']
db.sdiff("s1", "s2"); // ['a']
```

## Hashes

```js
db.hset("user:1", "name", "joji"); // 1
db.hmset("user:1", { age: "25", city: "istanbul" });
db.hget("user:1", "name"); // 'joji'
db.hgetall("user:1"); // { name: 'joji', age: '25', city: 'istanbul' }
db.hdel("user:1", "city"); // 1
db.hexists("user:1", "name"); // true
db.hkeys("user:1"); // ['name', 'age']
db.hvals("user:1"); // ['joji', '25']
db.hlen("user:1"); // 2
db.hincrby("user:1", "age", 1); // 26
```

## Sorted Sets

```js
db.zadd("leaderboard", 100, "alice", 200, "bob", 50, "charlie"); // 3
db.zscore("leaderboard", "alice"); // 100
db.zrank("leaderboard", "charlie"); // 0 (lowest score)
db.zrange("leaderboard", 0, -1); // ['charlie', 'alice', 'bob']
db.zrange("leaderboard", 0, 1, { withScores: true });
// [{ member: 'charlie', score: 50 }, { member: 'alice', score: 100 }]

db.zrevrange("leaderboard", 0, 0); // ['bob'] (highest)
db.zrangebyscore("leaderboard", 50, 150); // ['charlie', 'alice']
db.zincrby("leaderboard", 500, "charlie"); // 550
db.zcount("leaderboard", 100, 300); // 2
db.zrem("leaderboard", "bob"); // 1
db.zcard("leaderboard"); // 2
```

## Queries & Pattern Matching

```js
db.keys(); // all keys (limit: 1000)
db.scan("user:"); // prefix scan → [[key, value], ...]
db.range("user:1", "user:5"); // range query
db.countPrefix("user:"); // count by prefix

// Glob pattern matching (like Redis KEYS command)
db.keysMatch("user:*"); // all user keys
db.keysMatch("post:?"); // single-char wildcard
db.keysMatch("*:admin"); // suffix match
```

## Cursor-Based Iteration

```js
// Manual cursor scan (for large datasets)
let { cursor, entries, done } = db.sscan("user:", 0, 100);
while (!done) {
  for (const [key, val] of entries) {
    /* process */
  }
  ({ cursor, entries, done } = db.sscan("user:", cursor, 100));
}

// Generator-based iteration
for (const [key, value] of db.iterate("user:", 100)) {
  console.log(key, value);
}
```

## Batch Operations

```js
db.putBatch([
  ["a", "1"],
  ["b", "2"],
  ["c", "3"],
]);
db.getBatch(["a", "b", "missing"]); // ['1', '2', null]
```

## Transactions (MULTI/EXEC)

```js
const tx = db.multi();
tx.put("tx:1", "val1");
tx.put("tx:2", "val2");
tx.incr("tx:counter");
tx.sadd("tx:set", "a", "b");
const results = tx.exec(); // [undefined, undefined, 1, 2]

// Discard all queued commands
const tx2 = db.multi();
tx2.put("x", "y");
tx2.discard(); // clears queue
```

## Pub/Sub

```js
// Subscribe to a channel
db.subscribe("chat:general", (message, channel) => {
  console.log(`[${channel}] ${message}`);
});

// Pattern subscribe (glob wildcards)
db.subscribe("chat:*", (message, channel) => {
  console.log(`Pattern match on ${channel}: ${message}`);
});

// Publish
db.publish("chat:general", "Hello!"); // returns listener count

// Unsubscribe
db.unsubscribe("chat:general");
```

## JSON Import / Export

```js
// Import object-style JSON { key: value, ... }
db.importJSON("./series.json", { prefix: "series:" });

// Import array-style JSON [{ id: ..., ... }, ...]
db.importJSON("./users.json", { prefix: "user:", idField: "id" });

// Streaming import (for large files)
await db.importJSONStream("./huge.json", { prefix: "data:", batchSize: 10000 });

// Export to file
db.exportJSON("./backup.json", { prefix: "series:", limit: 5000 });

// Export to object (no file)
const data = db.exportJSON(null, { prefix: "user:" });
```

## Persistence & WAL

```js
const db = new TitanKV("./mydb", {
  sync: "sync",
  recoverMode: "permissive",
  bloomFilter: true,
  autoCompact: true,
  compactMinOps: 2000,
  compactTombstoneRatio: 0.35,
  compactMinWalBytes: 4 * 1024 * 1024,
});

db.put("key", "value"); // auto-persisted via WAL
db.flush(); // force WAL flush
db.compact(); // rewrite WAL (remove dead entries)

await db.flushAsync(); // non-blocking flush path
await db.compactAsync(); // non-blocking WAL compaction

// Recover on restart
const db2 = new TitanKV("./mydb", { sync: "sync" });
db2.get("key"); // 'value'
```

Recovery mode options:

- `permissive` (default): replay valid WAL prefix and stop at corrupted tail
- `strict`: throw on checksum mismatch or malformed recovery input

Read path option:

- `bloomFilter` (default `true`): enables SSTable Bloom filters to reduce unnecessary disk probes on missing keys

Compaction policy options:

- `autoCompact` (default `false`): enables policy-driven automatic WAL compaction
- `compactMinOps` (default `2000`): minimum WAL operations before policy evaluation
- `compactTombstoneRatio` (default `0.35`): minimum delete ratio required to trigger compaction
- `compactMinWalBytes` (default `4MB`): minimum WAL size gate before compaction is allowed
- Auto compaction runs on a background engine thread; `db.close()` waits for in-flight compaction before releasing resources

## Lifecycle

```js
// Explicit cleanup — flushes WAL, closes file handles
db.close();
```

## Utility Commands

```js
db.exists("key"); // true (alias for has())
db.dbsize(); // 42 (alias for size())
db.rename("old", "new"); // 'OK' — preserves TTL
db.type("mylist"); // 'list' | 'string' | 'set' | 'hash' | 'zset' | 'none'
db.randomkey(); // 'some:random:key' or null
```

## Background Cleanup

```js
// Auto-purge expired keys every 5 seconds
const db = new TitanKV(null, { cleanupIntervalMs: 5000 });
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
//   compressionRatio: 0.73,
//   walBytes: 5242880,
//   logicalWriteBytes: 18000000,
//   physicalWriteBytes: 21600000,
//   compactionCount: 12,
//   autoCompactionCount: 7,
//   writeAmplification: 1.2,
//   spaceAmplification: 1.08
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
import { TitanKV, Transaction, ZMember, ScanResult } from "titankv";

const db = new TitanKV("./data");
db.zadd("scores", 100, "alice");
const top: ZMember[] = db.zrange("scores", 0, 9, {
  withScores: true,
}) as ZMember[];
```

## Why TitanKV?

|                 | TitanKV        | Redis         | better-sqlite3 |
| --------------- | -------------- | ------------- | -------------- |
| Architecture    | In-process     | Client-server | In-process     |
| Network         | None           | TCP/TLS       | None           |
| Serialization   | None           | Required      | Required       |
| Data Structures | Yes            | Yes           | SQL only       |
| Compression     | Zstd per-entry | No            | No             |
| TTL             | Yes            | Yes           | Manual         |
| Pub/Sub         | Yes            | Yes           | No             |
| Transactions    | Yes            | Yes           | Yes            |
| Persistence     | WAL            | RDB/AOF       | WAL            |

## License

MIT © [jojibyte](https://github.com/jojibyte)
