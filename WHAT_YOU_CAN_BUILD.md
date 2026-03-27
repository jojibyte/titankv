# TitanKV Capabilities Guide

This document explains three things clearly:

1. Current project status
2. What TitanKV provides to product users and developers
3. What can be built with TitanKV in real systems

## Current Project Status (As of 2026-03-27)

- Core v3 feature set is implemented and validated in local test and benchmark flows.
- Package metadata and changelog are prepared for `3.0.0`.
- Release pipeline is configured for tag-based publish.
- Final release operations remaining are operational steps: commit, tag, and push.

Status summary: TitanKV is in release-ready state for v3 from a feature and validation perspective.

## What TitanKV Is

TitanKV is an in-process key-value engine for Node.js with a Redis-like API and optional disk persistence.

Primary design goal:

- Keep application data access local to the Node.js process for very low latency.
- Avoid network round-trips for common cache and state workloads.
- Preserve familiar data-structure APIs so adoption is fast.

## Value for Product Users

These are the practical outcomes your end users see when TitanKV is used well:

- Faster page loads and API responses due to local reads.
- Lower tail latency in high-traffic endpoints.
- More stable behavior under sudden traffic bursts.
- Fewer external cache dependencies in simple deployments.
- Better resilience in single-node and edge-like deployments.

## Value for Developers

These are direct developer benefits:

- Redis-like API surface: easy onboarding.
- Rich data structures in one library:
  - strings
  - lists
  - sets
  - sorted sets
  - hashes
- TTL and expiration controls for cache-first architecture.
- Sync and async APIs for different runtime needs.
- Batch operations for throughput-heavy paths.
- Query helpers (`scan`, `range`, prefix count, pattern match).
- Built-in adapters for Express session and Next.js cache handler.
- TypeScript definitions included.

## Persistence, Recovery, and Operational Safety

TitanKV v3 improves durability and recoverability with:

- WAL-based persistence (`titan.tkv`).
- Recovery modes:
  - `permissive` for partial replay on corrupted tail data
  - `strict` for fail-fast integrity behavior
- Recovery manifest support (`titan.manifest`).
- Optional SSTable Bloom filter for faster negative lookups.
- Compaction policy controls for WAL growth management.
- Amplification and storage metrics in `stats()`.

This gives teams better control over data safety vs startup strictness.

## What You Can Build with TitanKV

### 1) Web Session Infrastructure

- Express session store without external Redis.
- Fast session reads and writes with optional persistence.

### 2) Next.js Fetch and ISR Cache

- App Router cache handler replacement.
- Local caching for fetch results and regeneration metadata.

### 3) API Response Cache Layer

- TTL-based endpoint caching.
- Prefix invalidation for grouped resources.

### 4) Rate Limiting and Quota Control

- Atomic counters with `incr` and `decr`.
- Per-IP, per-token, per-route budgets.

### 5) Leaderboards and Ranking Systems

- Sorted-set based ranking.
- Score updates and top-N retrieval.

### 6) In-Process Job and Queue Workflows

- List operations for producer and worker pipelines.
- Lightweight queueing for single service boundaries.

### 7) Feature Flags and Configuration Cache

- Local read-optimized flag lookups.
- Fast rollout reads with periodic refresh.

### 8) Real-Time Local Pub/Sub Eventing

- In-process channel notifications.
- Service-internal fan-out with minimal overhead.

### 9) Hot Object Cache for Read-Heavy Domains

- Product catalog snippets
- User profile cards
- Routing tables

### 10) Hybrid Memory Plus Disk Data Layer

- Keep hot keys in memory.
- Persist state via WAL and SSTable flows for restart recovery.

## Example Architecture Patterns

### Pattern A: Single Service Local Cache

- API layer
- TitanKV in same process
- Optional background refresh job

Best for: latency-first microservices.

### Pattern B: Monolith with Embedded State

- Web app routes
- TitanKV for sessions, counters, flags
- Primary SQL store for long-term business records

Best for: simplified deployments that want fewer infrastructure pieces.

### Pattern C: Edge-Like Worker Node

- Request handling at local node
- TitanKV as local state and response cache
- Periodic sync to central store

Best for: local-first read performance.

## When TitanKV Is a Strong Fit

- You want sub-millisecond local reads for hot keys.
- You prefer in-process architecture over client-server cache for some workloads.
- You need Redis-like developer ergonomics in a Node.js-native package.
- You want optional persistence without deploying a separate cache service.

## When TitanKV Should Not Be Your Only Data System

- You need cross-region distributed consensus.
- You need multi-node transactional guarantees across services.
- You need analytical queries and relational joins as primary workflow.

In those cases, TitanKV should be used as a fast local layer beside a primary database.

## Developer Adoption Checklist

1. Start with in-memory mode for baseline performance.
2. Add persistence with explicit options once needed.
3. Define key naming conventions with prefixes.
4. Add TTL defaults for cache-like domains.
5. Use batch APIs in write-heavy and read-heavy paths.
6. Track `stats()` metrics in observability dashboards.
7. Run benchmark checks before and after workload changes.

## Bottom Line

TitanKV gives teams a fast, practical local data engine for Node.js.
It enables session systems, caching layers, ranking engines, queue-like flows, and local messaging without mandatory external cache infrastructure.
For many application paths, this means simpler architecture and higher performance at the same time.
