#include "wal.hpp"
#include "checksum.hpp"
#include <cstring>
#include <array>

#ifndef _WIN32
#include <fcntl.h>
#include <unistd.h>
#include <sys/file.h>
#endif

namespace titan {

namespace {
constexpr const char* kWalFileName = "titan.tkv";
constexpr const char* kLegacyWalFileName = "titan.t";
constexpr std::array<uint8_t, 8> kWalMagic{{'T', 'K', 'V', 'W', 'A', 'L', '3', '\n'}};
}

bool WAL::isChecksummedWalFile(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in.is_open()) {
        return false;
    }

    std::array<uint8_t, kWalMagic.size()> header{};
    if (!in.read(reinterpret_cast<char*>(header.data()), static_cast<std::streamsize>(header.size()))) {
        return false;
    }

    return header == kWalMagic;
}

void WAL::writeWalMagicHeader(std::ofstream& out) {
    out.write(reinterpret_cast<const char*>(kWalMagic.data()), static_cast<std::streamsize>(kWalMagic.size()));
}

void WAL::recoverCompactionArtifacts() {
    auto temp_path = path_;
    temp_path += ".tmp";
    auto backup_path = path_;
    backup_path += ".bak";

    std::error_code ec;

    const auto exists_file = [&](const std::filesystem::path& p) {
        std::error_code inner_ec;
        return std::filesystem::exists(p, inner_ec);
    };

    bool has_main = exists_file(path_);
    bool has_temp = exists_file(temp_path);
    bool has_backup = exists_file(backup_path);

    if (!has_main && has_backup) {
        std::filesystem::rename(backup_path, path_, ec);
        if (ec) {
            ec.clear();
        }
    }

    has_main = exists_file(path_);
    has_temp = exists_file(temp_path);
    has_backup = exists_file(backup_path);

    if (!has_main && has_temp) {
        std::filesystem::rename(temp_path, path_, ec);
        if (ec) {
            ec.clear();
        }
    }

    has_main = exists_file(path_);
    has_temp = exists_file(temp_path);
    has_backup = exists_file(backup_path);

    if (has_main && has_temp) {
        std::filesystem::remove(temp_path, ec);
        ec.clear();
    }

    if (has_main && has_backup) {
        std::filesystem::remove(backup_path, ec);
        ec.clear();
    }
}

WAL::WAL(const std::filesystem::path& dir) {
    if (!std::filesystem::exists(dir)) {
        std::filesystem::create_directories(dir);
    }

    lock_path_ = dir / "titan.lock";
#ifdef _WIN32
    lock_handle_ = CreateFileW(lock_path_.wstring().c_str(), GENERIC_READ | GENERIC_WRITE,
                               0, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (lock_handle_ == INVALID_HANDLE_VALUE) {
        throw std::runtime_error("failed to acquire exclusive database lock (another process is using it)");
    }
#else
    lock_fd_ = ::open(lock_path_.c_str(), O_RDWR | O_CREAT, 0666);
    if (lock_fd_ < 0 || ::flock(lock_fd_, LOCK_EX | LOCK_NB) < 0) {
        if (lock_fd_ >= 0) { ::close(lock_fd_); lock_fd_ = -1; }
        throw std::runtime_error("failed to acquire exclusive database lock (another process is using it)");
    }
#endif

    path_ = dir / kWalFileName;
    const auto legacy_path = dir / kLegacyWalFileName;
    if (!std::filesystem::exists(path_) && std::filesystem::exists(legacy_path)) {
        std::error_code ec;
        std::filesystem::rename(legacy_path, path_, ec);
        if (ec) {
            path_ = legacy_path;
        }
    }

    recoverCompactionArtifacts();

    std::error_code size_ec;
    const bool wal_exists = std::filesystem::exists(path_, size_ec);
    const auto wal_size = wal_exists ? std::filesystem::file_size(path_, size_ec) : 0;

    if (wal_exists && wal_size > 0) {
        checksummed_format_ = isChecksummedWalFile(path_);
    } else {
        checksummed_format_ = true;
        std::ofstream init(path_, std::ios::binary | std::ios::trunc);
        if (!init.is_open()) {
            throw std::runtime_error("failed to initialize WAL file: " + path_.string());
        }
        writeWalMagicHeader(init);
        init.flush();
        init.close();
    }

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

#ifdef _WIN32
    if (lock_handle_ != INVALID_HANDLE_VALUE) {
        CloseHandle(lock_handle_);
        lock_handle_ = INVALID_HANDLE_VALUE;
        std::filesystem::remove(lock_path_);
    }
#else
    if (lock_fd_ >= 0) {
        ::flock(lock_fd_, LOCK_UN);
        ::close(lock_fd_);
        lock_fd_ = -1;
        std::filesystem::remove(lock_path_);
    }
#endif
}

void WAL::writeEntry(WalOp op, const std::string& key, const std::vector<uint8_t>& value, int64_t ttl_ms) {
    uint32_t klen = static_cast<uint32_t>(key.size());
    uint32_t vlen = static_cast<uint32_t>(value.size());
    uint8_t op_byte = static_cast<uint8_t>(op);

    TITAN_ASSERT(klen > 0, "empty key in WAL write");

    if (!checksummed_format_) {
        file_.write(reinterpret_cast<const char*>(&op_byte), 1);
        file_.write(reinterpret_cast<const char*>(&klen), 4);
        if (op == WalOp::PUT) {
            file_.write(reinterpret_cast<const char*>(&vlen), 4);
        }
        file_.write(key.data(), klen);
        if (op == WalOp::PUT) {
            file_.write(reinterpret_cast<const char*>(value.data()), vlen);
            file_.write(reinterpret_cast<const char*>(&ttl_ms), 8);
        }
        return;
    }

    std::vector<uint8_t> payload;
    payload.reserve(1 + 4 + (op == WalOp::PUT ? 4 : 0) + key.size() + value.size() + (op == WalOp::PUT ? 8 : 0));

    payload.push_back(op_byte);
    payload.insert(payload.end(), reinterpret_cast<const uint8_t*>(&klen), reinterpret_cast<const uint8_t*>(&klen) + sizeof(klen));
    if (op == WalOp::PUT) {
        payload.insert(payload.end(), reinterpret_cast<const uint8_t*>(&vlen), reinterpret_cast<const uint8_t*>(&vlen) + sizeof(vlen));
    }
    payload.insert(payload.end(), key.begin(), key.end());
    if (op == WalOp::PUT) {
        payload.insert(payload.end(), value.begin(), value.end());
        payload.insert(payload.end(), reinterpret_cast<const uint8_t*>(&ttl_ms), reinterpret_cast<const uint8_t*>(&ttl_ms) + sizeof(ttl_ms));
    }

    const uint32_t checksum = fnv1a32(payload.data(), payload.size());
    file_.write(reinterpret_cast<const char*>(payload.data()), static_cast<std::streamsize>(payload.size()));
    file_.write(reinterpret_cast<const char*>(&checksum), static_cast<std::streamsize>(sizeof(checksum)));
}

void WAL::logPut(const std::string& key, const std::string& value, int64_t ttl_ms, int compression_level) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto compressed = compressor_->compress(value, compression_level);
    writeEntry(WalOp::PUT, key, compressed, ttl_ms);
    file_.flush();
}

void WAL::logPrecompressedBatch(const std::vector<std::pair<std::string, std::vector<uint8_t>>>& batch) {
    std::lock_guard<std::mutex> lock(mutex_);
    for (const auto& [key, compressed] : batch) {
        writeEntry(WalOp::PUT, key, compressed);
    }
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

std::vector<LogEntry> WAL::recover(RecoveryMode mode) {
    std::vector<LogEntry> entries;
    std::ifstream in(path_, std::ios::binary);
    if (!in.is_open()) return entries;

    auto handleCorruption = [&](const std::string& message) {
        if (mode == RecoveryMode::Strict) {
            throw std::runtime_error(message);
        }
    };

    if (checksummed_format_) {
        std::array<uint8_t, kWalMagic.size()> header{};
        if (!in.read(reinterpret_cast<char*>(header.data()), static_cast<std::streamsize>(header.size()))) {
            handleCorruption("corrupt WAL: missing magic header");
            return entries;
        }
        if (header != kWalMagic) {
            handleCorruption("corrupt WAL: invalid magic header");
            return entries;
        }

        while (in.peek() != EOF) {
            std::vector<uint8_t> payload;

            uint8_t op_byte = 0;
            if (!in.read(reinterpret_cast<char*>(&op_byte), 1)) {
                break;
            }
            payload.push_back(op_byte);

            WalOp op = static_cast<WalOp>(op_byte);
            if (op != WalOp::PUT && op != WalOp::DEL) {
                handleCorruption("corrupt WAL: invalid operation code");
                break;
            }

            uint32_t klen = 0;
            if (!in.read(reinterpret_cast<char*>(&klen), 4)) {
                handleCorruption("corrupt WAL: truncated key length");
                break;
            }
            payload.insert(payload.end(), reinterpret_cast<const uint8_t*>(&klen), reinterpret_cast<const uint8_t*>(&klen) + sizeof(klen));

            constexpr uint32_t MAX_KEY_SIZE = 1024 * 1024;
            if (klen == 0 || klen > MAX_KEY_SIZE) {
                handleCorruption("corrupt WAL: invalid key length");
                break;
            }

            uint32_t vlen = 0;
            if (op == WalOp::PUT) {
                if (!in.read(reinterpret_cast<char*>(&vlen), 4)) {
                    handleCorruption("corrupt WAL: truncated value length");
                    break;
                }
                payload.insert(payload.end(), reinterpret_cast<const uint8_t*>(&vlen), reinterpret_cast<const uint8_t*>(&vlen) + sizeof(vlen));

                constexpr uint32_t MAX_VALUE_SIZE = 100 * 1024 * 1024;
                if (vlen > MAX_VALUE_SIZE) {
                    handleCorruption("corrupt WAL: invalid value length");
                    break;
                }
            }

            std::string key(klen, '\0');
            if (!in.read(key.data(), static_cast<std::streamsize>(klen))) {
                handleCorruption("corrupt WAL: truncated key payload");
                break;
            }
            payload.insert(payload.end(), key.begin(), key.end());

            std::vector<uint8_t> value;
            int64_t ttl_ms = 0;
            if (op == WalOp::PUT) {
                value.resize(vlen);
                if (!in.read(reinterpret_cast<char*>(value.data()), static_cast<std::streamsize>(vlen))) {
                    handleCorruption("corrupt WAL: truncated value payload");
                    break;
                }
                payload.insert(payload.end(), value.begin(), value.end());

                if (!in.read(reinterpret_cast<char*>(&ttl_ms), 8)) {
                    handleCorruption("corrupt WAL: truncated ttl payload");
                    break;
                }
                payload.insert(payload.end(), reinterpret_cast<const uint8_t*>(&ttl_ms), reinterpret_cast<const uint8_t*>(&ttl_ms) + sizeof(ttl_ms));
            }

            uint32_t stored_checksum = 0;
            if (!in.read(reinterpret_cast<char*>(&stored_checksum), 4)) {
                handleCorruption("corrupt WAL: missing checksum");
                break;
            }

            const uint32_t actual_checksum = fnv1a32(payload.data(), payload.size());
            if (actual_checksum != stored_checksum) {
                handleCorruption("corrupt WAL: checksum mismatch");
                break;
            }

            entries.push_back({op, std::move(key), std::move(value), ttl_ms});
        }

        return entries;
    }

    while (in.peek() != EOF) {
        uint8_t op_byte;
        if (!in.read(reinterpret_cast<char*>(&op_byte), 1)) break;
        WalOp op = static_cast<WalOp>(op_byte);
        if (op != WalOp::PUT && op != WalOp::DEL) {
            handleCorruption("corrupt WAL: invalid operation code");
            break;
        }

        uint32_t klen;
        if (!in.read(reinterpret_cast<char*>(&klen), 4)) {
            handleCorruption("corrupt WAL: truncated key length");
            break;
        }

        constexpr uint32_t MAX_KEY_SIZE = 1024 * 1024; // 1MB
        if (klen == 0 || klen > MAX_KEY_SIZE) {
            handleCorruption("corrupt WAL: invalid key length");
            break;
        }

        uint32_t vlen = 0;
        if (op == WalOp::PUT) {
            if (!in.read(reinterpret_cast<char*>(&vlen), 4)) {
                handleCorruption("corrupt WAL: truncated value length");
                break;
            }
            constexpr uint32_t MAX_VALUE_SIZE = 100 * 1024 * 1024; // 100MB
            if (vlen > MAX_VALUE_SIZE) {
                handleCorruption("corrupt WAL: value length too large");
                break;
            }
        }

        std::string key(klen, '\0');
        if (!in.read(key.data(), klen)) {
            handleCorruption("corrupt WAL: truncated key payload");
            break;
        }

        std::vector<uint8_t> value;
        int64_t ttl_ms = 0;
        if (op == WalOp::PUT) {
            value.resize(vlen);
            if (!in.read(reinterpret_cast<char*>(value.data()), vlen)) {
                handleCorruption("corrupt WAL: truncated value payload");
                break;
            }
            if (!in.read(reinterpret_cast<char*>(&ttl_ms), 8)) {
                handleCorruption("corrupt WAL: truncated ttl payload");
                break;
            }
        }

        entries.push_back({op, std::move(key), std::move(value), ttl_ms});
    }
    return entries;
}

void WAL::compact(const std::vector<LogEntry>& active_entries) {
    std::lock_guard<std::mutex> lock(mutex_);

    file_.close();
    auto temp_path = path_;
    temp_path += ".tmp";
    auto backup_path = path_;
    backup_path += ".bak";

    std::error_code ec;
    std::filesystem::remove(temp_path, ec);
    ec.clear();
    std::filesystem::remove(backup_path, ec);
    ec.clear();

    std::ofstream out(temp_path, std::ios::binary | std::ios::trunc);
    if (!out.is_open()) {
        file_.open(path_, std::ios::binary | std::ios::app);
        throw std::runtime_error("failed to open compact temp WAL: " + temp_path.string());
    }

    if (checksummed_format_) {
        writeWalMagicHeader(out);
    }

    for (const auto& entry : active_entries) {
        uint32_t klen = static_cast<uint32_t>(entry.key.size());
        uint32_t vlen = static_cast<uint32_t>(entry.value.size());
        uint8_t op_byte = static_cast<uint8_t>(WalOp::PUT);
        int64_t ttl_ms = entry.ttl_ms;

        if (!checksummed_format_) {
            out.write(reinterpret_cast<const char*>(&op_byte), 1);
            out.write(reinterpret_cast<const char*>(&klen), 4);
            out.write(reinterpret_cast<const char*>(&vlen), 4);
            out.write(entry.key.data(), klen);
            out.write(reinterpret_cast<const char*>(entry.value.data()), vlen);
            out.write(reinterpret_cast<const char*>(&ttl_ms), 8);
            continue;
        }

        std::vector<uint8_t> payload;
        payload.reserve(1 + 4 + 4 + entry.key.size() + entry.value.size() + 8);
        payload.push_back(op_byte);
        payload.insert(payload.end(), reinterpret_cast<const uint8_t*>(&klen), reinterpret_cast<const uint8_t*>(&klen) + sizeof(klen));
        payload.insert(payload.end(), reinterpret_cast<const uint8_t*>(&vlen), reinterpret_cast<const uint8_t*>(&vlen) + sizeof(vlen));
        payload.insert(payload.end(), entry.key.begin(), entry.key.end());
        payload.insert(payload.end(), entry.value.begin(), entry.value.end());
        payload.insert(payload.end(), reinterpret_cast<const uint8_t*>(&ttl_ms), reinterpret_cast<const uint8_t*>(&ttl_ms) + sizeof(ttl_ms));

        const uint32_t checksum = fnv1a32(payload.data(), payload.size());
        out.write(reinterpret_cast<const char*>(payload.data()), static_cast<std::streamsize>(payload.size()));
        out.write(reinterpret_cast<const char*>(&checksum), sizeof(checksum));
    }
    out.flush();
    out.close();

    const bool had_original = std::filesystem::exists(path_, ec);
    ec.clear();

    if (had_original) {
        std::filesystem::rename(path_, backup_path, ec);
        if (ec) {
            file_.open(path_, std::ios::binary | std::ios::app);
            throw std::runtime_error("failed to stage WAL backup during compact: " + path_.string());
        }
    }

    std::filesystem::rename(temp_path, path_, ec);
    if (ec) {
        if (had_original) {
            std::error_code rollback_ec;
            std::filesystem::rename(backup_path, path_, rollback_ec);
        }
        file_.open(path_, std::ios::binary | std::ios::app);
        throw std::runtime_error("failed to finalize compacted WAL swap: " + path_.string());
    }

    if (had_original) {
        std::filesystem::remove(backup_path, ec);
        ec.clear();
    }

    file_.open(path_, std::ios::binary | std::ios::app);
    if (!file_.is_open()) {
        throw std::runtime_error("failed to reopen WAL after compact: " + path_.string());
    }
}

} // namespace titan
