# Changelog

All notable changes to this project will be documented in this file.

This project adheres to [Semantic Versioning](https://semver.org/).

## [2.1.0] - 2026-03-20

### Breaking Changes
- **WAL format v2**: TTL field now persisted per entry (+8 bytes per PUT record). Old WAL files from v2.0.0 are **incompatible** — run `compact()` before upgrading, or delete the WAL and re-import data.
- **`srem()` return type**: Now returns `number` (removed count) instead of `boolean`, matching Redis behavior.

### Fixed
- **Expired keys leaked memory**: `get()`/`has()`/`getBatch()` now lazily delete expired keys from storage instead of leaving them in memory forever.
- **`putPrecompressedBatch` stats drift**: Each entry now stores correct `raw_size` via `getDecompressedSize()`. Overwrites properly subtract old entry bytes.
- **`hitRate` calculation**: Changed from `hits/totalOps` to `hits/(hits+misses)` for accurate cache hit rate.
- **WAL compression level mismatch**: WAL now uses the same compression level as storage (configurable, default 3) instead of hardcoded level 15.
- **WAL did not persist TTL**: Keys with TTL now survive WAL recovery with correct expiration timestamps.
- **Thread-safety**: `get()`/`has()`/`getBatch()` upgraded to `unique_lock` for safe lazy deletion of expired keys.
- **`.gitignore` pattern**: `benchmark.js` → `/benchmark.js` so `test/benchmark.js` is no longer accidentally ignored.
- **Catch blocks**: All `catch {}` blocks now have descriptive comments explaining the tolerated error.

### Added
- **`close()` method**: Explicit cleanup — flushes WAL, closes file handles, clears subscriptions and TTL map. Available in both JS and C++ layers.
- **`iterate()` safety counter**: Generator now throws after 1,000,000 rounds to prevent infinite loops (NASA Rule 2).
- **CI: Node.js version matrix**: Tests now run against Node 18, 20, and 22.
- **CI: Prebuild verification**: Publish step now validates all platform prebuilds exist before publishing to npm.
- **TypeScript**: `close()` method added to type definitions. `srem` return type updated to `number`.

### Changed
- **CI macOS x64**: Changed from `macos-latest` (arm64 runner) to `macos-13` for reliable Intel builds.

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
