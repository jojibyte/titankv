#include "compressor.hpp"
#include "utils.hpp"
#include <zstd.h>
#include <stdexcept>

namespace titan {

Compressor::Compressor() {
    cctx_ = ZSTD_createCCtx();
    dctx_ = ZSTD_createDCtx();
    TITAN_ASSERT(cctx_ != nullptr, "failed to create ZSTD compression context");
    TITAN_ASSERT(dctx_ != nullptr, "failed to create ZSTD decompression context");
}

Compressor::~Compressor() {
    if (cctx_) ZSTD_freeCCtx(cctx_);
    if (dctx_) ZSTD_freeDCtx(dctx_);
}

std::vector<uint8_t> Compressor::compress(const std::string& data, int level) {
    if (data.empty()) return {};

    size_t bound = ZSTD_compressBound(data.size());
    std::vector<uint8_t> buffer(bound);

    size_t result = ZSTD_compressCCtx(cctx_, buffer.data(), bound, data.data(), data.size(), level);
    TITAN_ASSERT(!ZSTD_isError(result),
        std::string("compression failed: ") + ZSTD_getErrorName(result));

    buffer.resize(result);
    return buffer;
}

std::string Compressor::decompress(const std::vector<uint8_t>& compressed) {
    if (compressed.empty()) return "";

    unsigned long long content_size = ZSTD_getFrameContentSize(compressed.data(), compressed.size());
    TITAN_ASSERT(content_size != ZSTD_CONTENTSIZE_UNKNOWN, "unknown content size");
    TITAN_ASSERT(content_size != ZSTD_CONTENTSIZE_ERROR, "invalid compressed data");

    constexpr unsigned long long MAX_DECOMPRESS = 1024ULL * 1024 * 1024;
    TITAN_ASSERT(content_size < MAX_DECOMPRESS, "decompressed size exceeds 1GB limit");

    std::string output;
    output.resize(content_size);

    size_t result = ZSTD_decompressDCtx(dctx_, output.data(), content_size,
                                         compressed.data(), compressed.size());
    TITAN_ASSERT(!ZSTD_isError(result),
        std::string("decompression failed: ") + ZSTD_getErrorName(result));

    return output;
}

} // namespace titan
