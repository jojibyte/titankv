# TitanKV Compare Benchmarks

This folder benchmarks TitanKV against optional local embedded engines.
All compare-engine dependencies are isolated under this folder, not in the root project.

## Quick start

1. Build/install TitanKV first:

```bash
npm install
```

2. Run compare benchmark (TitanKV-only is enough):

```bash
npm run benchmark:compare
```

3. Open generated report:

- `benchmark/compare/benchmark-report.html`
- `benchmark/compare/reports/01-titankv.html`
- `benchmark/compare/reports/02-sqlite.html`
- `benchmark/compare/reports/03-lmdb.html`
- `benchmark/compare/reports/04-level.html`
- `benchmark/compare/reports/05-rocksdb.html`

Every run rewrites only this compare report file.
Runner also writes cumulative stage reports in `benchmark/compare/reports` (TitanKV first, then each next engine).

Report now includes a wider operation matrix for each engine:

- sequential insert
- sequential read
- miss read
- mixed read/write (80/20)
- hot-key read skew
- large value put/get

Environment section intentionally reports CPU info only.

## Recommended engine order

Use this order to expand comparisons safely:

1. `titankv` (always available in this repository)
2. `better-sqlite3` (stable, simple baseline)
3. `lmdb` (high read performance baseline)
4. `level` (common JS ecosystem baseline)
5. `rocksdb` (optional native baseline; can be harder to install)

## Install optional engines

Install one by one and re-run benchmark each time:

```bash
npm run benchmark:compare:add:sqlite
npm run benchmark:compare

npm run benchmark:compare:add:lmdb
npm run benchmark:compare

npm run benchmark:compare:add:level
npm run benchmark:compare

npm run benchmark:compare:add:rocksdb
npm run benchmark:compare
```

This flow is sequential, not parallel: each step adds one engine and the report is refreshed after each run.

If an optional package is missing, the runner skips that engine and still produces a report.

## Runtime knobs

You can tune workload using environment variables:

- `COMPARE_ITER` default: `20000`
- `COMPARE_WARMUP` default: `2000`
- `COMPARE_ROUNDS` default: `5` (measured rounds used in median)
- `COMPARE_BURNIN_ROUNDS` default: `1` (warm-up rounds, excluded from scoring)
- `COMPARE_MAX_CV` default: `0` (disabled). If `> 0`, run exits non-zero when any engine exceeds this stability CV (%)

For more confidence, increase rounds and keep burn-in enabled.
Recommended stable profile:

- `COMPARE_ITER=5000`
- `COMPARE_WARMUP=500`
- `COMPARE_ROUNDS=7`
- `COMPARE_BURNIN_ROUNDS=1`

Example (PowerShell):

```bash
$env:COMPARE_ITER=5000; $env:COMPARE_WARMUP=500; $env:COMPARE_ROUNDS=7; $env:COMPARE_BURNIN_ROUNDS=1; npm run benchmark:compare
```

Optional stability gate example:

```bash
$env:COMPARE_ITER=5000; $env:COMPARE_WARMUP=500; $env:COMPARE_ROUNDS=7; $env:COMPARE_BURNIN_ROUNDS=1; $env:COMPARE_MAX_CV=15; npm run benchmark:compare
```
