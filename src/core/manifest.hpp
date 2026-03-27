#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace titan {

struct SegmentManifestEntry {
    std::string relative_path;
    uint64_t size_bytes = 0;
    int64_t mtime_ms = 0;
};

struct RecoveryManifest {
    int version = 1;
    int64_t updated_at_ms = 0;
    std::string wal_file;
    std::string wal_format;
    uint64_t wal_size_bytes = 0;
    std::vector<SegmentManifestEntry> sstables;
};

class ManifestStore {
public:
    explicit ManifestStore(const std::filesystem::path& db_dir);

    bool load(RecoveryManifest& out_manifest) const;
    void save(const RecoveryManifest& manifest) const;

private:
    std::filesystem::path db_dir_;
    std::filesystem::path manifest_path_;
};

} // namespace titan
