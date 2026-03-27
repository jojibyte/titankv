#include "manifest.hpp"

#include <algorithm>
#include <fstream>
#include <sstream>
#include <system_error>

namespace titan {

namespace {

std::vector<std::string> splitTabs(const std::string& line) {
    std::vector<std::string> fields;
    std::stringstream ss(line);
    std::string part;
    while (std::getline(ss, part, '\t')) {
        fields.push_back(part);
    }
    return fields;
}

} // namespace

ManifestStore::ManifestStore(const std::filesystem::path& db_dir)
    : db_dir_(db_dir), manifest_path_(db_dir / "titan.manifest") {}

bool ManifestStore::load(RecoveryManifest& out_manifest) const {
    out_manifest = RecoveryManifest{};

    std::ifstream in(manifest_path_);
    if (!in.is_open()) {
        return false;
    }

    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) {
            continue;
        }

        auto fields = splitTabs(line);
        if (fields.empty()) {
            continue;
        }

        if (fields[0] == "version" && fields.size() >= 2) {
            out_manifest.version = std::stoi(fields[1]);
            continue;
        }
        if (fields[0] == "updated_at_ms" && fields.size() >= 2) {
            out_manifest.updated_at_ms = std::stoll(fields[1]);
            continue;
        }
        if (fields[0] == "wal_file" && fields.size() >= 2) {
            out_manifest.wal_file = fields[1];
            continue;
        }
        if (fields[0] == "wal_format" && fields.size() >= 2) {
            out_manifest.wal_format = fields[1];
            continue;
        }
        if (fields[0] == "wal_size_bytes" && fields.size() >= 2) {
            out_manifest.wal_size_bytes = static_cast<uint64_t>(std::stoull(fields[1]));
            continue;
        }
        if (fields[0] == "sst" && fields.size() >= 4) {
            SegmentManifestEntry entry;
            entry.relative_path = fields[1];
            entry.size_bytes = static_cast<uint64_t>(std::stoull(fields[2]));
            entry.mtime_ms = std::stoll(fields[3]);
            out_manifest.sstables.push_back(std::move(entry));
            continue;
        }
    }

    return true;
}

void ManifestStore::save(const RecoveryManifest& manifest) const {
    std::filesystem::create_directories(db_dir_);

    auto temp_path = manifest_path_;
    temp_path += ".tmp";

    std::ofstream out(temp_path, std::ios::trunc);
    if (!out.is_open()) {
        throw std::runtime_error("failed to write manifest: " + temp_path.string());
    }

    out << "version\t" << manifest.version << '\n';
    out << "updated_at_ms\t" << manifest.updated_at_ms << '\n';
    out << "wal_file\t" << manifest.wal_file << '\n';
    out << "wal_format\t" << manifest.wal_format << '\n';
    out << "wal_size_bytes\t" << manifest.wal_size_bytes << '\n';

    std::vector<SegmentManifestEntry> sorted = manifest.sstables;
    std::sort(sorted.begin(), sorted.end(), [](const auto& a, const auto& b) {
        return a.relative_path < b.relative_path;
    });

    for (const auto& segment : sorted) {
        out << "sst\t"
            << segment.relative_path << '\t'
            << segment.size_bytes << '\t'
            << segment.mtime_ms << '\n';
    }

    out.flush();
    out.close();

    std::error_code ec;
    std::filesystem::rename(temp_path, manifest_path_, ec);
    if (ec) {
        std::filesystem::remove(manifest_path_, ec);
        ec.clear();
        std::filesystem::rename(temp_path, manifest_path_, ec);
        if (ec) {
            throw std::runtime_error("failed to finalize manifest write: " + manifest_path_.string());
        }
    }
}

} // namespace titan
