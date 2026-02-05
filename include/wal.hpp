#pragma once

#include <string>
#include <string_view>
#include <fstream>
#include <filesystem>
#include <mutex>
#include <vector>

namespace titan {

enum class SyncMode {
    SYNC,   // fsync after each write
    ASYNC,  // batch fsync
    NONE    // no fsync (fastest, least durable)
};

enum class WalOp : uint8_t {
    PUT = 1,
    DEL = 2
};

class WAL {
public:
    WAL(const std::filesystem::path& dir, SyncMode mode = SyncMode::ASYNC);
    ~WAL();

    WAL(const WAL&) = delete;
    WAL& operator=(const WAL&) = delete;

    void logPut(const std::string& key, const std::string& value);
    void logDel(const std::string& key);
    void flush();

    // recovery - returns all ops in order
    struct LogEntry {
        WalOp op;
        std::string key;
        std::string value;
    };
    std::vector<LogEntry> recover();
    void compact(); // clear log after checkpoint

private:
    std::filesystem::path path_;
    std::ofstream file_;
    SyncMode mode_;
    std::mutex mutex_;
    size_t unflushed_ = 0;

    void writeEntry(WalOp op, const std::string& key, const std::string& value);
    void maybeFlush();
};

} // namespace titan
