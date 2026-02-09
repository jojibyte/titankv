# Changelog

All notable changes to this project will be documented in this file.

This project adheres to [Semantic Versioning](https://semver.org/).

## [2.0.0] - 2026-02-09

### Breaking Changes
- **Build system**: Switched from `node-gyp` to `cmake-js` for native compilation.
- **Compression**: All values now compressed with Zstd. WAL format changed (`.wal` → `.t`).
- **Stats API**: `stats()` now returns `{ totalOps, hits, misses, hitRate, keyCount, rawBytes, compressedBytes, compressionRatio }`.
- **Removed deps**: `better-sqlite3`, `classic-level`, `ioredis` removed — TitanKV is fully self-contained.

### Added
- **Zstd compression** — configurable level (1–22), average 10–20x space savings.
- **Hash operations** — `hset`, `hmset`, `hget`, `hgetall`, `hdel`, `hexists`, `hkeys`, `hvals`, `hlen`, `hincrby`.
- **Sorted Sets** — `zadd`, `zrem`, `zscore`, `zcard`, `zrank`, `zrange`, `zrevrange`, `zrangebyscore`, `zincrby`, `zcount`.
- **Pub/Sub** — `subscribe`, `unsubscribe`, `publish` with wildcard pattern matching.
- **MULTI/EXEC transactions** — `db.multi()` returns a `Transaction` with `exec()` / `discard()`.
- **EXPIRE/TTL commands** — `expire(key, ms)`, `ttl(key)`, `persist(key)` on existing keys.
- **KEYS glob matching** — `keysMatch(pattern)` with `*` and `?` wildcards.
- **Cursor-based scan** — `sscan(prefix, cursor, count)` and `iterate(prefix)` generator.
- **JSON import/export** — `importJSON`, `importJSONStream` (async chunked), `exportJSON`.
- **WAL compaction** — `compact()` rewrites WAL with re-compressed active entries.
- **Multi-value push** — `lpush(key, ...values)`, `rpush(key, ...values)`, `sadd(key, ...members)`.
- **List index ops** — `lindex(key, index)`, `lset(key, index, value)`.
- **TypeScript definitions** — full `.d.ts` with all interfaces.
- **Benchmark suite** — `npm run benchmark` for performance profiling.

### Changed
- Lists, Sets, Hashes, Sorted Sets implemented in JS layer for maximum flexibility.
- Dependencies pinned to stable versions (`^x.y.z` instead of `*`).

## [1.0.1] - 2026-02-05

### Fixed
- macOS build: removed try-catch for clang compatibility.
- CI: explicit node architecture for cross-platform prebuilds.

## [1.0.0] - 2026-02-05

### Added
- Initial release.
- Core KV: `put`, `get`, `del`, `has`, `size`, `clear`.
- Atomic: `incr`, `decr`.
- TTL: auto-expire keys with millisecond precision.
- Lists: `lpush`, `rpush`, `lpop`, `rpop`, `lrange`, `llen`.
- Sets: `sadd`, `srem`, `sismember`, `smembers`, `scard`.
- Queries: `keys`, `scan`, `range`, `countPrefix`.
- Batch: `putBatch`, `getBatch`.
- WAL persistence with sync/async modes.
- GitHub Actions CI/CD with prebuild support.
