# TitanKV Roadmap (Performance-First)

This roadmap defines the execution order for v3 with one hard rule: **speed and correctness ship together**.

## Product Direction

TitanKV is an ultra-fast embedded key-value database for Node.js. The main value is eliminating external cache/database network hops for common app workloads while keeping a Redis-like developer experience.

## Current Baseline (v2.4.x)

- Strong in-process throughput for core operations
- WAL persistence with locking and compaction
- Express session adapter and Next.js cache handler
- Foundational async APIs and disk spill/restart behavior

## v3 Execution Plan (Apply in Order)

### 1) Recovery Integrity Foundation (v3 alpha.1)

Status:

- Completed: manifest/checksum/corruption-mode implementation landed and is test-validated.

Deliverables:

- Add manifest/metadata file for segment inventory and replay boundaries
- Add checksums for WAL and SSTable records/pages
- Define deterministic startup recovery order and corruption handling modes

Performance and quality gates:

- 0 data loss in controlled crash-recovery test matrix
- Startup recovery remains bounded and predictable for large datasets
- No throughput regression greater than 10% versus v2.4 baseline on core benchmark set

### 2) Read Path Acceleration at Scale (v3 alpha.2)

Status:

- Completed: sparse fence-index and optional Bloom filter path landed and is test-validated.

Deliverables:

- Sparse index improvements for SSTable lookups
- Optional Bloom filter per SSTable to reduce unnecessary disk reads
- Faster negative lookups and reduced read amplification

Performance and quality gates:

- Lower p95 read latency for high-cardinality datasets
- Measurable reduction in disk reads per miss
- Stable memory overhead under long-running scan/read workloads

### 3) Full Async I/O Surface (v3 beta.1)

Status:

- Completed: async API surface now includes core (`putAsync/getAsync`), batch (`putBatchAsync/getBatchAsync`), query (`keysAsync/scanAsync/rangeAsync/countPrefixAsync`), and WAL lifecycle (`flushAsync/compactAsync`) paths.

Deliverables:

- Extend async workers beyond `putAsync/getAsync` to heavy disk paths
- Add async variants for batch and long-running operations where needed
- Ensure event-loop friendly behavior under sustained load

Performance and quality gates:

- Event loop delay stays within agreed budget under mixed read/write load
- No deadlocks/races in stress + fuzz scenarios
- Throughput remains competitive with sync path in equivalent workloads

### 4) Compaction Policy and Lifecycle (v3 beta.2)

Status:

- Completed: policy-driven auto-compaction, background-safe lifecycle, interruption-safe recovery, and amplification metrics with benchmark quality gates are now in place.

Deliverables:

- Define compaction trigger policy (size ratio, tombstone ratio, level pressure)
- Background-safe compaction lifecycle and failure recovery semantics
- Clear write amplification and space amplification targets

Performance and quality gates:

- Bounded disk growth during churn-heavy workloads
- Predictable compaction impact on p95/p99 latency
- Recovery consistency proven after compaction interruption tests

### 5) Release Candidate Hardening (v3 rc.1)

Status:

- Completed: migration and compatibility documentation landed, and CI now includes pinned performance regression gates.

Deliverables:

- End-to-end migration guide (including legacy `titan.t` to `titan.tkv` behavior): delivered in `MIGRATION_GUIDE.md`
- Backward-compatibility matrix and explicit breakage documentation: delivered in `COMPATIBILITY_MATRIX.md`
- CI performance regression stage with pinned benchmark scenarios: delivered in `.github/workflows/ci.yml` (`perf-regression` job)

Performance and quality gates:

- No critical correctness failures in soak test suite
- Regression alarms active for read/write latency and throughput
- Release notes include operational limits and recommended tuning profile

## Supporting Workstreams

- Documentation refresh for production tuning and deployment patterns
- Example projects for cache/session/rate-limit/leaderboard usage
- Better observability guidance (`stats()`, benchmark interpretation, capacity planning)

## Definition of Done for v3.0.0

v3 is complete when:

- Recovery correctness, compaction, and async paths are production-stable
- Performance goals are verified in CI and reproducible locally
- Users can migrate with a clear, low-risk playbook
