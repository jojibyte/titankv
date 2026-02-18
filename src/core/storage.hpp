#pragma once

#include "titankv.hpp"
#include "compressor.hpp"
#include "utils.hpp"
#include <unordered_map>
#include <string>
#include <vector>
#include <mutex>
#include <shared_mutex>
#include <optional>
#include <memory>

namespace titan {

struct ValueEntry {
    std::vector<uint8_t> compressed_value;
    int64_t expires_at = 0;
};

class Storage {
public:
    Storage();

    void put(const std::string& key, const std::string& value, int64_t ttl_ms = 0);
    void putPrecompressed(const std::string& key, std::vector<uint8_t>&& compressed_value, int64_t ttl_ms = 0);
    void putPrecompressedBatch(std::vector<std::pair<std::string, std::vector<uint8_t>>>&& batch, size_t total_raw_size);

    std::optional<std::string> get(const std::string& key);
    bool del(const std::string& key);
    bool has(const std::string& key);
    void clear();

    std::vector<std::string> keys(size_t limit) const;
    std::vector<std::pair<std::string, std::string>> scan(const std::string& prefix, size_t limit) const;
    std::vector<std::pair<std::string, std::string>> range(const std::string& start, const std::string& end, size_t limit) const;

    std::vector<std::pair<std::string, std::string>> snapshot() const;

    StorageStats getStats() const;
    void setCompressionLevel(int level) { compression_level_ = level; }
    int getCompressionLevel() const { return compression_level_; }

private:
    mutable std::shared_mutex mutex_;
    std::unordered_map<std::string, ValueEntry> store_;
    std::unique_ptr<Compressor> compressor_;
    int compression_level_ = 3;

    size_t raw_bytes_ = 0;
    size_t compressed_bytes_ = 0;

    int64_t now() const;
    bool isExpired(const ValueEntry& entry) const;
};

} // namespace titan
