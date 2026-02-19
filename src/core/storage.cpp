#include "storage.hpp"
#include <chrono>
#include <algorithm>
#include <cstring>

namespace titan {

Storage::Storage() {
    compressor_ = std::make_unique<Compressor>();
}

int64_t Storage::now() const {
    using namespace std::chrono;
    return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}

bool Storage::isExpired(const ValueEntry& entry) const {
    if (entry.expires_at == 0) return false;
    return now() >= entry.expires_at;
}

void Storage::put(const std::string& key, const std::string& value, int64_t ttl_ms) {
    std::unique_lock lock(mutex_);
    TITAN_ASSERT(!key.empty(), "key cannot be empty");

    auto compressed = compressor_->compress(value, compression_level_);

    raw_bytes_ += value.size();
    compressed_bytes_ += compressed.size();

    int64_t expires = ttl_ms > 0 ? now() + ttl_ms : 0;
    store_[key] = {std::move(compressed), expires};
}

void Storage::putPrecompressed(const std::string& key, std::vector<uint8_t>&& compressed_value, int64_t ttl_ms) {
    std::unique_lock lock(mutex_);
    compressed_bytes_ += compressed_value.size();
    int64_t expires = ttl_ms > 0 ? now() + ttl_ms : 0;
    store_[key] = {std::move(compressed_value), expires};
}

std::optional<std::string> Storage::get(const std::string& key) {
    std::shared_lock lock(mutex_);

    auto it = store_.find(key);
    if (it == store_.end()) return std::nullopt;
    if (isExpired(it->second)) return std::nullopt;

    return compressor_->decompress(it->second.compressed_value);
}

bool Storage::del(const std::string& key) {
    std::unique_lock lock(mutex_);
    return store_.erase(key) > 0;
}

bool Storage::has(const std::string& key) {
    std::shared_lock lock(mutex_);
    auto it = store_.find(key);
    if (it == store_.end()) return false;
    return !isExpired(it->second);
}

void Storage::clear() {
    std::unique_lock lock(mutex_);
    store_.clear();
    raw_bytes_ = 0;
    compressed_bytes_ = 0;
}

StorageStats Storage::getStats() const {
    std::shared_lock lock(mutex_);
    StorageStats s;
    s.key_count = store_.size();
    s.raw_bytes = raw_bytes_;
    s.compressed_bytes = compressed_bytes_;
    return s;
}

std::vector<std::string> Storage::keys(size_t limit) const {
    std::shared_lock lock(mutex_);
    std::vector<std::string> result;
    result.reserve(std::min(limit, store_.size()));

    for (const auto& [k, v] : store_) {
        if (!isExpired(v)) {
            result.push_back(k);
            if (result.size() >= limit) break;
        }
    }
    return result;
}

std::vector<std::pair<std::string, std::string>> Storage::scan(const std::string& prefix, size_t limit) const {
    std::shared_lock lock(mutex_);
    std::vector<std::pair<std::string, std::string>> result;

    for (const auto& [k, v] : store_) {
        if (!isExpired(v) && k.starts_with(prefix)) {
            result.emplace_back(k, compressor_->decompress(v.compressed_value));
            if (result.size() >= limit) break;
        }
    }
    return result;
}

size_t Storage::countPrefix(const std::string& prefix) const {
    std::shared_lock lock(mutex_);
    size_t count = 0;

    for (const auto& [k, v] : store_) {
        if (!isExpired(v) && k.starts_with(prefix)) {
            count++;
        }
    }
    return count;
}

std::vector<std::pair<std::string, std::string>> Storage::range(
    const std::string& start, const std::string& end, size_t limit) const {
    std::shared_lock lock(mutex_);
    std::vector<std::pair<std::string, std::string>> result;

    for (const auto& [k, v] : store_) {
        if (!isExpired(v) && k >= start && k <= end) {
            result.emplace_back(k, compressor_->decompress(v.compressed_value));
        }
    }

    std::sort(result.begin(), result.end());
    if (result.size() > limit) {
        result.resize(limit);
    }
    return result;
}

std::vector<std::pair<std::string, std::string>> Storage::snapshot() const {
    std::shared_lock lock(mutex_);
    std::vector<std::pair<std::string, std::string>> result;
    result.reserve(store_.size());

    for (const auto& [k, v] : store_) {
        if (!isExpired(v)) {
            result.emplace_back(k, compressor_->decompress(v.compressed_value));
        }
    }
    return result;
}

} // namespace titan
