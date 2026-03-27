#pragma once

#include <string>
#include <vector>
#include <optional>
#include <memory>
#include <filesystem>
#include <cstdint>
#include <thread>
#include <atomic>

namespace titan {

enum class RecoveryMode : uint8_t {
    Permissive = 0,
    Strict = 1
};

struct StorageStats {
    size_t key_count = 0;
    size_t raw_bytes = 0;
    size_t compressed_bytes = 0;
    size_t wal_size_bytes = 0;
    size_t logical_write_bytes = 0;
    size_t physical_write_bytes = 0;
    size_t compaction_count = 0;
    size_t auto_compaction_count = 0;
    double write_amplification = 0.0;
    double space_amplification = 0.0;
};

struct CompactionPolicy {
    bool auto_compact = false;
    size_t min_ops = 2000;
    double tombstone_ratio = 0.35;
    size_t min_wal_bytes = 4 * 1024 * 1024;
};

class Storage;
class WAL;

class TitanEngine {
public:
    using KVPair = std::pair<std::string, std::string>;

    TitanEngine();
    explicit TitanEngine(
        const std::string& data_dir,
        RecoveryMode recovery_mode = RecoveryMode::Permissive,
        bool sstable_bloom_enabled = true);
    ~TitanEngine();

    TitanEngine(const TitanEngine&) = delete;
    TitanEngine& operator=(const TitanEngine&) = delete;

    void put(const std::string& key, const std::string& value, int64_t ttl_ms = 0);
    std::optional<std::string> get(const std::string& key);
    bool del(const std::string& key);
    bool has(const std::string& key);
    size_t size() const;
    void clear();

    int64_t incr(const std::string& key, int64_t delta = 1);
    int64_t decr(const std::string& key, int64_t delta = 1);

    std::vector<std::string> keys(size_t limit = 1000) const;
    std::vector<KVPair> scan(const std::string& prefix, size_t limit = 1000) const;
    std::vector<KVPair> range(const std::string& start, const std::string& end, size_t limit = 1000) const;
    size_t countPrefix(const std::string& prefix) const;

    void putBatch(const std::vector<KVPair>& pairs);
    std::vector<std::optional<std::string>> getBatch(const std::vector<std::string>& keys);

    void flush();
    void compact();
    void close();
    void setCompressionLevel(int level);
    void setMaxMemoryBytes(size_t limit_bytes);
    void setSSTableBloomFilterEnabled(bool enabled);
    void setAutoCompactEnabled(bool enabled);
    void setCompactionPolicy(size_t min_ops, double tombstone_ratio, size_t min_wal_bytes);

    StorageStats getStats() const;

private:
    std::unique_ptr<Storage> storage_;
    std::unique_ptr<WAL> wal_;
    std::filesystem::path db_path_;
    RecoveryMode recovery_mode_ = RecoveryMode::Permissive;
    CompactionPolicy compaction_policy_{};
    std::atomic<size_t> wal_put_ops_{0};
    std::atomic<size_t> wal_del_ops_{0};
    std::atomic<size_t> wal_bytes_since_compact_{0};
    std::atomic<size_t> logical_write_bytes_total_{0};
    std::atomic<size_t> physical_write_bytes_total_{0};
    std::atomic<size_t> compaction_count_total_{0};
    std::atomic<size_t> auto_compaction_count_total_{0};
    std::atomic<bool> compact_in_progress_{false};
    std::thread compaction_thread_;

    void recover();
    void writeRecoveryManifestSnapshot();
    void maybeAutoCompact();
    void trackWalActivity(size_t put_ops, size_t del_ops, size_t estimated_bytes);
    void resetCompactionCountersFromWal();
    void compactInternal(bool auto_triggered = false);
};

} // namespace titan
