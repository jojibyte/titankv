# TitanKV Migration Guide (v2.4.x -> v3.0)

This guide is for teams upgrading from TitanKV v2.4.x to v3.0 with low risk and predictable performance.

## Scope

- Source: v2.4.x
- Target: v3.0.0
- Runtime: Node.js >= 16

## What Changed in v3

- Recovery integrity model added (`recoverMode`: `permissive` | `strict`)
- WAL/SSTable checksummed v3 path
- Optional SSTable Bloom filter (`bloomFilter`, default `true`)
- Compaction policy controls (`autoCompact`, `compactMinOps`, `compactTombstoneRatio`, `compactMinWalBytes`)
- Expanded async APIs for heavy operations

## Pre-Upgrade Checklist

1. Pin current production version and create a rollback tag.
2. Back up each database directory (`data/`, WAL, and SSTables).
3. Capture baseline metrics before upgrade:
   - throughput (write/read/has)
   - p95/p99 latency
   - `db.stats()` values (`walBytes`, `writeAmplification`, `spaceAmplification`)
4. Confirm your service startup path can pass explicit options to `new TitanKV(...)`.

## WAL Compatibility and `titan.t` Migration

v3 uses `titan.tkv` as the default WAL filename.

Legacy behavior is preserved:

- If `titan.tkv` exists: v3 uses it.
- If `titan.tkv` does not exist but `titan.t` exists: TitanKV migrates (or falls back safely) on startup.

Recommended operational step:

1. Upgrade one replica/canary first.
2. Start service and verify the new WAL/manifests are created.
3. Validate a read/write smoke test before broad rollout.

## Recommended Startup Profiles

### 1) Safety-First (strict corruption handling)

```js
const db = new TitanKV("./data", {
  sync: "sync",
  recoverMode: "strict",
  bloomFilter: true,
  autoCompact: true,
  compactMinOps: 2000,
  compactTombstoneRatio: 0.35,
  compactMinWalBytes: 4 * 1024 * 1024,
});
```

### 2) Throughput-First (still production-safe)

```js
const db = new TitanKV("./data", {
  sync: "async",
  recoverMode: "permissive",
  bloomFilter: true,
  autoCompact: true,
});
```

## Validation Steps After Upgrade

1. Run functional tests:
   - `npm test`
2. Run benchmark quality gates:
   - `npm run benchmark`
3. Confirm compaction and amplification behavior:
   - `db.stats().writeAmplification`
   - `db.stats().spaceAmplification`
   - `db.stats().walBytes`
4. Restart process and verify persistence recovery.

## Canary Rollout Checklist

Use this sequence for low-risk production rollout.

| Step | Action                                     | Pass Criteria                                                  | Abort Condition                        |
| ---- | ------------------------------------------ | -------------------------------------------------------------- | -------------------------------------- |
| 1    | Deploy v3 to a single canary instance      | Service boots cleanly; no startup recovery errors              | Startup exception or recovery mismatch |
| 2    | Verify WAL/manifest state                  | `titan.tkv` and `titan.manifest` present and readable          | Missing/invalid WAL or manifest        |
| 3    | Run live smoke traffic (read/write/delete) | Functional parity with previous version                        | Any correctness mismatch               |
| 4    | Observe 30-60 min on canary                | Error rate and p95/p99 within agreed budget                    | Error rate spike or latency regression |
| 5    | Compare performance KPIs                   | Throughput not below baseline floor; `has/read/write` stable   | Throughput floor violated              |
| 6    | Check storage health                       | `walBytes` growth bounded; amplification within expected range | Unbounded WAL growth or WA/SA anomaly  |
| 7    | Expand to 10-25% fleet                     | Same SLO/SLA behavior as canary                                | New regressions at partial rollout     |
| 8    | Complete rollout to 100%                   | Stable metrics across full fleet                               | Any critical incident                  |

## Rollback Plan

1. Stop service.
2. Restore the pre-upgrade data backup.
3. Deploy previous application version.
4. Start service and run read/write smoke tests.

## Operational Limits and Tuning Notes

- Use bounded key length and practical value sizes.
- Keep `bloomFilter: true` for read-heavy datasets with many SSTables.
- Keep compaction enabled in churn-heavy workloads.
- Track `walBytes` continuously and investigate sustained growth.
- For large imports, prefer async import paths.
