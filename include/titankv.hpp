#pragma once

#include <string>
#include <vector>
#include <optional>
#include <memory>
#include <filesystem>
#include <cstdint>

namespace titan {

struct StorageStats {
    size_t key_count = 0;
    size_t raw_bytes = 0;
    size_t compressed_bytes = 0;
};

class Storage;
class WAL;

class TitanEngine {
public:
    using KVPair = std::pair<std::string, std::string>;

    TitanEngine();
    explicit TitanEngine(const std::string& data_dir);
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
    void setCompressionLevel(int level);

    StorageStats getStats() const;

private:
    std::unique_ptr<Storage> storage_;
    std::unique_ptr<WAL> wal_;
    std::filesystem::path db_path_;

    void recover();
};

} // namespace titan
