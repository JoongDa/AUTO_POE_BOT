#pragma once

#include <filesystem>
#include <system_error>

namespace poebot::config::detail {

// Replace an already-written temp file with the final path. On Windows, some
// filesystems reject rename() when the target exists; remove and retry once.
inline bool atomicReplaceFile(const std::filesystem::path& tmp,
                              const std::filesystem::path& final,
                              std::error_code& ec) {
    ec.clear();
    std::filesystem::rename(tmp, final, ec);
    if (!ec) return true;

    std::error_code remove_ec;
    std::filesystem::remove(final, remove_ec);

    ec.clear();
    std::filesystem::rename(tmp, final, ec);
    return !ec;
}

}  // namespace poebot::config::detail
