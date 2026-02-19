#include "titankv.hpp"
#include "storage.hpp"
#include "wal.hpp"
#include "utils.hpp"
#include <algorithm>

namespace titan {

TitanEngine::TitanEngine() : TitanEngine("") {}

TitanEngine::TitanEngine(const std::string& data_dir) {
    storage_ = std::make_unique<Storage>();
    if (!data_dir.empty()) {
        wal_ = std::make_unique<WAL>(std::filesystem::path(data_dir));
        recover();
    }
}

TitanEngine::~TitanEngine() = default;

void TitanEngine::recover() {
    if (!wal_) return;

    auto entries = wal_->recover();
    for (auto& entry : entries) {
        if (entry.op == WalOp::PUT) {
            storage_->putPrecompressed(entry.key, std::move(entry.value));
        } else if (entry.op == WalOp::DEL) {
            storage_->del(entry.key);
        }
    }
}

void TitanEngine::put(const std::string& key, const std::string& value, int64_t ttl_ms) {
    storage_->put(key, value, ttl_ms);
    if (wal_) wal_->logPut(key, value);
}

std::optional<std::string> TitanEngine::get(const std::string& key) {
    return storage_->get(key);
}

bool TitanEngine::del(const std::string& key) {
    bool deleted = storage_->del(key);
    if (deleted && wal_) wal_->logDel(key);
    return deleted;
}

bool TitanEngine::has(const std::string& key) {
    return storage_->has(key);
}

size_t TitanEngine::size() const {
    return storage_->getStats().key_count;
}

void TitanEngine::clear() {
    storage_->clear();
    if (wal_) wal_->compact({});
}

int64_t TitanEngine::incr(const std::string& key, int64_t delta) {
    auto val_opt = storage_->get(key);
    int64_t val = 0;
    if (val_opt) {
        try {
            val = std::stoll(*val_opt);
        } catch (...) {
            val = 0;
        }
    }
    val += delta;
    put(key, std::to_string(val));
    return val;
}

int64_t TitanEngine::decr(const std::string& key, int64_t delta) {
    return incr(key, -delta);
}

std::vector<std::string> TitanEngine::keys(size_t limit) const {
    return storage_->keys(limit);
}

std::vector<TitanEngine::KVPair> TitanEngine::scan(const std::string& prefix, size_t limit) const {
    return storage_->scan(prefix, limit);
}

std::vector<TitanEngine::KVPair> TitanEngine::range(
    const std::string& start, const std::string& end, size_t limit) const {
    return storage_->range(start, end, limit);
}

size_t TitanEngine::countPrefix(const std::string& prefix) const {
    constexpr size_t COUNT_LIMIT = 100000;
    return storage_->scan(prefix, COUNT_LIMIT).size();
}

void TitanEngine::putBatch(const std::vector<KVPair>& pairs) {
    for (const auto& [k, v] : pairs) {
        put(k, v);
    }
}

std::vector<std::optional<std::string>> TitanEngine::getBatch(const std::vector<std::string>& keys) {
    std::vector<std::optional<std::string>> results;
    results.reserve(keys.size());
    for (const auto& k : keys) {
        results.push_back(get(k));
    }
    return results;
}

void TitanEngine::flush() {
    if (wal_) wal_->flush();
}

void TitanEngine::compact() {
    if (!wal_) return;

    auto snapshot = storage_->snapshot();
    std::vector<LogEntry> entries;
    entries.reserve(snapshot.size());

    for (auto& [k, v] : snapshot) {
        entries.push_back({WalOp::PUT, std::move(k), std::move(v)});
    }

    wal_->compact(entries);
}

void TitanEngine::setCompressionLevel(int level) {
    storage_->setCompressionLevel(level);
}

StorageStats TitanEngine::getStats() const {
    return storage_->getStats();
}

} // namespace titan
