#include "titankv.hpp"
#include "storage.hpp"
#include "wal.hpp"
#include "manifest.hpp"
#include "compressor.hpp"
#include "utils.hpp"
#include <algorithm>
#include <chrono>

namespace {

int64_t unixNowMs() {
    using namespace std::chrono;
    return duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
}

}

namespace titan {

TitanEngine::TitanEngine() : TitanEngine("", RecoveryMode::Permissive, true) {}

TitanEngine::TitanEngine(const std::string& data_dir, RecoveryMode recovery_mode, bool sstable_bloom_enabled)
    : recovery_mode_(recovery_mode) {
    storage_ = std::make_unique<Storage>();
    storage_->setSSTableBloomFilterEnabled(sstable_bloom_enabled);
    if (!data_dir.empty()) {
        db_path_ = std::filesystem::path(data_dir);
        storage_->setSpillDirectory((db_path_ / "sstables").string());
        wal_ = std::make_unique<WAL>(db_path_);
        recover();
    }
}

TitanEngine::~TitanEngine() {
    try {
        close();
    } catch (...) {
    }
}

void TitanEngine::trackWalActivity(size_t put_ops, size_t del_ops, size_t estimated_bytes) {
    wal_put_ops_.fetch_add(put_ops);
    wal_del_ops_.fetch_add(del_ops);
    wal_bytes_since_compact_.fetch_add(estimated_bytes);
    physical_write_bytes_total_.fetch_add(estimated_bytes);
}

void TitanEngine::resetCompactionCountersFromWal() {
    wal_put_ops_.store(0);
    wal_del_ops_.store(0);
    wal_bytes_since_compact_.store(0);

    if (!wal_) {
        return;
    }

    std::error_code ec;
    if (std::filesystem::exists(wal_->path(), ec)) {
        wal_bytes_since_compact_.store(std::filesystem::file_size(wal_->path(), ec));
        if (ec) {
            ec.clear();
            wal_bytes_since_compact_.store(0);
        }
    }

    auto entries = wal_->recover(RecoveryMode::Permissive);
    for (const auto& entry : entries) {
        if (entry.op == WalOp::PUT) {
            wal_put_ops_.fetch_add(1);
        } else if (entry.op == WalOp::DEL) {
            wal_del_ops_.fetch_add(1);
        }
    }
}

void TitanEngine::maybeAutoCompact() {
    if (!wal_ || compact_in_progress_.load() || !compaction_policy_.auto_compact) {
        return;
    }

    const size_t put_ops = wal_put_ops_.load();
    const size_t del_ops = wal_del_ops_.load();
    const size_t total_ops = put_ops + del_ops;
    if (total_ops < compaction_policy_.min_ops) {
        return;
    }

    if (wal_bytes_since_compact_.load() < compaction_policy_.min_wal_bytes) {
        return;
    }

    const double tombstone_ratio = total_ops == 0
        ? 0.0
        : static_cast<double>(del_ops) / static_cast<double>(total_ops);

    if (tombstone_ratio < compaction_policy_.tombstone_ratio) {
        return;
    }

    bool expected = false;
    if (!compact_in_progress_.compare_exchange_strong(expected, true)) {
        return;
    }

    if (compaction_thread_.joinable()) {
        compaction_thread_.join();
    }

    compaction_thread_ = std::thread([this]() {
        try {
            compactInternal(true);
        } catch (...) {
        }
        compact_in_progress_.store(false);
    });
}

void TitanEngine::recover() {
    if (!wal_) return;

    RecoveryManifest manifest;
    bool has_manifest = false;
    if (!db_path_.empty()) {
        ManifestStore manifest_store(db_path_);
        has_manifest = manifest_store.load(manifest);
    }

    auto entries = wal_->recover(recovery_mode_);
    if (entries.empty() && !db_path_.empty()) {
        if (has_manifest && !manifest.sstables.empty()) {
            std::vector<std::string> ordered_sst_paths;
            ordered_sst_paths.reserve(manifest.sstables.size());

            for (const auto& segment : manifest.sstables) {
                std::filesystem::path segment_path(segment.relative_path);
                if (segment_path.is_relative()) {
                    segment_path = db_path_ / segment_path;
                }
                ordered_sst_paths.push_back(segment_path.string());
            }

            storage_->loadSSTablesFromFiles(ordered_sst_paths, recovery_mode_);
        } else {
            storage_->loadSSTablesFromDirectory((db_path_ / "sstables").string(), recovery_mode_);
        }

        resetCompactionCountersFromWal();
        writeRecoveryManifestSnapshot();
        return;
    }

    for (auto& entry : entries) {
        if (entry.op == WalOp::PUT) {
            storage_->putPrecompressed(entry.key, std::move(entry.value), entry.ttl_ms);
        } else if (entry.op == WalOp::DEL) {
            storage_->del(entry.key);
        }
    }

    resetCompactionCountersFromWal();

    writeRecoveryManifestSnapshot();
}

void TitanEngine::put(const std::string& key, const std::string& value, int64_t ttl_ms) {
    storage_->put(key, value, ttl_ms);
    logical_write_bytes_total_.fetch_add(value.size());
    if (wal_) {
        wal_->logPut(key, value, ttl_ms, storage_->getCompressionLevel());
        const size_t estimated_bytes = 1 + 4 + 4 + key.size() + value.size() + 8 + 4;
        trackWalActivity(1, 0, estimated_bytes);
        maybeAutoCompact();
    }
}

std::optional<std::string> TitanEngine::get(const std::string& key) {
    return storage_->get(key);
}

bool TitanEngine::del(const std::string& key) {
    bool deleted = storage_->del(key);
    if (deleted && wal_) {
        wal_->logDel(key);
        const size_t estimated_bytes = 1 + 4 + key.size() + 4;
        trackWalActivity(0, 1, estimated_bytes);
        maybeAutoCompact();
    }
    return deleted;
}

bool TitanEngine::has(const std::string& key) {
    return storage_->has(key);
}

size_t TitanEngine::size() const {
    return storage_->getStats().key_count;
}

void TitanEngine::clear() {
    storage_->clear();
    if (wal_) {
        wal_->compact({});
        resetCompactionCountersFromWal();
    }
    writeRecoveryManifestSnapshot();
}

int64_t TitanEngine::incr(const std::string& key, int64_t delta) {
    auto val_opt = storage_->get(key);
    int64_t val = 0;
    if (val_opt) {
        try {
            val = std::stoll(*val_opt);
        } catch (...) {
            val = 0;
        }
    }
    val += delta;
    put(key, std::to_string(val));
    return val;
}

int64_t TitanEngine::decr(const std::string& key, int64_t delta) {
    return incr(key, -delta);
}

std::vector<std::string> TitanEngine::keys(size_t limit) const {
    return storage_->keys(limit);
}

std::vector<TitanEngine::KVPair> TitanEngine::scan(const std::string& prefix, size_t limit) const {
    return storage_->scan(prefix, limit);
}

std::vector<TitanEngine::KVPair> TitanEngine::range(
    const std::string& start, const std::string& end, size_t limit) const {
    return storage_->range(start, end, limit);
}

size_t TitanEngine::countPrefix(const std::string& prefix) const {
    return storage_->countPrefix(prefix);
}

void TitanEngine::putBatch(const std::vector<KVPair>& pairs) {
    if (pairs.empty()) return;

    std::vector<std::pair<std::string, std::vector<uint8_t>>> compressed_batch;
    compressed_batch.reserve(pairs.size());
    size_t total_raw_size = 0;

    Compressor comp;
    int level = storage_->getCompressionLevel();
    for (const auto& [k, v] : pairs) {
        compressed_batch.push_back({k, comp.compress(v, level)});
        total_raw_size += v.size();
        logical_write_bytes_total_.fetch_add(v.size());
    }

    size_t estimated_bytes = 0;
    for (const auto& [k, v] : compressed_batch) {
        estimated_bytes += 1 + 4 + 4 + k.size() + v.size() + 8 + 4;
    }

    if (wal_) wal_->logPrecompressedBatch(compressed_batch);
    storage_->putPrecompressedBatch(std::move(compressed_batch), total_raw_size);

    if (wal_) {
        trackWalActivity(pairs.size(), 0, estimated_bytes);
        maybeAutoCompact();
    }
}

std::vector<std::optional<std::string>> TitanEngine::getBatch(const std::vector<std::string>& keys) {
    return storage_->getBatch(keys);
}

void TitanEngine::flush() {
    if (wal_) wal_->flush();
}

void TitanEngine::compactInternal(bool auto_triggered) {
    if (!wal_) return;

    auto snapshot = storage_->snapshot();
    std::vector<LogEntry> entries;
    entries.reserve(snapshot.size());

    for (auto& [k, v] : snapshot) {
        entries.push_back({WalOp::PUT, std::move(k), std::move(v)});
    }

    wal_->compact(entries);
    std::error_code ec;
    if (std::filesystem::exists(wal_->path(), ec)) {
        const size_t compacted_wal_size = std::filesystem::file_size(wal_->path(), ec);
        if (!ec) {
            physical_write_bytes_total_.fetch_add(compacted_wal_size);
        } else {
            ec.clear();
        }
    }

    compaction_count_total_.fetch_add(1);
    if (auto_triggered) {
        auto_compaction_count_total_.fetch_add(1);
    }

    resetCompactionCountersFromWal();
    writeRecoveryManifestSnapshot();
}

void TitanEngine::compact() {
    if (!wal_) return;

    bool expected = false;
    if (!compact_in_progress_.compare_exchange_strong(expected, true)) {
        return;
    }

    if (compaction_thread_.joinable()) {
        compaction_thread_.join();
    }

    try {
        compactInternal(false);
        compact_in_progress_.store(false);
    } catch (...) {
        compact_in_progress_.store(false);
        throw;
    }
}

void TitanEngine::setCompressionLevel(int level) {
    storage_->setCompressionLevel(level);
}

void TitanEngine::setMaxMemoryBytes(size_t limit_bytes) {
    storage_->setMaxMemoryBytes(limit_bytes);
}

void TitanEngine::setSSTableBloomFilterEnabled(bool enabled) {
    storage_->setSSTableBloomFilterEnabled(enabled);
}

void TitanEngine::setAutoCompactEnabled(bool enabled) {
    compaction_policy_.auto_compact = enabled;
}

void TitanEngine::setCompactionPolicy(size_t min_ops, double tombstone_ratio, size_t min_wal_bytes) {
    compaction_policy_.min_ops = min_ops == 0 ? 1 : min_ops;

    if (tombstone_ratio < 0.0) {
        compaction_policy_.tombstone_ratio = 0.0;
    } else if (tombstone_ratio > 1.0) {
        compaction_policy_.tombstone_ratio = 1.0;
    } else {
        compaction_policy_.tombstone_ratio = tombstone_ratio;
    }

    compaction_policy_.min_wal_bytes = min_wal_bytes;
}

StorageStats TitanEngine::getStats() const {
    StorageStats stats = storage_->getStats();

    std::error_code ec;
    if (wal_ && std::filesystem::exists(wal_->path(), ec)) {
        stats.wal_size_bytes = std::filesystem::file_size(wal_->path(), ec);
        if (ec) {
            ec.clear();
            stats.wal_size_bytes = 0;
        }
    }

    stats.logical_write_bytes = logical_write_bytes_total_.load();
    stats.physical_write_bytes = physical_write_bytes_total_.load();
    stats.compaction_count = compaction_count_total_.load();
    stats.auto_compaction_count = auto_compaction_count_total_.load();

    if (stats.logical_write_bytes > 0) {
        stats.write_amplification = static_cast<double>(stats.physical_write_bytes)
            / static_cast<double>(stats.logical_write_bytes);
    } else {
        stats.write_amplification = 0.0;
    }

    if (stats.raw_bytes > 0) {
        stats.space_amplification = static_cast<double>(stats.wal_size_bytes)
            / static_cast<double>(stats.raw_bytes);
    } else {
        stats.space_amplification = 0.0;
    }

    return stats;
}

void TitanEngine::close() {
    if (compaction_thread_.joinable()) {
        compaction_thread_.join();
    }

    if (storage_) {
        storage_->flushSpillState();
    }

    if (wal_) {
        wal_->flush();
        writeRecoveryManifestSnapshot();
        wal_.reset();
    }
    storage_.reset();
}

void TitanEngine::writeRecoveryManifestSnapshot() {
    if (db_path_.empty()) {
        return;
    }

    ManifestStore manifest_store(db_path_);
    RecoveryManifest manifest;
    manifest.updated_at_ms = unixNowMs();

    if (wal_) {
        manifest.wal_file = wal_->path().filename().string();
        manifest.wal_format = wal_->usesChecksummedFormat() ? "checksummed" : "legacy";

        std::error_code ec;
        manifest.wal_size_bytes = std::filesystem::exists(wal_->path(), ec)
            ? std::filesystem::file_size(wal_->path(), ec)
            : 0;
    } else {
        manifest.wal_file = "";
        manifest.wal_format = "missing";
        manifest.wal_size_bytes = 0;
    }

    const std::filesystem::path sst_dir = db_path_ / "sstables";
    std::error_code ec;
    if (std::filesystem::exists(sst_dir, ec)) {
        std::vector<std::filesystem::path> files;
        for (const auto& entry : std::filesystem::directory_iterator(sst_dir, ec)) {
            if (ec) break;
            if (!entry.is_regular_file()) continue;
            if (entry.path().extension() != ".sst") continue;
            files.push_back(entry.path());
        }

        std::sort(files.begin(), files.end(), [](const auto& a, const auto& b) {
            return a.filename().string() < b.filename().string();
        });

        for (const auto& file : files) {
            SegmentManifestEntry item;
            item.relative_path = std::filesystem::relative(file, db_path_, ec).generic_string();
            if (ec) {
                ec.clear();
                item.relative_path = file.filename().generic_string();
            }

            item.size_bytes = std::filesystem::file_size(file, ec);
            if (ec) {
                ec.clear();
                item.size_bytes = 0;
            }

            if (std::filesystem::exists(file, ec)) {
                const auto mtime = std::filesystem::last_write_time(file, ec);
                if (!ec) {
                    using namespace std::chrono;
                    const auto now_sys = system_clock::now();
                    const auto now_fs = std::filesystem::file_time_type::clock::now();
                    const auto adjusted = now_sys + duration_cast<system_clock::duration>(mtime - now_fs);
                    item.mtime_ms = duration_cast<milliseconds>(adjusted.time_since_epoch()).count();
                } else {
                    ec.clear();
                }
            }

            manifest.sstables.push_back(std::move(item));
        }
    }

    manifest_store.save(manifest);
}

} // namespace titan
