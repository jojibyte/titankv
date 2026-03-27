# Changelog

All notable changes to this project will be documented in this file.

This project adheres to [Semantic Versioning](https://semver.org/).

## [Unreleased]

## [3.0.0] - 2026-03-27

### Added

- **Frontend test example app**: Added `examples/frontend-test` with a lightweight HTTP API and browser UI to manually test put/get/delete/list flows.
- **README capability guide**: Added a "What You Can Build with TitanKV" section and a performance-first usage guide.
- **Recovery manifest foundation (`titan.manifest`)**: Added deterministic recovery metadata for WAL format/state and SSTable segment inventory.
- **Checksummed WAL/SSTable v3 path**: New WAL records and newly built SSTables now include integrity checks to detect corruption.
- **Recovery mode option**: Added `recoverMode` option (`permissive` | `strict`) for corruption handling control during startup.
- **SSTable sparse fence index**: Added fence-pointer based key search narrowing for better cache locality on large SSTable indices.
- **Optional SSTable Bloom filter**: Added `bloomFilter` option (default `true`) for faster negative lookups across many SSTables.
- **Async batch APIs**: Added `putBatchAsync` and `getBatchAsync` to extend async coverage beyond single-key operations.
- **Async persistence APIs**: Added `flushAsync` and `compactAsync` to move WAL lifecycle operations off the event loop.
- **Async query APIs**: Added `keysAsync`, `scanAsync`, `rangeAsync`, and `countPrefixAsync` for non-blocking high-cardinality reads.
- **Compaction policy options**: Added `autoCompact`, `compactMinOps`, `compactTombstoneRatio`, and `compactMinWalBytes` options for policy-driven WAL compaction.
- **Amplification metrics in stats**: Added `walBytes`, `logicalWriteBytes`, `physicalWriteBytes`, `compactionCount`, `autoCompactionCount`, `writeAmplification`, and `spaceAmplification` to `stats()`.
- **End-to-end migration guide**: Added `MIGRATION_GUIDE.md` with upgrade, verification, and rollback playbook for v2.4.x -> v3.0.
- **Compatibility matrix**: Added `COMPATIBILITY_MATRIX.md` with runtime/storage compatibility and explicit historical breakages.

### Changed

- **WAL default filename**: Changed default WAL file from `titan.t` to `titan.tkv`.
- **Backward compatibility for WAL migration**: If `titan.tkv` does not exist but legacy `titan.t` exists, TitanKV automatically migrates (or safely falls back) during startup.
- **Roadmap language and structure**: Rewrote `ROADMAP.md` in English with a step-by-step, performance-first v3 plan and measurable release gates.
- **Streaming JSON import path**: `importJSONStream` now uses async batch writes internally to reduce event-loop pressure on large imports.
- **Auto-compaction trigger engine**: TitanEngine now evaluates WAL operation count, tombstone ratio, and WAL size thresholds before auto-running `compact()`.
- **Background-safe compaction lifecycle**: Policy-triggered auto-compaction now runs on a background engine thread, and engine shutdown waits for in-flight compaction to complete safely.
- **Interruption-safe WAL compact recovery**: Added `.bak/.tmp` startup artifact recovery and rollback-aware compact swap to preserve recoverability across interrupted compaction phases.
- **Benchmark metric visibility**: `test/benchmark.js` now prints write amplification, space amplification, WAL bytes, and compaction counters.
- **CI performance regression stage**: Added pinned benchmark scenario and throughput alarms in CI (`perf-regression` job).
- **Benchmark throughput gates**: `test/benchmark.js` now supports pinned throughput floors via env gates (`BENCH_MIN_SEQ_WRITE_OPS`, `BENCH_MIN_SEQ_READ_OPS`, `BENCH_MIN_HAS_OPS`) and scenario sizing knobs.

## [2.4.0] - 2026-03-26

### Added

- **Phase 1: Robustness & Fuzzing (Cluster File Locking & Memory Stability)**: Implemented `titan.lock` mechanism. When deployed via PM2 or cluster mode, secondary processes attempting to access the same directory will throw a safe, descriptive error instead of corrupting the WAL. Included rigorous Fuzz & Memory Leak testing to guarantee stability over millions of operations.
- **Phase 2: Ecosystem Adapters**:
  - **Express.js Adapter (`titankv-express-session`)**: Added official `express-session` store integration! Can be imported out-of-the-box via `require('titankv/lib/express-session')(session)`. Enables developers to switch their external Redis session caches to TitanKV with zero network latency.
  - **Next.js Cache Adapter (`titankv-nextjs-cache`)**: Added a Next.js App Router compatible Custom Cache Handler (`lib/nextjs-cache`). Seamlessly replace Vercel KV or Redis with TitanKV to cache `fetch` requests and ISR completely in-memory with disk persistence.

### Fixed

- **Cascade Deletions for Complex Types**: Fixed a memory/storage leak where calling `db.del(key)` on sets, lists, or hashes would only delete the meta-mapping but leave the underlying prefixed keys in the C++ layer.
- **Benchmark Lock Release**: Fixed issue in `test/benchmark.js` where lack of explicit `close()` calls caused IPC lock failures during testing.

## [2.2.0] - 2026-03-20

### Added

- **Set operations**: `sunion(...keys)`, `sinter(...keys)`, `sdiff(key, ...otherKeys)` for union, intersection, and difference.
- **`rename(oldKey, newKey)`**: Atomically rename a key, preserving TTL. Throws if key doesn't exist.
- **`type(key)`**: Returns key type — `'string'` | `'list'` | `'set'` | `'hash'` | `'zset'` | `'none'`.
- **`randomkey()`**: Returns a random key from the database, or `null` if empty.
- **`exists(key)`**: Redis-compatible alias for `has()`.
- **`dbsize()`**: Redis-compatible alias for `size()`.
- **Background TTL cleanup**: New `cleanupIntervalMs` option. When set, a background timer periodically purges expired keys that haven't been accessed.
- **TypeScript**: All new methods added to type definitions.

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
