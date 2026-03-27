#include "storage.hpp"
#include "sstable.hpp"
#include <chrono>
#include <algorithm>
#include <cstring>
#include <filesystem>
#include <limits>

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

void Storage::setMaxMemoryBytes(size_t limit_bytes) {
    std::unique_lock lock(mutex_);
    max_memory_bytes_ = limit_bytes;
    maybeSpillToDiskUnlocked();
}

void Storage::setSSTableBloomFilterEnabled(bool enabled) {
    std::unique_lock lock(mutex_);
    sstable_bloom_enabled_ = enabled;
}

void Storage::setSpillDirectory(const std::string& spill_dir) {
    std::unique_lock lock(mutex_);
    spill_dir_ = spill_dir;
    if (!spill_dir_.empty()) {
        std::filesystem::create_directories(spill_dir_);
    }
}

void Storage::loadSSTablesFromDirectory(const std::string& spill_dir, RecoveryMode mode) {
    std::unique_lock lock(mutex_);
    spill_dir_ = spill_dir;
    if (spill_dir_.empty()) return;

    std::filesystem::create_directories(spill_dir_);

    std::vector<std::filesystem::path> files;
    for (const auto& entry : std::filesystem::directory_iterator(spill_dir_)) {
        if (!entry.is_regular_file()) continue;
        if (entry.path().extension() != ".sst") continue;
        files.push_back(entry.path());
    }

    std::sort(files.begin(), files.end(), [](const auto& a, const auto& b) {
        return a.filename().string() < b.filename().string();
    });

    sstables_.clear();
    spill_seq_ = 0;

    for (const auto& filepath : files) {
        try {
            sstables_.push_back(std::make_shared<SSTable>(filepath.string(), sstable_bloom_enabled_));
        } catch (...) {
            if (mode == RecoveryMode::Strict) {
                throw;
            }
        }
    }

    spill_seq_ = sstables_.size();
}

void Storage::loadSSTablesFromFiles(const std::vector<std::string>& sst_files, RecoveryMode mode) {
    std::unique_lock lock(mutex_);

    sstables_.clear();
    spill_seq_ = 0;

    for (const auto& file : sst_files) {
        const std::filesystem::path filepath(file);
        if (!std::filesystem::exists(filepath) || !std::filesystem::is_regular_file(filepath)) {
            if (mode == RecoveryMode::Strict) {
                throw std::runtime_error("missing SSTable referenced by manifest: " + filepath.string());
            }
            continue;
        }

        try {
            sstables_.push_back(std::make_shared<SSTable>(filepath.string(), sstable_bloom_enabled_));
        } catch (...) {
            if (mode == RecoveryMode::Strict) {
                throw;
            }
        }
    }

    spill_seq_ = sstables_.size();
}

void Storage::flushSpillState() {
    std::unique_lock lock(mutex_);
    if (spill_dir_.empty()) return;
    if (store_.empty()) return;
    if (max_memory_bytes_ == 0 && sstables_.empty()) return;

    spillToDiskUnlocked(nextSpillFilePathUnlocked());
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
    deleted_keys_.erase(key);
    maybeSpillToDiskUnlocked();
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
    deleted_keys_.erase(key);
    maybeSpillToDiskUnlocked();
}

void Storage::putPrecompressedBatch(std::vector<std::pair<std::string, std::vector<uint8_t>>>&& batch, size_t total_raw_size) {
    std::unique_lock lock(mutex_);
    (void)total_raw_size;

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
        deleted_keys_.erase(key);
    }

    maybeSpillToDiskUnlocked();
}

void Storage::spillToDisk(const std::string& filepath) {
    std::unique_lock lock(mutex_);
    spillToDiskUnlocked(filepath);
}

void Storage::spillToDiskUnlocked(const std::string& filepath) {
    if (store_.empty()) return;
    if (filepath.empty()) return;

    auto target_path = std::filesystem::path(filepath);
    auto parent = target_path.parent_path();
    if (!parent.empty()) {
        std::filesystem::create_directories(parent);
    }

    SSTable::build(filepath, store_);
    sstables_.push_back(std::make_shared<SSTable>(filepath, sstable_bloom_enabled_));

    store_.clear();
    raw_bytes_ = 0;
    compressed_bytes_ = 0;
}

void Storage::maybeSpillToDiskUnlocked() {
    if (max_memory_bytes_ == 0) return;
    if (raw_bytes_ <= max_memory_bytes_) return;
    if (spill_dir_.empty()) return;

    spillToDiskUnlocked(nextSpillFilePathUnlocked());
}

std::string Storage::nextSpillFilePathUnlocked() {
    if (spill_dir_.empty()) return "";

    const uint64_t seq = spill_seq_++;
    const std::string filename = "sst-" + std::to_string(now()) + "-" + std::to_string(seq) + ".sst";
    return (std::filesystem::path(spill_dir_) / filename).string();
}

void Storage::clearSpillFilesUnlocked() {
    if (spill_dir_.empty()) return;

    std::error_code ec;
    if (!std::filesystem::exists(spill_dir_, ec)) return;

    for (const auto& entry : std::filesystem::directory_iterator(spill_dir_, ec)) {
        if (ec) break;
        if (!entry.is_regular_file()) continue;
        if (entry.path().extension() != ".sst") continue;
        std::filesystem::remove(entry.path(), ec);
        ec.clear();
    }
}

std::optional<ValueEntry> Storage::findInSSTablesUnlocked(const std::string& key) const {
    for (auto it = sstables_.rbegin(); it != sstables_.rend(); ++it) {
        auto entry = (*it)->get(key);
        if (entry.has_value()) {
            return entry;
        }
    }
    return std::nullopt;
}

std::map<std::string, std::string> Storage::materializeVisibleUnlocked() const {
    std::map<std::string, std::string> merged;

    for (const auto& table : sstables_) {
        auto table_keys = table->keys();
        for (const auto& key : table_keys) {
            if (deleted_keys_.find(key) != deleted_keys_.end()) continue;

            auto entry = table->get(key);
            if (!entry.has_value()) continue;

            if (isExpired(*entry)) {
                merged.erase(key);
                continue;
            }

            merged[key] = compressor_->decompress(entry->compressed_value);
        }
    }

    for (const auto& [key, entry] : store_) {
        if (deleted_keys_.find(key) != deleted_keys_.end()) continue;
        if (isExpired(entry)) {
            merged.erase(key);
            continue;
        }
        merged[key] = compressor_->decompress(entry.compressed_value);
    }

    for (const auto& key : deleted_keys_) {
        merged.erase(key);
    }

    return merged;
}

std::optional<std::string> Storage::get(const std::string& key) {
    std::unique_lock lock(mutex_);

    if (deleted_keys_.find(key) != deleted_keys_.end()) return std::nullopt;

    auto it = store_.find(key);
    if (it != store_.end()) {
        if (isExpired(it->second)) {
            raw_bytes_ -= it->second.raw_size;
            compressed_bytes_ -= it->second.compressed_value.size();
            store_.erase(it);
            return std::nullopt;
        }

        return compressor_->decompress(it->second.compressed_value);
    }

    auto sst_entry = findInSSTablesUnlocked(key);
    if (!sst_entry.has_value()) return std::nullopt;
    if (isExpired(*sst_entry)) return std::nullopt;

    return compressor_->decompress(sst_entry->compressed_value);
}

std::vector<std::optional<std::string>> Storage::getBatch(const std::vector<std::string>& keys) {
    std::unique_lock lock(mutex_);
    std::vector<std::optional<std::string>> results;
    results.reserve(keys.size());

    for (const auto& k : keys) {
        if (deleted_keys_.find(k) != deleted_keys_.end()) {
            results.push_back(std::nullopt);
            continue;
        }

        auto it = store_.find(k);
        if (it != store_.end()) {
            if (isExpired(it->second)) {
                raw_bytes_ -= it->second.raw_size;
                compressed_bytes_ -= it->second.compressed_value.size();
                store_.erase(it);
                results.push_back(std::nullopt);
                continue;
            }

            results.push_back(compressor_->decompress(it->second.compressed_value));
            continue;
        }

        auto sst_entry = findInSSTablesUnlocked(k);
        if (!sst_entry.has_value() || isExpired(*sst_entry)) {
            results.push_back(std::nullopt);
            continue;
        }

        results.push_back(compressor_->decompress(sst_entry->compressed_value));
    }

    return results;
}

bool Storage::del(const std::string& key) {
    std::unique_lock lock(mutex_);
    bool deleted = false;

    auto it = store_.find(key);
    if (it != store_.end()) {
        raw_bytes_ -= it->second.raw_size;
        compressed_bytes_ -= it->second.compressed_value.size();
        store_.erase(it);
        deleted = true;
    }

    if (!deleted && findInSSTablesUnlocked(key).has_value()) {
        deleted = true;
    }

    if (deleted) {
        deleted_keys_.insert(key);
    }

    return deleted;
}

bool Storage::has(const std::string& key) {
    std::unique_lock lock(mutex_);

    if (deleted_keys_.find(key) != deleted_keys_.end()) return false;

    auto it = store_.find(key);
    if (it != store_.end()) {
        if (isExpired(it->second)) {
            raw_bytes_ -= it->second.raw_size;
            compressed_bytes_ -= it->second.compressed_value.size();
            store_.erase(it);
            return false;
        }

        return true;
    }

    auto sst_entry = findInSSTablesUnlocked(key);
    if (!sst_entry.has_value()) return false;
    if (isExpired(*sst_entry)) return false;

    return true;
}

void Storage::clear() {
    std::unique_lock lock(mutex_);
    clearSpillFilesUnlocked();
    store_.clear();
    sstables_.clear();
    deleted_keys_.clear();
    raw_bytes_ = 0;
    compressed_bytes_ = 0;
    spill_seq_ = 0;
}

StorageStats Storage::getStats() const {
    std::shared_lock lock(mutex_);
    StorageStats s;

    if (sstables_.empty() && deleted_keys_.empty()) {
        s.key_count = store_.size();
        s.raw_bytes = raw_bytes_;
        s.compressed_bytes = compressed_bytes_;
        return s;
    }

    const auto merged = materializeVisibleUnlocked();
    s.key_count = merged.size();

    size_t total_raw = 0;
    size_t total_compressed = 0;
    for (const auto& [_, value] : merged) {
        total_raw += value.size();
        total_compressed += compressor_->compress(value, compression_level_).size();
    }

    s.raw_bytes = total_raw;
    s.compressed_bytes = total_compressed;
    return s;
}

std::vector<std::string> Storage::keys(size_t limit) const {
    std::shared_lock lock(mutex_);
    const auto merged = materializeVisibleUnlocked();

    std::vector<std::string> result;
    result.reserve(std::min(limit, merged.size()));

    for (const auto& [k, _] : merged) {
        result.push_back(k);
        if (result.size() >= limit) break;
    }

    return result;
}

std::vector<std::pair<std::string, std::string>> Storage::scan(const std::string& prefix, size_t limit) const {
    std::shared_lock lock(mutex_);
    const auto merged = materializeVisibleUnlocked();

    std::vector<std::pair<std::string, std::string>> result;

    auto it = prefix.empty() ? merged.begin() : merged.lower_bound(prefix);
    for (; it != merged.end() && result.size() < limit; ++it) {
        if (!prefix.empty() && it->first.compare(0, prefix.size(), prefix) != 0) break;
        result.emplace_back(it->first, it->second);
    }

    return result;
}

size_t Storage::countPrefix(const std::string& prefix) const {
    std::shared_lock lock(mutex_);
    const auto merged = materializeVisibleUnlocked();

    size_t count = 0;

    auto it = prefix.empty() ? merged.begin() : merged.lower_bound(prefix);
    for (; it != merged.end(); ++it) {
        if (!prefix.empty() && it->first.compare(0, prefix.size(), prefix) != 0) break;
        count++;
    }

    return count;
}

std::vector<std::pair<std::string, std::string>> Storage::range(
    const std::string& start, const std::string& end, size_t limit) const {
    std::shared_lock lock(mutex_);
    const auto merged = materializeVisibleUnlocked();

    std::vector<std::pair<std::string, std::string>> result;

    for (auto it = merged.lower_bound(start); it != merged.end() && it->first <= end && result.size() < limit; ++it) {
        result.emplace_back(it->first, it->second);
    }

    return result;
}

std::vector<std::pair<std::string, std::vector<uint8_t>>> Storage::snapshot() const {
    std::shared_lock lock(mutex_);

    if (sstables_.empty() && deleted_keys_.empty()) {
        std::vector<std::pair<std::string, std::vector<uint8_t>>> result;
        result.reserve(store_.size());

        for (const auto& [k, v] : store_) {
            if (!isExpired(v)) {
                result.emplace_back(k, v.compressed_value);
            }
        }
        return result;
    }

    const auto merged = materializeVisibleUnlocked();
    std::vector<std::pair<std::string, std::vector<uint8_t>>> result;
    result.reserve(merged.size());

    for (const auto& [k, value] : merged) {
        result.emplace_back(k, compressor_->compress(value, compression_level_));
    }

    return result;
}

} // namespace titan
