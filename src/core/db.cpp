#include "titankv.hpp"
#include "wal.hpp"
#include <algorithm>

namespace titan {

int64_t TitanEngine::now() const {
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        Clock::now().time_since_epoch()
    ).count();
    return ms - start_time_;
}

bool TitanEngine::isExpired(const ValueEntry& entry) const {
    if (entry.expires_at == 0) return false;
    return now() >= entry.expires_at;
}

TitanEngine::TitanEngine() 
    : start_time_(std::chrono::duration_cast<std::chrono::milliseconds>(
          Clock::now().time_since_epoch()).count()) {
    store_.reserve(10000);
}

TitanEngine::TitanEngine(const std::string& data_dir, int sync_mode)
    : TitanEngine() {
    SyncMode mode = static_cast<SyncMode>(sync_mode);
    wal_ = std::make_unique<WAL>(data_dir, mode);
    recover();
}

TitanEngine::~TitanEngine() {
    if (wal_) wal_->flush();
}

void TitanEngine::recover() {
    if (!wal_) return;
    auto entries = wal_->recover();
    for (const auto& e : entries) {
        if (e.op == WalOp::PUT) {
            store_[e.key] = {e.value, 0};
        } else if (e.op == WalOp::DEL) {
            store_.erase(e.key);
        }
    }
}

void TitanEngine::put(const std::string& key, const std::string& value, int64_t ttl_ms) {
    std::unique_lock lock(mutex_);
    int64_t expires = ttl_ms > 0 ? now() + ttl_ms : 0;
    store_[key] = {value, expires};
    stats_.total_ops++;
    if (wal_) wal_->logPut(key, value);
}

std::optional<std::string> TitanEngine::get(const std::string& key) {
    std::shared_lock lock(mutex_);
    stats_.total_ops++;
    auto it = store_.find(key);
    if (it == store_.end()) {
        stats_.misses++;
        return std::nullopt;
    }
    if (isExpired(it->second)) {
        stats_.expired++;
        stats_.misses++;
        return std::nullopt;
    }
    stats_.hits++;
    return it->second.value;
}

bool TitanEngine::del(const std::string& key) {
    std::unique_lock lock(mutex_);
    stats_.total_ops++;
    auto erased = store_.erase(key) > 0;
    if (erased && wal_) wal_->logDel(key);
    return erased;
}

bool TitanEngine::has(const std::string& key) {
    std::shared_lock lock(mutex_);
    stats_.total_ops++;
    auto it = store_.find(key);
    if (it == store_.end()) return false;
    return !isExpired(it->second);
}

size_t TitanEngine::size() const {
    std::shared_lock lock(mutex_);
    return store_.size();
}

void TitanEngine::clear() {
    std::unique_lock lock(mutex_);
    store_.clear();
    lists_.clear();
    sets_.clear();
    if (wal_) wal_->compact();
}

int64_t TitanEngine::incr(const std::string& key, int64_t delta) {
    std::unique_lock lock(mutex_);
    stats_.total_ops++;
    auto it = store_.find(key);
    int64_t val = 0;
    if (it != store_.end() && !isExpired(it->second)) {
        try { val = std::stoll(it->second.value); } catch (...) {}
    }
    val += delta;
    std::string new_val = std::to_string(val);
    store_[key] = {new_val, 0};
    if (wal_) wal_->logPut(key, new_val);
    return val;
}

int64_t TitanEngine::decr(const std::string& key, int64_t delta) {
    return incr(key, -delta);
}

std::vector<std::string> TitanEngine::keys() const {
    std::shared_lock lock(mutex_);
    std::vector<std::string> result;
    result.reserve(store_.size());
    for (const auto& [k, v] : store_) {
        if (!isExpired(v)) result.push_back(k);
    }
    return result;
}

std::vector<TitanEngine::KVPair> TitanEngine::scan(const std::string& prefix) const {
    std::shared_lock lock(mutex_);
    std::vector<KVPair> result;
    for (const auto& [k, v] : store_) {
        if (k.size() >= prefix.size() && 
            k.compare(0, prefix.size(), prefix) == 0 &&
            !isExpired(v)) {
            result.emplace_back(k, v.value);
        }
    }
    return result;
}

std::vector<TitanEngine::KVPair> TitanEngine::range(const std::string& start, const std::string& end) const {
    std::shared_lock lock(mutex_);
    std::vector<KVPair> result;
    for (const auto& [k, v] : store_) {
        if (k >= start && k <= end && !isExpired(v)) {
            result.emplace_back(k, v.value);
        }
    }
    std::sort(result.begin(), result.end());
    return result;
}

size_t TitanEngine::countPrefix(const std::string& prefix) const {
    std::shared_lock lock(mutex_);
    size_t count = 0;
    for (const auto& [k, v] : store_) {
        if (k.size() >= prefix.size() && 
            k.compare(0, prefix.size(), prefix) == 0 &&
            !isExpired(v)) {
            count++;
        }
    }
    return count;
}

// list ops
size_t TitanEngine::lpush(const std::string& key, const std::string& value) {
    std::unique_lock lock(mutex_);
    stats_.total_ops++;
    lists_[key].push_front(value);
    return lists_[key].size();
}

size_t TitanEngine::rpush(const std::string& key, const std::string& value) {
    std::unique_lock lock(mutex_);
    stats_.total_ops++;
    lists_[key].push_back(value);
    return lists_[key].size();
}

std::optional<std::string> TitanEngine::lpop(const std::string& key) {
    std::unique_lock lock(mutex_);
    stats_.total_ops++;
    auto it = lists_.find(key);
    if (it == lists_.end() || it->second.empty()) return std::nullopt;
    std::string val = std::move(it->second.front());
    it->second.pop_front();
    if (it->second.empty()) lists_.erase(it);
    return val;
}

std::optional<std::string> TitanEngine::rpop(const std::string& key) {
    std::unique_lock lock(mutex_);
    stats_.total_ops++;
    auto it = lists_.find(key);
    if (it == lists_.end() || it->second.empty()) return std::nullopt;
    std::string val = std::move(it->second.back());
    it->second.pop_back();
    if (it->second.empty()) lists_.erase(it);
    return val;
}

std::vector<std::string> TitanEngine::lrange(const std::string& key, int start, int stop) {
    std::shared_lock lock(mutex_);
    auto it = lists_.find(key);
    if (it == lists_.end()) return {};
    const auto& list = it->second;
    int len = static_cast<int>(list.size());
    if (start < 0) start = std::max(0, len + start);
    if (stop < 0) stop = len + stop;
    stop = std::min(stop, len - 1);
    std::vector<std::string> result;
    for (int i = start; i <= stop && i < len; i++) {
        result.push_back(list[i]);
    }
    return result;
}

size_t TitanEngine::llen(const std::string& key) {
    std::shared_lock lock(mutex_);
    auto it = lists_.find(key);
    return it != lists_.end() ? it->second.size() : 0;
}

// set ops
size_t TitanEngine::sadd(const std::string& key, const std::string& member) {
    std::unique_lock lock(mutex_);
    stats_.total_ops++;
    auto [_, inserted] = sets_[key].insert(member);
    return inserted ? 1 : 0;
}

bool TitanEngine::srem(const std::string& key, const std::string& member) {
    std::unique_lock lock(mutex_);
    stats_.total_ops++;
    auto it = sets_.find(key);
    if (it == sets_.end()) return false;
    bool removed = it->second.erase(member) > 0;
    if (it->second.empty()) sets_.erase(it);
    return removed;
}

bool TitanEngine::sismember(const std::string& key, const std::string& member) {
    std::shared_lock lock(mutex_);
    auto it = sets_.find(key);
    if (it == sets_.end()) return false;
    return it->second.count(member) > 0;
}

std::vector<std::string> TitanEngine::smembers(const std::string& key) {
    std::shared_lock lock(mutex_);
    auto it = sets_.find(key);
    if (it == sets_.end()) return {};
    return std::vector<std::string>(it->second.begin(), it->second.end());
}

size_t TitanEngine::scard(const std::string& key) {
    std::shared_lock lock(mutex_);
    auto it = sets_.find(key);
    return it != sets_.end() ? it->second.size() : 0;
}

void TitanEngine::putBatch(const std::vector<KVPair>& pairs) {
    std::unique_lock lock(mutex_);
    store_.reserve(store_.size() + pairs.size());
    for (const auto& [key, value] : pairs) {
        store_[key] = {value, 0};
        stats_.total_ops++;
        if (wal_) wal_->logPut(key, value);
    }
}

std::vector<std::optional<std::string>> TitanEngine::getBatch(const std::vector<std::string>& keys) {
    std::shared_lock lock(mutex_);
    std::vector<std::optional<std::string>> results;
    results.reserve(keys.size());
    for (const auto& key : keys) {
        stats_.total_ops++;
        auto it = store_.find(key);
        if (it != store_.end() && !isExpired(it->second)) {
            stats_.hits++;
            results.push_back(it->second.value);
        } else {
            stats_.misses++;
            results.push_back(std::nullopt);
        }
    }
    return results;
}

void TitanEngine::flush() {
    if (wal_) wal_->flush();
}

void TitanEngine::compact() {
    if (wal_) wal_->compact();
}

Stats TitanEngine::getStats() const {
    std::shared_lock lock(mutex_);
    Stats s = stats_;
    s.total_keys = store_.size();
    return s;
}

} // namespace titan
