#pragma once

#include "titankv.hpp"
#include "compressor.hpp"
#include "utils.hpp"
#include <map>
#include <string>
#include <vector>
#include <mutex>
#include <shared_mutex>
#include <optional>
#include <memory>
#include <set>

namespace titan {

struct ValueEntry {
    std::vector<uint8_t> compressed_value;
    size_t raw_size = 0;
    int64_t expires_at = 0;
};

class SSTable;

class Storage {
public:
    Storage();

    void put(const std::string& key, const std::string& value, int64_t ttl_ms = 0);
    void putPrecompressed(const std::string& key, std::vector<uint8_t>&& compressed_value, int64_t ttl_ms = 0);
    void putPrecompressedBatch(std::vector<std::pair<std::string, std::vector<uint8_t>>>&& batch, size_t total_raw_size);

    std::optional<std::string> get(const std::string& key);
    std::vector<std::optional<std::string>> getBatch(const std::vector<std::string>& keys);
    bool del(const std::string& key);
    bool has(const std::string& key);
    void clear();

    std::vector<std::string> keys(size_t limit) const;
    std::vector<std::pair<std::string, std::string>> scan(const std::string& prefix, size_t limit) const;
    size_t countPrefix(const std::string& prefix) const;
    std::vector<std::pair<std::string, std::string>> range(const std::string& start, const std::string& end, size_t limit) const;

    std::vector<std::pair<std::string, std::vector<uint8_t>>> snapshot() const;

    StorageStats getStats() const;
    void setCompressionLevel(int level) { compression_level_ = level; }
    int getCompressionLevel() const { return compression_level_; }

    void setMaxMemoryBytes(size_t limit_bytes);
    void setSSTableBloomFilterEnabled(bool enabled);
    void setSpillDirectory(const std::string& spill_dir);
    void spillToDisk(const std::string& filepath);
    void loadSSTablesFromDirectory(const std::string& spill_dir, RecoveryMode mode = RecoveryMode::Permissive);
    void loadSSTablesFromFiles(const std::vector<std::string>& sst_files, RecoveryMode mode);
    void flushSpillState();

private:
    mutable std::shared_mutex mutex_;
    std::map<std::string, ValueEntry> store_;
    std::unique_ptr<Compressor> compressor_;
    std::vector<std::shared_ptr<SSTable>> sstables_;
    std::set<std::string> deleted_keys_;
    int compression_level_ = 3;

    size_t raw_bytes_ = 0;
    size_t compressed_bytes_ = 0;
    size_t max_memory_bytes_ = 0;
    bool sstable_bloom_enabled_ = true;
    std::string spill_dir_;
    uint64_t spill_seq_ = 0;

    int64_t now() const;
    bool isExpired(const ValueEntry& entry) const;
    void maybeSpillToDiskUnlocked();
    void spillToDiskUnlocked(const std::string& filepath);
    std::string nextSpillFilePathUnlocked();
    void clearSpillFilesUnlocked();
    std::optional<ValueEntry> findInSSTablesUnlocked(const std::string& key) const;
    std::map<std::string, std::string> materializeVisibleUnlocked() const;
};

} // namespace titan
