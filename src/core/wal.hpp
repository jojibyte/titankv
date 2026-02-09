#pragma once

#include "utils.hpp"
#include "compressor.hpp"
#include <string>
#include <vector>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <memory>

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
};

class WAL {
public:
    explicit WAL(const std::filesystem::path& dir);
    ~WAL();

    WAL(const WAL&) = delete;
    WAL& operator=(const WAL&) = delete;

    void logPut(const std::string& key, const std::string& value);
    void logPutCompressed(const std::string& key, const std::vector<uint8_t>& compressed_value);
    void logDel(const std::string& key);

    std::vector<LogEntry> recover();
    void compact(const std::vector<LogEntry>& active_entries);
    void flush();

private:
    std::filesystem::path path_;
    std::ofstream file_;
    std::mutex mutex_;
    std::unique_ptr<Compressor> compressor_;

    void writeEntry(WalOp op, const std::string& key, const std::vector<uint8_t>& value);
};

} // namespace titan
