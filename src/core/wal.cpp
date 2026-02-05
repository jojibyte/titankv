#include "wal.hpp"
#include <cstring>

namespace titan {

WAL::WAL(const std::filesystem::path& dir, SyncMode mode)
    : mode_(mode) {
    std::filesystem::create_directories(dir);
    path_ = dir / "titan.wal";
    file_.open(path_, std::ios::binary | std::ios::app);
}

WAL::~WAL() {
    if (file_.is_open()) {
        file_.flush();
        file_.close();
    }
}

void WAL::writeEntry(WalOp op, const std::string& key, const std::string& value) {
    uint32_t key_len = static_cast<uint32_t>(key.size());
    uint32_t val_len = static_cast<uint32_t>(value.size());

    file_.write(reinterpret_cast<const char*>(&op), 1);
    file_.write(reinterpret_cast<const char*>(&key_len), 4);
    file_.write(reinterpret_cast<const char*>(&val_len), 4);
    file_.write(key.data(), key_len);
    file_.write(value.data(), val_len);
}

void WAL::maybeFlush() {
    unflushed_++;
    if (mode_ == SyncMode::SYNC) {
        file_.flush();
        unflushed_ = 0;
    } else if (mode_ == SyncMode::ASYNC && unflushed_ >= 100) {
        file_.flush();
        unflushed_ = 0;
    }
}

void WAL::logPut(const std::string& key, const std::string& value) {
    std::lock_guard lock(mutex_);
    writeEntry(WalOp::PUT, key, value);
    maybeFlush();
}

void WAL::logDel(const std::string& key) {
    std::lock_guard lock(mutex_);
    writeEntry(WalOp::DEL, key, "");
    maybeFlush();
}

void WAL::flush() {
    std::lock_guard lock(mutex_);
    file_.flush();
    unflushed_ = 0;
}

std::vector<WAL::LogEntry> WAL::recover() {
    std::vector<LogEntry> entries;
    std::ifstream in(path_, std::ios::binary);
    if (!in) return entries;

    while (in.peek() != EOF) {
        LogEntry e;
        uint8_t op;
        uint32_t key_len, val_len;

        if (!in.read(reinterpret_cast<char*>(&op), 1)) break;
        if (!in.read(reinterpret_cast<char*>(&key_len), 4)) break;
        if (!in.read(reinterpret_cast<char*>(&val_len), 4)) break;

        e.op = static_cast<WalOp>(op);
        e.key.resize(key_len);
        e.value.resize(val_len);

        if (!in.read(e.key.data(), key_len)) break;
        if (!in.read(e.value.data(), val_len)) break;

        entries.push_back(std::move(e));
    }
    return entries;
}

void WAL::compact() {
    std::lock_guard lock(mutex_);
    file_.close();
    std::filesystem::remove(path_);
    file_.open(path_, std::ios::binary | std::ios::app);
}

} // namespace titan
