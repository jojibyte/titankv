#include "wal.hpp"
#include <cstring>

namespace titan {

WAL::WAL(const std::filesystem::path& dir) {
    if (!std::filesystem::exists(dir)) {
        std::filesystem::create_directories(dir);
    }
    path_ = dir / "titan.t";
    file_.open(path_, std::ios::binary | std::ios::app);
    if (!file_.is_open()) {
        throw std::runtime_error("failed to open WAL file: " + path_.string());
    }
    compressor_ = std::make_unique<Compressor>();
}

WAL::~WAL() {
    if (file_.is_open()) {
        file_.flush();
        file_.close();
    }
}

void WAL::writeEntry(WalOp op, const std::string& key, const std::vector<uint8_t>& value) {
    uint32_t klen = static_cast<uint32_t>(key.size());
    uint32_t vlen = static_cast<uint32_t>(value.size());
    uint8_t op_byte = static_cast<uint8_t>(op);

    TITAN_ASSERT(klen > 0, "empty key in WAL write");

    file_.write(reinterpret_cast<const char*>(&op_byte), 1);
    file_.write(reinterpret_cast<const char*>(&klen), 4);
    if (op == WalOp::PUT) {
        file_.write(reinterpret_cast<const char*>(&vlen), 4);
    }
    file_.write(key.data(), klen);
    if (op == WalOp::PUT) {
        file_.write(reinterpret_cast<const char*>(value.data()), vlen);
    }
}

void WAL::logPut(const std::string& key, const std::string& value) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto compressed = compressor_->compress(value, 15);
    writeEntry(WalOp::PUT, key, compressed);
    file_.flush();
}

void WAL::logDel(const std::string& key) {
    std::lock_guard<std::mutex> lock(mutex_);
    writeEntry(WalOp::DEL, key, {});
    file_.flush();
}

void WAL::flush() {
    std::lock_guard<std::mutex> lock(mutex_);
    file_.flush();
}

std::vector<LogEntry> WAL::recover() {
    std::vector<LogEntry> entries;
    std::ifstream in(path_, std::ios::binary);
    if (!in.is_open()) return entries;

    while (in.peek() != EOF) {
        uint8_t op_byte;
        if (!in.read(reinterpret_cast<char*>(&op_byte), 1)) break;
        WalOp op = static_cast<WalOp>(op_byte);

        uint32_t klen;
        if (!in.read(reinterpret_cast<char*>(&klen), 4)) break;

        constexpr uint32_t MAX_KEY_SIZE = 1024 * 1024; // 1MB
        if (klen == 0 || klen > MAX_KEY_SIZE) {
            throw std::runtime_error("corrupt WAL: invalid key length");
        }

        uint32_t vlen = 0;
        if (op == WalOp::PUT) {
            if (!in.read(reinterpret_cast<char*>(&vlen), 4)) break;
            constexpr uint32_t MAX_VALUE_SIZE = 100 * 1024 * 1024; // 100MB
            if (vlen > MAX_VALUE_SIZE) {
                throw std::runtime_error("corrupt WAL: value length too large");
            }
        }

        std::string key(klen, '\0');
        if (!in.read(key.data(), klen)) break;

        std::vector<uint8_t> value;
        if (op == WalOp::PUT) {
            value.resize(vlen);
            if (!in.read(reinterpret_cast<char*>(value.data()), vlen)) break;
        }

        entries.push_back({op, std::move(key), std::move(value)});
    }
    return entries;
}

void WAL::compact(const std::vector<LogEntry>& active_entries) {
    std::lock_guard<std::mutex> lock(mutex_);

    file_.close();
    auto temp_path = path_;
    temp_path += ".tmp";
    std::ofstream out(temp_path, std::ios::binary | std::ios::trunc);

    for (const auto& entry : active_entries) {
        uint32_t klen = static_cast<uint32_t>(entry.key.size());
        uint32_t vlen = static_cast<uint32_t>(entry.value.size());
        uint8_t op_byte = static_cast<uint8_t>(WalOp::PUT);

        out.write(reinterpret_cast<const char*>(&op_byte), 1);
        out.write(reinterpret_cast<const char*>(&klen), 4);
        out.write(reinterpret_cast<const char*>(&vlen), 4);
        out.write(entry.key.data(), klen);
        out.write(reinterpret_cast<const char*>(entry.value.data()), vlen);
    }
    out.flush();
    out.close();

    std::filesystem::rename(temp_path, path_);
    file_.open(path_, std::ios::binary | std::ios::app);
}

} // namespace titan
