# TitanKV Compatibility Matrix

This matrix defines supported combinations, backward compatibility guarantees, and known breaks.

## Runtime and Platform

| Area                 | Status    | Notes                           |
| -------------------- | --------- | ------------------------------- |
| Node.js 16           | Supported | Included in CI version matrix   |
| Node.js 18           | Supported | Included in CI version matrix   |
| Node.js 20           | Supported | Included in CI version matrix   |
| Node.js 22           | Supported | Included in CI version matrix   |
| Linux x64 prebuild   | Supported | CI build and publish validation |
| macOS arm64 prebuild | Supported | CI build and publish validation |
| Windows x64 prebuild | Supported | CI build and publish validation |

## Storage Compatibility

| Storage Artifact           | Compatibility                      | Notes                                                                                |
| -------------------------- | ---------------------------------- | ------------------------------------------------------------------------------------ |
| `titan.tkv` WAL            | Forward for v3                     | Primary WAL format                                                                   |
| Legacy `titan.t` WAL       | Backward-compatible migration path | If `titan.tkv` missing and `titan.t` exists, startup migration/fallback path is used |
| `titan.manifest`           | v3 metadata                        | Stores recovery inventory and WAL metadata                                           |
| SSTable checksummed format | v3 path                            | Validated on read with checksum checks                                               |

## API Surface Compatibility

| Feature                                                         | v2.4.x | v3.0 |
| --------------------------------------------------------------- | ------ | ---- |
| Core sync API (`put/get/del/has/...`)                           | Yes    | Yes  |
| Async API (`putAsync/getAsync`)                                 | Yes    | Yes  |
| Async batch/query (`putBatchAsync/getBatchAsync/keysAsync/...`) | Yes    | Yes  |
| Compaction controls (`autoCompact`, policy knobs)               | Yes    | Yes  |
| Recovery mode (`recoverMode`)                                   | Yes    | Yes  |

## Explicit Breakage Documentation

Current v3 line has no new intentional API break versus v2.4.x in core/public methods.

Historical breakages to be aware of when migrating from older lines:

1. v2.1.0: WAL format v2 incompatibility from v2.0.0 (`titan.t` behavior changed).
2. v2.1.0: `srem()` return type changed from boolean to number.
3. v2.x: build pipeline switched to `cmake-js` (toolchain expectations changed from very old setups).

## CI Regression Alarms

The CI pipeline enforces pinned benchmark scenarios for:

- Sequential write throughput floor
- Sequential read throughput floor
- `has()` throughput floor
- Amplification and compaction quality gates

This provides automatic alarms for performance regressions on pull requests and main branch updates.
