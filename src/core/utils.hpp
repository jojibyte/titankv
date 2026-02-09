#pragma once

#include <stdexcept>
#include <string>
#include <format>
#include <source_location>

namespace titan {

inline void TITAN_ASSERT(bool condition, const std::string& message, 
                        const std::source_location location = std::source_location::current()) {
    if (!condition) {
        throw std::runtime_error(std::format(
            "assertion failed: {} | {}:{}", 
            message, location.file_name(), location.line()
        ));
    }
}

class LoopGuard {
public:
    explicit LoopGuard(size_t max, const std::string& ctx)
        : max_(max), ctx_(ctx) {}

    void tick() {
        if (++count_ > max_) {
            throw std::runtime_error("loop limit exceeded: " + ctx_);
        }
    }

private:
    size_t max_;
    size_t count_ = 0;
    std::string ctx_;
};

} // namespace titan
