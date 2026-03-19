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
    size_t new_raw_size = value.size();
    size_t new_compressed_size = compressed.size();

    auto it = store_.find(key);
    if (it != store_.end()) {
        raw_bytes_ -= it->second.raw_size;
        compressed_bytes_ -= it->second.compressed_value.size();
    }

    raw_bytes_ += new_raw_size;
    compressed_bytes_ += new_compressed_size;

    int64_t expires = ttl_ms > 0 ? now() + ttl_ms : 0;
    store_[key] = {std::move(compressed), new_raw_size, expires};
}

void Storage::putPrecompressed(const std::string& key, std::vector<uint8_t>&& compressed_value, int64_t ttl_ms) {
    std::unique_lock lock(mutex_);

    size_t new_raw_size = Compressor::getDecompressedSize(compressed_value);
    size_t new_compressed_size = compressed_value.size();

    auto it = store_.find(key);
    if (it != store_.end()) {
        raw_bytes_ -= it->second.raw_size;
        compressed_bytes_ -= it->second.compressed_value.size();
    }

    raw_bytes_ += new_raw_size;
    compressed_bytes_ += new_compressed_size;

    int64_t expires = ttl_ms > 0 ? now() + ttl_ms : 0;
    store_[key] = {std::move(compressed_value), new_raw_size, expires};
}

void Storage::putPrecompressedBatch(std::vector<std::pair<std::string, std::vector<uint8_t>>>&& batch, size_t /*total_raw_size*/) {
    std::unique_lock lock(mutex_);
    for (auto& [key, compressed] : batch) {
        TITAN_ASSERT(!key.empty(), "key cannot be empty");

        size_t entry_raw = Compressor::getDecompressedSize(compressed);
        size_t entry_comp = compressed.size();

        auto it = store_.find(key);
        if (it != store_.end()) {
            raw_bytes_ -= it->second.raw_size;
            compressed_bytes_ -= it->second.compressed_value.size();
        }

        raw_bytes_ += entry_raw;
        compressed_bytes_ += entry_comp;
        store_[key] = {std::move(compressed), entry_raw};
    }
}

std::optional<std::string> Storage::get(const std::string& key) {
    std::unique_lock lock(mutex_);

    auto it = store_.find(key);
    if (it == store_.end()) return std::nullopt;
    if (isExpired(it->second)) {
        raw_bytes_ -= it->second.raw_size;
        compressed_bytes_ -= it->second.compressed_value.size();
        store_.erase(it);
        return std::nullopt;
    }

    return compressor_->decompress(it->second.compressed_value);
}

std::vector<std::optional<std::string>> Storage::getBatch(const std::vector<std::string>& keys) {
    std::unique_lock lock(mutex_);
    std::vector<std::optional<std::string>> results;
    results.reserve(keys.size());

    for (const auto& k : keys) {
        auto it = store_.find(k);
        if (it == store_.end()) {
            results.push_back(std::nullopt);
        } else if (isExpired(it->second)) {
            raw_bytes_ -= it->second.raw_size;
            compressed_bytes_ -= it->second.compressed_value.size();
            store_.erase(it);
            results.push_back(std::nullopt);
        } else {
            results.push_back(compressor_->decompress(it->second.compressed_value));
        }
    }
    return results;
}

bool Storage::del(const std::string& key) {
    std::unique_lock lock(mutex_);
    auto it = store_.find(key);
    if (it == store_.end()) return false;

    raw_bytes_ -= it->second.raw_size;
    compressed_bytes_ -= it->second.compressed_value.size();
    store_.erase(it);
    return true;
}

bool Storage::has(const std::string& key) {
    std::unique_lock lock(mutex_);
    auto it = store_.find(key);
    if (it == store_.end()) return false;
    if (isExpired(it->second)) {
        raw_bytes_ -= it->second.raw_size;
        compressed_bytes_ -= it->second.compressed_value.size();
        store_.erase(it);
        return false;
    }
    return true;
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

std::vector<std::pair<std::string, std::vector<uint8_t>>> Storage::snapshot() const {
    std::shared_lock lock(mutex_);
    std::vector<std::pair<std::string, std::vector<uint8_t>>> result;
    result.reserve(store_.size());

    for (const auto& [k, v] : store_) {
        if (!isExpired(v)) {
            result.emplace_back(k, v.compressed_value);
        }
    }
    return result;
}

} // namespace titan
