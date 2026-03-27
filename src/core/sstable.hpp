#pragma once

#include <string>
#include <vector>
#include <optional>
#include <cstdint>
#include <fstream>
#include <array>
#include <string_view>
#include "storage.hpp"

namespace titan {

class SSTable {
public:
    explicit SSTable(const std::string& filepath, bool bloom_enabled = true);

    static void build(const std::string& filepath, const std::map<std::string, titan::ValueEntry>& memtable);

    std::optional<titan::ValueEntry> get(const std::string& key) const;
    std::vector<std::string> keys() const;

    std::string getFilePath() const { return filepath_; }

    size_t size() const { return index_.size(); }

private:
    struct IndexEntry {
        std::string key;
        uint64_t offset = 0;
    };

    struct FencePointer {
        std::string key;
        uint32_t position = 0;
    };

    std::string filepath_;
    bool checksummed_format_ = false;
    bool bloom_enabled_ = true;

    std::vector<IndexEntry> index_;
    std::vector<FencePointer> fence_pointers_;
    std::string min_key_;
    std::string max_key_;

    uint32_t bloom_bits_count_ = 0;
    uint32_t bloom_hash_count_ = 0;
    std::vector<uint8_t> bloom_bits_;

    static constexpr uint32_t kFenceStride = 64;
    static constexpr uint32_t kBloomBitsPerKey = 10;
    static constexpr uint32_t kBloomMinBits = 1024;

    static bool hasChecksummedHeader(std::ifstream& in);
    static uint32_t hashKeyWithSeed(std::string_view key, uint32_t seed);

    void rebuildReadPathStructures();
    void buildFencePointers();
    void buildBloomFilter();
    void bloomInsert(std::string_view key);
    bool bloomMayContain(std::string_view key) const;

    void loadIndex();
    void loadLegacyIndex(std::ifstream& in);
    void loadChecksummedIndex(std::ifstream& in);
    std::optional<uint64_t> findRecordOffset(const std::string& key) const;
};

} // namespace titan
