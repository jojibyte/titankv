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

} // namespace titan
