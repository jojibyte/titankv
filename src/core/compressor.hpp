#pragma once

#include <vector>
#include <string>
#include <memory>
#include <cstdint>

typedef struct ZSTD_CCtx_s ZSTD_CCtx;
typedef struct ZSTD_DCtx_s ZSTD_DCtx;

namespace titan {

class Compressor {
public:
    Compressor();
    ~Compressor();

    Compressor(const Compressor&) = delete;
    Compressor& operator=(const Compressor&) = delete;

    std::vector<uint8_t> compress(const std::string& data, int level = 15);
    std::string decompress(const std::vector<uint8_t>& compressed);

private:
    ZSTD_CCtx* cctx_;
    ZSTD_DCtx* dctx_;
};

} // namespace titan
