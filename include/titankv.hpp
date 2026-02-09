#pragma once

#include <string>
#include <unordered_map>
#include <map>
#include <optional>
#include <shared_mutex>
#include <vector>
#include <utility>
#include <chrono>
#include <memory>
#include <atomic>
#include <functional>
#include <thread>
#include <deque>
#include <unordered_set>

namespace titan {

class WAL;
enum class SyncMode;

// FNV-1a hash
struct FastHash {
    size_t operator()(const std::string& s) const noexcept {
        size_t hash = 14695981039346656037ULL;
        for (char c : s) {
            hash ^= static_cast<size_t>(c);
            hash *= 1099511628211ULL;
        }
        return hash;
    }
};

struct ValueEntry {
    std::string value;
    int64_t expires_at = 0;
};

struct Stats {
    size_t total_keys = 0;
    size_t total_ops = 0;
    size_t hits = 0;
    size_t misses = 0;
    size_t expired = 0;
};

class TitanEngine {
public:
    using KVPair = std::pair<std::string, std::string>;
    using Clock = std::chrono::steady_clock;

    TitanEngine();
    TitanEngine(const std::string& data_dir, int sync_mode = 1);
    ~TitanEngine();

    TitanEngine(const TitanEngine&) = delete;
    TitanEngine& operator=(const TitanEngine&) = delete;

    // core ops
    void put(const std::string& key, const std::string& value, int64_t ttl_ms = 0);
    std::optional<std::string> get(const std::string& key);
    bool del(const std::string& key);
    bool has(const std::string& key);
    size_t size() const;
    void clear();

    // atomic
    int64_t incr(const std::string& key, int64_t delta = 1);
    int64_t decr(const std::string& key, int64_t delta = 1);

    // iteration & queries
    std::vector<std::string> keys() const;
    std::vector<KVPair> scan(const std::string& prefix) const;
    std::vector<KVPair> range(const std::string& start, const std::string& end) const;
    size_t countPrefix(const std::string& prefix) const;

    // list ops (redis-like)
    size_t lpush(const std::string& key, const std::string& value);
    size_t rpush(const std::string& key, const std::string& value);
    std::optional<std::string> lpop(const std::string& key);
    std::optional<std::string> rpop(const std::string& key);
    std::vector<std::string> lrange(const std::string& key, int start, int stop);
    size_t llen(const std::string& key);

    // set ops
    size_t sadd(const std::string& key, const std::string& member);
    bool srem(const std::string& key, const std::string& member);
    bool sismember(const std::string& key, const std::string& member);
    std::vector<std::string> smembers(const std::string& key);
    size_t scard(const std::string& key);

    // batch
    void putBatch(const std::vector<KVPair>& pairs);
    std::vector<std::optional<std::string>> getBatch(const std::vector<std::string>& keys);

    // persistence
    void flush();
    void compact();

    // stats
    Stats getStats() const;

private:
    mutable std::shared_mutex mutex_;
    std::unordered_map<std::string, ValueEntry, FastHash> store_;
    std::unordered_map<std::string, std::deque<std::string>, FastHash> lists_;
    std::unordered_map<std::string, std::unordered_set<std::string>, FastHash> sets_;
    std::unique_ptr<WAL> wal_;
    int64_t start_time_;
    mutable Stats stats_;

    int64_t now() const;
    bool isExpired(const ValueEntry& entry) const;
    void recover();
};

} // namespace titan
