import { EventEmitter } from 'events';

export interface TitanOptions {
    compressionLevel?: number;
    sync?: 'sync' | 'async' | 'none';
}

export interface TitanStats {
    totalOps: number;
    hits: number;
    misses: number;
    hitRate: number;
    keyCount: number;
    rawBytes: number;
    compressedBytes: number;
    compressionRatio: number;
}

export interface ImportOptions {
    prefix?: string;
    idField?: string;
}

export interface StreamImportOptions extends ImportOptions {
    batchSize?: number;
}

export interface ExportOptions {
    prefix?: string;
    limit?: number;
}

export interface ScanResult {
    cursor: number;
    entries: [string, string][];
    done: boolean;
}

export interface ZMember {
    member: string;
    score: number;
}

export interface ZRangeOptions {
    withScores?: boolean;
}

export interface ZRangeByScoreOptions extends ZRangeOptions {
    limit?: { offset: number; count: number };
}

export class Transaction {
    put(key: string, value: string, ttl?: number): this;
    get(key: string): this;
    del(key: string): this;
    incr(key: string, delta?: number): this;
    decr(key: string, delta?: number): this;
    hset(key: string, field: string, value: string): this;
    lpush(key: string, ...values: string[]): this;
    rpush(key: string, ...values: string[]): this;
    sadd(key: string, ...members: string[]): this;
    zadd(key: string, ...args: (number | string)[]): this;
    exec(): unknown[];
    discard(): string;
    readonly length: number;
}

export class TitanKV extends EventEmitter {
    constructor(dataDir?: string, options?: TitanOptions);

    // Core
    put(key: string, value: string, ttlMs?: number): void;
    get(key: string): string | null;
    del(key: string): boolean;
    has(key: string): boolean;
    size(): number;
    clear(): void;

    // Atomic
    incr(key: string, delta?: number): number;
    decr(key: string, delta?: number): number;

    // Query
    keys(limit?: number): string[];
    scan(prefix: string, limit?: number): [string, string][];
    range(start: string, end: string, limit?: number): [string, string][];
    countPrefix(prefix: string): number;
    keysMatch(pattern: string, limit?: number): string[];

    // Batch
    putBatch(pairs: [string, string][]): void;
    getBatch(keys: string[]): (string | null)[];

    // TTL
    expire(key: string, ttlMs: number): boolean;
    ttl(key: string): number;
    persist(key: string): boolean;

    // Cursor-based scan
    sscan(prefix: string, cursor: number, count?: number): ScanResult;
    iterate(prefix?: string, batchSize?: number): IterableIterator<[string, string]>;

    // JSON
    importJSON(filePath: string, opts?: ImportOptions): number;
    importJSONStream(filePath: string, opts?: StreamImportOptions): Promise<number>;
    exportJSON(filePath?: string | null, opts?: ExportOptions): Record<string, unknown>;

    // Transactions
    multi(): Transaction;

    // Pub/Sub
    subscribe(channel: string, listener: (message: string, channel: string) => void): this;
    unsubscribe(channel: string, listener?: (message: string, channel: string) => void): this;
    publish(channel: string, message: string): number;

    // List (Redis-like)
    lpush(key: string, ...values: string[]): number;
    rpush(key: string, ...values: string[]): number;
    lpop(key: string): string | null;
    rpop(key: string): string | null;
    lrange(key: string, start: number, stop: number): string[];
    llen(key: string): number;
    lindex(key: string, index: number): string | null;
    lset(key: string, index: number, value: string): boolean;

    // Set (Redis-like)
    sadd(key: string, ...members: string[]): number;
    srem(key: string, ...members: string[]): boolean;
    sismember(key: string, member: string): boolean;
    smembers(key: string): string[];
    scard(key: string): number;

    // Hash (Redis-like)
    hset(key: string, field: string, value: string): number;
    hmset(key: string, obj: Record<string, string>): void;
    hget(key: string, field: string): string | null;
    hgetall(key: string): Record<string, string>;
    hdel(key: string, ...fields: string[]): number;
    hexists(key: string, field: string): boolean;
    hkeys(key: string): string[];
    hvals(key: string): string[];
    hlen(key: string): number;
    hincrby(key: string, field: string, increment?: number): number;

    // Sorted Set (Redis-like)
    zadd(key: string, ...args: (number | string)[]): number;
    zrem(key: string, ...members: string[]): number;
    zscore(key: string, member: string): number | null;
    zcard(key: string): number;
    zrank(key: string, member: string): number | null;
    zrange(key: string, start: number, stop: number, opts?: ZRangeOptions): string[] | ZMember[];
    zrevrange(key: string, start: number, stop: number, opts?: ZRangeOptions): string[] | ZMember[];
    zrangebyscore(key: string, min: number | '-inf', max: number | '+inf', opts?: ZRangeByScoreOptions): string[] | ZMember[];
    zincrby(key: string, increment: number, member: string): number;
    zcount(key: string, min: number | '-inf', max: number | '+inf'): number;

    // Persistence
    flush(): void;
    compact(): void;

    // Stats
    stats(): TitanStats;
}
