export interface TitanOptions {
    sync?: 'sync' | 'async' | 'none';
}

export interface TitanStats {
    totalKeys: number;
    totalOps: number;
    hits: number;
    misses: number;
    expired: number;
    hitRate: number;
}

export class TitanKV {
    constructor(dataDir?: string, options?: TitanOptions);

    // core operations
    put(key: string, value: string, ttlMs?: number): void;
    get(key: string): string | null;
    del(key: string): boolean;
    has(key: string): boolean;
    size(): number;
    clear(): void;

    // atomic operations
    incr(key: string, delta?: number): number;
    decr(key: string, delta?: number): number;

    // query operations
    keys(): string[];
    scan(prefix: string): [string, string][];
    range(start: string, end: string): [string, string][];
    countPrefix(prefix: string): number;

    // batch operations
    putBatch(pairs: [string, string][]): void;
    getBatch(keys: string[]): (string | null)[];

    // list operations (Redis-like)
    lpush(key: string, value: string): number;
    rpush(key: string, value: string): number;
    lpop(key: string): string | null;
    rpop(key: string): string | null;
    lrange(key: string, start: number, stop: number): string[];
    llen(key: string): number;

    // set operations (Redis-like)
    sadd(key: string, member: string): number;
    srem(key: string, member: string): boolean;
    sismember(key: string, member: string): boolean;
    smembers(key: string): string[];
    scard(key: string): number;

    // persistence
    flush(): void;

    // statistics
    stats(): TitanStats;
}
