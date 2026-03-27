#include "sstable.hpp"
#include "checksum.hpp"
#include <algorithm>
#include <stdexcept>
#include <array>

namespace titan {

namespace {
constexpr std::array<uint8_t, 8> kSstMagic{{'T', 'K', 'V', 'S', 'S', 'T', '3', '\n'}};
}

SSTable::SSTable(const std::string& filepath, bool bloom_enabled)
    : filepath_(filepath), bloom_enabled_(bloom_enabled) {
    loadIndex();
}

void SSTable::build(const std::string& filepath, const std::map<std::string, titan::ValueEntry>& memtable) {
    std::ofstream out(filepath, std::ios::binary | std::ios::trunc);
    if (!out.is_open()) {
        throw std::runtime_error("Failed to open SSTable for writing: " + filepath);
    }

    out.write(reinterpret_cast<const char*>(kSstMagic.data()), static_cast<std::streamsize>(kSstMagic.size()));

    std::vector<std::pair<std::string, uint64_t>> current_index;
    current_index.reserve(memtable.size());

    for (const auto& [key, entry] : memtable) {
        uint64_t current_offset = out.tellp();
        current_index.push_back({key, current_offset});

        uint32_t key_len = static_cast<uint32_t>(key.size());
        out.write(reinterpret_cast<const char*>(&key_len), sizeof(key_len));
        out.write(key.data(), static_cast<std::streamsize>(key_len));

        uint32_t val_len = static_cast<uint32_t>(entry.compressed_value.size());
        out.write(reinterpret_cast<const char*>(&val_len), sizeof(val_len));

        if (val_len > 0) {
            out.write(reinterpret_cast<const char*>(entry.compressed_value.data()), val_len);
        }

        uint64_t raw_size = static_cast<uint64_t>(entry.raw_size);
        out.write(reinterpret_cast<const char*>(&raw_size), sizeof(raw_size));

        out.write(reinterpret_cast<const char*>(&entry.expires_at), sizeof(entry.expires_at));

        uint32_t checksum = kFnv1a32Offset;
        checksum = fnv1a32Update(checksum, &key_len, sizeof(key_len));
        checksum = fnv1a32Update(checksum, key.data(), key.size());
        checksum = fnv1a32Update(checksum, &val_len, sizeof(val_len));
        if (val_len > 0) {
            checksum = fnv1a32Update(checksum, entry.compressed_value.data(), entry.compressed_value.size());
        }
        checksum = fnv1a32Update(checksum, &raw_size, sizeof(raw_size));
        checksum = fnv1a32Update(checksum, &entry.expires_at, sizeof(entry.expires_at));

        out.write(reinterpret_cast<const char*>(&checksum), sizeof(checksum));
    }

    uint64_t index_start_offset = out.tellp();

    uint32_t total_keys = static_cast<uint32_t>(current_index.size());
    out.write(reinterpret_cast<const char*>(&total_keys), sizeof(total_keys));

    for (const auto& [key, offset] : current_index) {
        uint16_t key_len = static_cast<uint16_t>(key.length());
        out.write(reinterpret_cast<const char*>(&key_len), sizeof(key_len));
        out.write(key.data(), key_len);
        out.write(reinterpret_cast<const char*>(&offset), sizeof(offset));
    }

    uint32_t index_checksum = kFnv1a32Offset;
    index_checksum = fnv1a32Update(index_checksum, &total_keys, sizeof(total_keys));
    for (const auto& [key, offset] : current_index) {
        const uint16_t key_len = static_cast<uint16_t>(key.size());
        index_checksum = fnv1a32Update(index_checksum, &key_len, sizeof(key_len));
        index_checksum = fnv1a32Update(index_checksum, key.data(), key.size());
        index_checksum = fnv1a32Update(index_checksum, &offset, sizeof(offset));
    }
    out.write(reinterpret_cast<const char*>(&index_checksum), sizeof(index_checksum));

    out.write(reinterpret_cast<const char*>(&index_start_offset), sizeof(index_start_offset));

    out.close();
}

bool SSTable::hasChecksummedHeader(std::ifstream& in) {
    in.seekg(0, std::ios::beg);
    std::array<uint8_t, kSstMagic.size()> header{};
    if (!in.read(reinterpret_cast<char*>(header.data()), static_cast<std::streamsize>(header.size()))) {
        in.clear();
        return false;
    }
    return header == kSstMagic;
}

uint32_t SSTable::hashKeyWithSeed(std::string_view key, uint32_t seed) {
    uint32_t hash = 2166136261u ^ seed;
    for (char ch : key) {
        hash ^= static_cast<uint8_t>(ch);
        hash *= 16777619u;
    }
    return hash;
}

void SSTable::buildFencePointers() {
    fence_pointers_.clear();
    if (index_.empty()) {
        return;
    }

    fence_pointers_.reserve((index_.size() / kFenceStride) + 1);
    for (size_t pos = 0; pos < index_.size(); pos += kFenceStride) {
        fence_pointers_.push_back({index_[pos].key, static_cast<uint32_t>(pos)});
    }
}

void SSTable::bloomInsert(std::string_view key) {
    if (bloom_bits_count_ == 0 || bloom_hash_count_ == 0) {
        return;
    }

    const uint32_t h1 = hashKeyWithSeed(key, 0x811C9DC5u);
    const uint32_t h2 = hashKeyWithSeed(key, 0x9E3779B9u) | 1u;

    for (uint32_t i = 0; i < bloom_hash_count_; ++i) {
        const uint32_t bit = (h1 + i * h2) % bloom_bits_count_;
        bloom_bits_[bit / 8] |= static_cast<uint8_t>(1u << (bit % 8));
    }
}

bool SSTable::bloomMayContain(std::string_view key) const {
    if (!bloom_enabled_ || bloom_bits_count_ == 0 || bloom_hash_count_ == 0) {
        return true;
    }

    const uint32_t h1 = hashKeyWithSeed(key, 0x811C9DC5u);
    const uint32_t h2 = hashKeyWithSeed(key, 0x9E3779B9u) | 1u;

    for (uint32_t i = 0; i < bloom_hash_count_; ++i) {
        const uint32_t bit = (h1 + i * h2) % bloom_bits_count_;
        if ((bloom_bits_[bit / 8] & static_cast<uint8_t>(1u << (bit % 8))) == 0) {
            return false;
        }
    }

    return true;
}

void SSTable::buildBloomFilter() {
    bloom_bits_.clear();
    bloom_bits_count_ = 0;
    bloom_hash_count_ = 0;

    if (!bloom_enabled_ || index_.empty()) {
        return;
    }

    const uint32_t estimated_bits = static_cast<uint32_t>(index_.size() * kBloomBitsPerKey);
    bloom_bits_count_ = std::max(kBloomMinBits, estimated_bits);
    bloom_hash_count_ = std::max<uint32_t>(1u, std::min<uint32_t>(8u, (kBloomBitsPerKey * 693u) / 1000u));
    bloom_bits_.assign((bloom_bits_count_ + 7) / 8, 0);

    for (const auto& entry : index_) {
        bloomInsert(entry.key);
    }
}

void SSTable::rebuildReadPathStructures() {
    if (index_.empty()) {
        min_key_.clear();
        max_key_.clear();
        fence_pointers_.clear();
        bloom_bits_.clear();
        bloom_bits_count_ = 0;
        bloom_hash_count_ = 0;
        return;
    }

    min_key_ = index_.front().key;
    max_key_ = index_.back().key;

    buildFencePointers();
    buildBloomFilter();
}

void SSTable::loadLegacyIndex(std::ifstream& in) {
    in.seekg(-8, std::ios::end);
    uint64_t index_offset = 0;
    in.read(reinterpret_cast<char*>(&index_offset), sizeof(index_offset));

    in.seekg(index_offset, std::ios::beg);

    uint32_t total_keys = 0;
    in.read(reinterpret_cast<char*>(&total_keys), sizeof(total_keys));

    for (uint32_t i = 0; i < total_keys; ++i) {
        uint16_t key_len = 0;
        in.read(reinterpret_cast<char*>(&key_len), sizeof(key_len));

        std::string key(key_len, '\0');
        in.read(&key[0], key_len);

        uint64_t offset = 0;
        in.read(reinterpret_cast<char*>(&offset), sizeof(offset));

        index_.push_back({std::move(key), offset});
    }
}

void SSTable::loadChecksummedIndex(std::ifstream& in) {
    in.seekg(-8, std::ios::end);
    uint64_t index_offset = 0;
    in.read(reinterpret_cast<char*>(&index_offset), sizeof(index_offset));

    in.seekg(index_offset, std::ios::beg);

    uint32_t total_keys = 0;
    in.read(reinterpret_cast<char*>(&total_keys), sizeof(total_keys));

    uint32_t checksum = kFnv1a32Offset;
    checksum = fnv1a32Update(checksum, &total_keys, sizeof(total_keys));

    for (uint32_t i = 0; i < total_keys; ++i) {
        uint16_t key_len = 0;
        in.read(reinterpret_cast<char*>(&key_len), sizeof(key_len));

        std::string key(key_len, '\0');
        in.read(&key[0], key_len);

        uint64_t offset = 0;
        in.read(reinterpret_cast<char*>(&offset), sizeof(offset));

        checksum = fnv1a32Update(checksum, &key_len, sizeof(key_len));
        checksum = fnv1a32Update(checksum, key.data(), key.size());
        checksum = fnv1a32Update(checksum, &offset, sizeof(offset));

        index_.push_back({std::move(key), offset});
    }

    uint32_t stored_checksum = 0;
    in.read(reinterpret_cast<char*>(&stored_checksum), sizeof(stored_checksum));
    if (stored_checksum != checksum) {
        throw std::runtime_error("SSTable index checksum mismatch: " + filepath_);
    }
}

void SSTable::loadIndex() {
    std::ifstream in(filepath_, std::ios::binary);
    if (!in.is_open()) {
        throw std::runtime_error("SSTable failed to open: " + filepath_);
    }

    checksummed_format_ = hasChecksummedHeader(in);
    if (checksummed_format_) {
        loadChecksummedIndex(in);
    } else {
        loadLegacyIndex(in);
    }

    rebuildReadPathStructures();
}

std::optional<uint64_t> SSTable::findRecordOffset(const std::string& key) const {
    if (index_.empty()) {
        return std::nullopt;
    }

    if (key < min_key_ || key > max_key_) {
        return std::nullopt;
    }

    if (bloom_enabled_ && !bloomMayContain(key)) {
        return std::nullopt;
    }

    size_t lo = 0;
    size_t hi = index_.size();
    if (!fence_pointers_.empty()) {
        auto fence_it = std::upper_bound(
            fence_pointers_.begin(),
            fence_pointers_.end(),
            key,
            [](const std::string& target, const FencePointer& fence) {
                return target < fence.key;
            });

        if (fence_it == fence_pointers_.begin()) {
            lo = 0;
        } else {
            lo = static_cast<size_t>((fence_it - 1)->position);
        }

        if (fence_it == fence_pointers_.end()) {
            hi = index_.size();
        } else {
            hi = static_cast<size_t>(fence_it->position);
        }

        if (hi <= lo) {
            hi = std::min(index_.size(), lo + static_cast<size_t>(kFenceStride) + 1);
        }
    }

    auto begin_it = index_.begin() + static_cast<std::ptrdiff_t>(lo);
    auto end_it = index_.begin() + static_cast<std::ptrdiff_t>(hi);
    auto it = std::lower_bound(
        begin_it,
        end_it,
        key,
        [](const IndexEntry& entry, const std::string& target) {
            return entry.key < target;
        });

    if (it == end_it || it->key != key) {
        return std::nullopt;
    }

    return it->offset;
}

std::optional<titan::ValueEntry> SSTable::get(const std::string& key) const {
    const auto offset = findRecordOffset(key);
    if (!offset.has_value()) {
        return std::nullopt;
    }

    std::ifstream in(filepath_, std::ios::binary);
    if (!in.is_open()) return std::nullopt;

    in.seekg(static_cast<std::streamoff>(*offset), std::ios::beg);

    titan::ValueEntry entry;

    if (checksummed_format_) {
        uint32_t key_len = 0;
        if (!in.read(reinterpret_cast<char*>(&key_len), sizeof(key_len))) {
            return std::nullopt;
        }

        std::string key_in_file(key_len, '\0');
        if (!in.read(&key_in_file[0], static_cast<std::streamsize>(key_len))) {
            return std::nullopt;
        }
        if (key_in_file != key) {
            throw std::runtime_error("SSTable key mismatch while reading: " + filepath_);
        }
    }

    uint32_t val_len = 0;
    if (!in.read(reinterpret_cast<char*>(&val_len), sizeof(val_len))) {
        return std::nullopt;
    }

    entry.compressed_value.resize(val_len);
    if (val_len > 0) {
        if (!in.read(reinterpret_cast<char*>(entry.compressed_value.data()), static_cast<std::streamsize>(val_len))) {
            return std::nullopt;
        }
    }

    uint64_t raw_size = 0;
    if (!in.read(reinterpret_cast<char*>(&raw_size), sizeof(raw_size))) {
        return std::nullopt;
    }
    entry.raw_size = raw_size;

    if (!in.read(reinterpret_cast<char*>(&entry.expires_at), sizeof(entry.expires_at))) {
        return std::nullopt;
    }

    if (checksummed_format_) {
        const uint32_t key_len = static_cast<uint32_t>(key.size());
        uint32_t actual_checksum = kFnv1a32Offset;
        actual_checksum = fnv1a32Update(actual_checksum, &key_len, sizeof(key_len));
        actual_checksum = fnv1a32Update(actual_checksum, key.data(), key.size());
        actual_checksum = fnv1a32Update(actual_checksum, &val_len, sizeof(val_len));
        if (val_len > 0) {
            actual_checksum = fnv1a32Update(actual_checksum, entry.compressed_value.data(), entry.compressed_value.size());
        }
        actual_checksum = fnv1a32Update(actual_checksum, &raw_size, sizeof(raw_size));
        actual_checksum = fnv1a32Update(actual_checksum, &entry.expires_at, sizeof(entry.expires_at));

        uint32_t stored_checksum = 0;
        if (!in.read(reinterpret_cast<char*>(&stored_checksum), sizeof(stored_checksum))) {
            return std::nullopt;
        }
        if (stored_checksum != actual_checksum) {
            throw std::runtime_error("SSTable record checksum mismatch: " + filepath_ + " key=" + key);
        }
    }

    return entry;
}

std::vector<std::string> SSTable::keys() const {
    std::vector<std::string> out;
    out.reserve(index_.size());
    for (const auto& entry : index_) {
        out.push_back(entry.key);
    }
    return out;
}

} // namespace titan
