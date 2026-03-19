'use strict';

const gyp = require('node-gyp-build');
const fs = require('fs');
const { EventEmitter } = require('events');
const { createReadStream } = require('fs');
const native = gyp(require('path').join(__dirname, '..'));

const LIST_PREFIX = '\x00L:';
const SET_PREFIX = '\x00S:';
const HASH_PREFIX = '\x00H:';
const ZSET_PREFIX = '\x00Z:';

class TitanKV extends EventEmitter {
    constructor(path, opts) {
        super();
        if (path) {
            this._db = new native.TitanKV(path, opts || {});
        } else {
            this._db = new native.TitanKV();
        }
        this._ops = 0;
        this._hits = 0;
        this._misses = 0;
        this._subs = new Map();
        this._ttls = new Map();
    }

    // -- Core --

    put(key, value, ttl) {
        this._ops++;
        if (ttl && ttl > 0) {
            this._ttls.set(key, Date.now() + ttl);
        }
        return this._db.put(key, value, ttl || 0);
    }

    get(key) {
        this._ops++;
        const val = this._db.get(key);
        if (val !== null && val !== undefined) {
            this._hits++;
        } else {
            this._misses++;
            this._ttls.delete(key);
        }
        return val;
    }

    del(key) {
        this._ops++;
        this._ttls.delete(key);
        return this._db.del(key);
    }

    has(key) {
        this._ops++;
        return this._db.has(key);
    }

    size() {
        return this._db.size();
    }

    clear() {
        this._ops++;
        this._ttls.clear();
        return this._db.clear();
    }

    incr(key, delta) {
        this._ops++;
        return this._db.incr(key, delta || 1);
    }

    decr(key, delta) {
        this._ops++;
        return this._db.decr(key, delta || 1);
    }

    keys(limit) {
        this._ops++;
        return this._db.keys(limit || 1000);
    }

    scan(prefix, limit) {
        this._ops++;
        return this._db.scan(prefix, limit || 1000);
    }

    range(start, end, limit) {
        this._ops++;
        return this._db.range(start, end, limit || 1000);
    }

    countPrefix(prefix) {
        this._ops++;
        return this._db.countPrefix(prefix);
    }

    putBatch(pairs) {
        this._ops++;
        return this._db.putBatch(pairs);
    }

    getBatch(keys) {
        this._ops++;
        return this._db.getBatch(keys);
    }

    flush() {
        return this._db.flush();
    }

    compact() {
        return this._db.compact();
    }

    // -- EXPIRE / TTL on existing keys --

    expire(key, ttlMs) {
        this._ops++;
        if (!this._db.has(key)) return false;
        const val = this._db.get(key);
        if (val === null || val === undefined) return false;
        this._ttls.set(key, Date.now() + ttlMs);
        this._db.put(key, val, ttlMs);
        return true;
    }

    ttl(key) {
        this._ops++;
        const deadline = this._ttls.get(key);
        if (deadline === undefined) {
            return this._db.has(key) ? -1 : -2;
        }
        const remaining = deadline - Date.now();
        if (remaining <= 0) {
            this._ttls.delete(key);
            return -2;
        }
        return remaining;
    }

    persist(key) {
        this._ops++;
        if (!this._db.has(key)) return false;
        const val = this._db.get(key);
        if (val === null || val === undefined) return false;
        this._ttls.delete(key);
        this._db.put(key, val, 0);
        return true;
    }

    // -- KEYS glob pattern matching --

    keysMatch(pattern, limit) {
        this._ops++;
        const allKeys = this._db.keys(limit || 100000);
        return allKeys.filter(k => _globMatch(pattern, k));
    }

    // -- Cursor-based scan iteration --

    sscan(prefix, cursor, count) {
        this._ops++;
        const batchSize = count || 10;
        const allPairs = this._db.scan(prefix, cursor + batchSize + 1);
        const slice = allPairs.slice(cursor, cursor + batchSize);
        const nextCursor = cursor + slice.length;
        const done = nextCursor >= allPairs.length;
        return {
            cursor: done ? 0 : nextCursor,
            entries: slice,
            done,
        };
    }

    *iterate(prefix, batchSize) {
        const step = batchSize || 100;
        let cursor = 0;
        while (true) {
            const result = this.sscan(prefix || '', cursor, step);
            for (const entry of result.entries) {
                yield entry;
            }
            if (result.done) break;
            cursor = result.cursor;
        }
    }

    // -- JSON import/export --

    importJSON(filePath, opts) {
        const prefix = (opts && opts.prefix) || '';
        const raw = fs.readFileSync(filePath, 'utf8');
        const data = JSON.parse(raw);
        const pairs = [];

        if (Array.isArray(data)) {
            const idField = (opts && opts.idField) || 'id';
            for (let i = 0; i < data.length; i++) {
                const item = data[i];
                const id = item[idField] !== undefined ? String(item[idField]) : String(i);
                pairs.push([prefix + id, JSON.stringify(item)]);
            }
        } else if (typeof data === 'object' && data !== null) {
            const entries = Object.entries(data);
            for (const [k, v] of entries) {
                pairs.push([prefix + k, JSON.stringify(v)]);
            }
        }

        const BATCH = 5000;
        for (let i = 0; i < pairs.length; i += BATCH) {
            this._db.putBatch(pairs.slice(i, i + BATCH));
        }
        this._ops += pairs.length;
        return pairs.length;
    }

    importJSONStream(filePath, opts) {
        const prefix = (opts && opts.prefix) || '';
        const batchSize = (opts && opts.batchSize) || 5000;
        const self = this;

        return new Promise((resolve, reject) => {
            const chunks = [];
            const stream = createReadStream(filePath, { encoding: 'utf8' });
            stream.on('data', chunk => chunks.push(chunk));
            stream.on('error', reject);
            stream.on('end', () => {
                try {
                    const data = JSON.parse(chunks.join(''));
                    const pairs = [];

                    if (Array.isArray(data)) {
                        const idField = (opts && opts.idField) || 'id';
                        for (let i = 0; i < data.length; i++) {
                            const item = data[i];
                            const id = item[idField] !== undefined ? String(item[idField]) : String(i);
                            pairs.push([prefix + id, JSON.stringify(item)]);
                        }
                    } else if (typeof data === 'object' && data !== null) {
                        for (const [k, v] of Object.entries(data)) {
                            pairs.push([prefix + k, JSON.stringify(v)]);
                        }
                    }

                    for (let i = 0; i < pairs.length; i += batchSize) {
                        self._db.putBatch(pairs.slice(i, i + batchSize));
                    }
                    self._ops += pairs.length;
                    resolve(pairs.length);
                } catch (err) {
                    reject(err);
                }
            });
        });
    }

    exportJSON(filePath, opts) {
        const prefix = (opts && opts.prefix) || '';
        const limit = (opts && opts.limit) || 1000000;
        let pairs;

        if (prefix) {
            pairs = this._db.scan(prefix, limit);
        } else {
            const allKeys = this._db.keys(limit);
            const vals = this._db.getBatch(allKeys);
            pairs = allKeys.map((k, i) => [k, vals[i]]);
        }

        const result = {};
        for (const [k, v] of pairs) {
            const cleanKey = prefix ? k.slice(prefix.length) : k;
            try { result[cleanKey] = JSON.parse(v); } catch { result[cleanKey] = v; }
        }

        if (filePath) {
            fs.writeFileSync(filePath, JSON.stringify(result, null, 2), 'utf8');
        }
        this._ops++;
        return result;
    }

    // -- MULTI/EXEC transactions --

    multi() {
        return new Transaction(this);
    }

    // -- Pub/Sub --

    subscribe(channel, listener) {
        if (!this._subs.has(channel)) {
            this._subs.set(channel, new Set());
        }
        this._subs.get(channel).add(listener);
        this.emit('subscribe', channel, this._subs.get(channel).size);
        return this;
    }

    unsubscribe(channel, listener) {
        const subs = this._subs.get(channel);
        if (!subs) return this;
        if (listener) {
            subs.delete(listener);
        } else {
            subs.clear();
        }
        if (subs.size === 0) this._subs.delete(channel);
        this.emit('unsubscribe', channel);
        return this;
    }

    publish(channel, message) {
        this._ops++;
        const subs = this._subs.get(channel);
        let count = 0;
        if (subs) {
            for (const fn of subs) {
                fn(message, channel);
                count++;
            }
        }
        // pattern-based subscribers
        for (const [pat, listeners] of this._subs) {
            if (pat !== channel && pat.includes('*')) {
                if (_globMatch(pat, channel)) {
                    for (const fn of listeners) {
                        fn(message, channel);
                        count++;
                    }
                }
            }
        }
        return count;
    }

    // -- List operations (Redis-like) --

    _getList(key) {
        const raw = this._db.get(LIST_PREFIX + key);
        if (raw === null || raw === undefined) return [];
        try { return JSON.parse(raw); } catch { return []; }
    }

    _setList(key, arr) {
        this._db.put(LIST_PREFIX + key, JSON.stringify(arr));
    }

    lpush(key, ...values) {
        this._ops++;
        const list = this._getList(key);
        for (let i = values.length - 1; i >= 0; i--) {
            list.unshift(values[i]);
        }
        this._setList(key, list);
        return list.length;
    }

    rpush(key, ...values) {
        this._ops++;
        const list = this._getList(key);
        for (const v of values) list.push(v);
        this._setList(key, list);
        return list.length;
    }

    lpop(key) {
        this._ops++;
        const list = this._getList(key);
        if (list.length === 0) return null;
        const val = list.shift();
        this._setList(key, list);
        return val;
    }

    rpop(key) {
        this._ops++;
        const list = this._getList(key);
        if (list.length === 0) return null;
        const val = list.pop();
        this._setList(key, list);
        return val;
    }

    llen(key) {
        this._ops++;
        return this._getList(key).length;
    }

    lrange(key, start, stop) {
        this._ops++;
        const list = this._getList(key);
        if (stop < 0) stop = list.length + stop;
        return list.slice(start, stop + 1);
    }

    lindex(key, index) {
        this._ops++;
        const list = this._getList(key);
        if (index < 0) index = list.length + index;
        return index >= 0 && index < list.length ? list[index] : null;
    }

    lset(key, index, value) {
        this._ops++;
        const list = this._getList(key);
        if (index < 0) index = list.length + index;
        if (index < 0 || index >= list.length) return false;
        list[index] = value;
        this._setList(key, list);
        return true;
    }

    // -- Set operations (Redis-like) --

    _getSet(key) {
        const raw = this._db.get(SET_PREFIX + key);
        if (raw === null || raw === undefined) return new Set();
        try { return new Set(JSON.parse(raw)); } catch { return new Set(); }
    }

    _setSet(key, s) {
        this._db.put(SET_PREFIX + key, JSON.stringify([...s]));
    }

    sadd(key, ...members) {
        this._ops++;
        const s = this._getSet(key);
        let added = 0;
        for (const m of members) {
            if (!s.has(m)) { s.add(m); added++; }
        }
        if (added > 0) this._setSet(key, s);
        return added;
    }

    srem(key, ...members) {
        this._ops++;
        const s = this._getSet(key);
        let removed = 0;
        for (const m of members) {
            if (s.delete(m)) removed++;
        }
        if (removed > 0) this._setSet(key, s);
        return removed > 0;
    }

    sismember(key, member) {
        this._ops++;
        return this._getSet(key).has(member);
    }

    smembers(key) {
        this._ops++;
        return [...this._getSet(key)];
    }

    scard(key) {
        this._ops++;
        return this._getSet(key).size;
    }

    // -- Hash operations (Redis-like) --

    _getHash(key) {
        const raw = this._db.get(HASH_PREFIX + key);
        if (raw === null || raw === undefined) return {};
        try { return JSON.parse(raw); } catch { return {}; }
    }

    _setHash(key, h) {
        this._db.put(HASH_PREFIX + key, JSON.stringify(h));
    }

    hset(key, field, value) {
        this._ops++;
        const h = this._getHash(key);
        const isNew = !(field in h);
        h[field] = value;
        this._setHash(key, h);
        return isNew ? 1 : 0;
    }

    hmset(key, obj) {
        this._ops++;
        const h = this._getHash(key);
        Object.assign(h, obj);
        this._setHash(key, h);
    }

    hget(key, field) {
        this._ops++;
        const h = this._getHash(key);
        return field in h ? h[field] : null;
    }

    hgetall(key) {
        this._ops++;
        return this._getHash(key);
    }

    hdel(key, ...fields) {
        this._ops++;
        const h = this._getHash(key);
        let count = 0;
        for (const f of fields) {
            if (f in h) { delete h[f]; count++; }
        }
        if (count > 0) this._setHash(key, h);
        return count;
    }

    hexists(key, field) {
        this._ops++;
        return field in this._getHash(key);
    }

    hkeys(key) {
        this._ops++;
        return Object.keys(this._getHash(key));
    }

    hvals(key) {
        this._ops++;
        return Object.values(this._getHash(key));
    }

    hlen(key) {
        this._ops++;
        return Object.keys(this._getHash(key)).length;
    }

    hincrby(key, field, increment) {
        this._ops++;
        const h = this._getHash(key);
        const current = Number(h[field]) || 0;
        h[field] = String(current + (increment || 1));
        this._setHash(key, h);
        return Number(h[field]);
    }

    // -- Sorted Set operations (Redis-like) --

    _getZset(key) {
        const raw = this._db.get(ZSET_PREFIX + key);
        if (raw === null || raw === undefined) return [];
        try { return JSON.parse(raw); } catch { return []; }
    }

    _setZset(key, arr) {
        this._db.put(ZSET_PREFIX + key, JSON.stringify(arr));
    }

    zadd(key, ...args) {
        this._ops++;
        const zset = this._getZset(key);
        let added = 0;

        for (let i = 0; i < args.length; i += 2) {
            const score = Number(args[i]);
            const member = args[i + 1];
            const idx = zset.findIndex(e => e[1] === member);
            if (idx >= 0) {
                zset[idx][0] = score;
            } else {
                zset.push([score, member]);
                added++;
            }
        }
        zset.sort((a, b) => a[0] - b[0] || (a[1] < b[1] ? -1 : a[1] > b[1] ? 1 : 0));
        this._setZset(key, zset);
        return added;
    }

    zrem(key, ...members) {
        this._ops++;
        const zset = this._getZset(key);
        const before = zset.length;
        const filtered = zset.filter(e => !members.includes(e[1]));
        if (filtered.length < before) {
            this._setZset(key, filtered);
        }
        return before - filtered.length;
    }

    zscore(key, member) {
        this._ops++;
        const zset = this._getZset(key);
        const entry = zset.find(e => e[1] === member);
        return entry ? entry[0] : null;
    }

    zcard(key) {
        this._ops++;
        return this._getZset(key).length;
    }

    zrank(key, member) {
        this._ops++;
        const zset = this._getZset(key);
        const idx = zset.findIndex(e => e[1] === member);
        return idx >= 0 ? idx : null;
    }

    zrange(key, start, stop, opts) {
        this._ops++;
        const zset = this._getZset(key);
        if (start < 0) start = Math.max(0, zset.length + start);
        if (stop < 0) stop = zset.length + stop;
        const slice = zset.slice(start, stop + 1);
        if (opts && opts.withScores) {
            return slice.map(e => ({ member: e[1], score: e[0] }));
        }
        return slice.map(e => e[1]);
    }

    zrevrange(key, start, stop, opts) {
        this._ops++;
        const zset = this._getZset(key);
        const reversed = [...zset].reverse();
        if (start < 0) start = Math.max(0, reversed.length + start);
        if (stop < 0) stop = reversed.length + stop;
        const slice = reversed.slice(start, stop + 1);
        if (opts && opts.withScores) {
            return slice.map(e => ({ member: e[1], score: e[0] }));
        }
        return slice.map(e => e[1]);
    }

    zrangebyscore(key, min, max, opts) {
        this._ops++;
        const zset = this._getZset(key);
        const minVal = min === '-inf' ? -Infinity : Number(min);
        const maxVal = max === '+inf' ? Infinity : Number(max);
        let filtered = zset.filter(e => e[0] >= minVal && e[0] <= maxVal);

        if (opts && opts.limit) {
            const offset = opts.limit.offset || 0;
            const count = opts.limit.count || filtered.length;
            filtered = filtered.slice(offset, offset + count);
        }
        if (opts && opts.withScores) {
            return filtered.map(e => ({ member: e[1], score: e[0] }));
        }
        return filtered.map(e => e[1]);
    }

    zincrby(key, increment, member) {
        this._ops++;
        const zset = this._getZset(key);
        const idx = zset.findIndex(e => e[1] === member);
        let newScore;
        if (idx >= 0) {
            zset[idx][0] += Number(increment);
            newScore = zset[idx][0];
        } else {
            newScore = Number(increment);
            zset.push([newScore, member]);
        }
        zset.sort((a, b) => a[0] - b[0] || (a[1] < b[1] ? -1 : a[1] > b[1] ? 1 : 0));
        this._setZset(key, zset);
        return newScore;
    }

    zcount(key, min, max) {
        this._ops++;
        const zset = this._getZset(key);
        const minVal = min === '-inf' ? -Infinity : Number(min);
        const maxVal = max === '+inf' ? Infinity : Number(max);
        return zset.filter(e => e[0] >= minVal && e[0] <= maxVal).length;
    }

    // -- Stats --

    stats() {
        const nativeStats = this._db.stats();
        const total = this._ops;
        const hitRate = total > 0 ? this._hits / total : 0;
        return {
            totalOps: total,
            hits: this._hits,
            misses: this._misses,
            hitRate,
            keyCount: nativeStats.keyCount,
            rawBytes: nativeStats.rawBytes,
            compressedBytes: nativeStats.compressedBytes,
            compressionRatio: nativeStats.compressionRatio,
        };
    }
}

// -- Transaction (MULTI/EXEC) --

class Transaction {
    constructor(db) {
        this._db = db;
        this._queue = [];
    }

    put(key, value, ttl) {
        this._queue.push({ op: 'put', args: [key, value, ttl] });
        return this;
    }

    get(key) {
        this._queue.push({ op: 'get', args: [key] });
        return this;
    }

    del(key) {
        this._queue.push({ op: 'del', args: [key] });
        return this;
    }

    incr(key, delta) {
        this._queue.push({ op: 'incr', args: [key, delta] });
        return this;
    }

    decr(key, delta) {
        this._queue.push({ op: 'decr', args: [key, delta] });
        return this;
    }

    hset(key, field, value) {
        this._queue.push({ op: 'hset', args: [key, field, value] });
        return this;
    }

    lpush(key, ...values) {
        this._queue.push({ op: 'lpush', args: [key, ...values] });
        return this;
    }

    rpush(key, ...values) {
        this._queue.push({ op: 'rpush', args: [key, ...values] });
        return this;
    }

    sadd(key, ...members) {
        this._queue.push({ op: 'sadd', args: [key, ...members] });
        return this;
    }

    zadd(key, ...args) {
        this._queue.push({ op: 'zadd', args: [key, ...args] });
        return this;
    }

    exec() {
        const results = [];
        for (const cmd of this._queue) {
            try {
                const fn = this._db[cmd.op].bind(this._db);
                results.push(fn(...cmd.args));
            } catch (err) {
                results.push(err);
            }
        }
        this._queue.length = 0;
        return results;
    }

    discard() {
        this._queue.length = 0;
        return 'OK';
    }

    get length() {
        return this._queue.length;
    }
}

// -- Helpers --

function _globMatch(pattern, str) {
    let pIndex = 0;
    let sIndex = 0;
    let pStar = -1;
    let sStar = -1;

    while (sIndex < str.length) {
        if (pIndex < pattern.length && (pattern[pIndex] === '?' || pattern[pIndex] === str[sIndex])) {
            pIndex++;
            sIndex++;
        } else if (pIndex < pattern.length && pattern[pIndex] === '*') {
            pStar = pIndex;
            sStar = sIndex;
            pIndex++;
        } else if (pStar !== -1) {
            pIndex = pStar + 1;
            sIndex = ++sStar;
        } else {
            return false;
        }
    }

    while (pIndex < pattern.length && pattern[pIndex] === '*') {
        pIndex++;
    }

    return pIndex === pattern.length;
}

module.exports = { TitanKV, Transaction };
