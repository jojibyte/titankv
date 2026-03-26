#pragma once

#include "utils.hpp"
#include "compressor.hpp"
#include <string>
#include <vector>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <memory>

#ifdef _WIN32
#include <windows.h>
#endif

namespace titan {

enum class WalOp : uint8_t {
    PUT = 1,
    DEL = 2,
    CHECKPOINT = 3
};

struct LogEntry {
    WalOp op;
    std::string key;
    std::vector<uint8_t> value;
    int64_t ttl_ms = 0;
};

class WAL {
public:
    explicit WAL(const std::filesystem::path& dir);
    ~WAL();

    WAL(const WAL&) = delete;
    WAL& operator=(const WAL&) = delete;

    void logPut(const std::string& key, const std::string& value, int64_t ttl_ms = 0, int compression_level = 3);
    void logPrecompressedBatch(const std::vector<std::pair<std::string, std::vector<uint8_t>>>& batch);
    void logDel(const std::string& key);

    std::vector<LogEntry> recover();
    void compact(const std::vector<LogEntry>& active_entries);
    void flush();

private:
    std::filesystem::path path_;
    std::filesystem::path lock_path_;
    std::ofstream file_;
    std::mutex mutex_;
    std::unique_ptr<Compressor> compressor_;

#ifdef _WIN32
    HANDLE lock_handle_ = INVALID_HANDLE_VALUE;
#else
    int lock_fd_ = -1;
#endif

    void writeEntry(WalOp op, const std::string& key, const std::vector<uint8_t>& value, int64_t ttl_ms = 0);
};

} // namespace titan
