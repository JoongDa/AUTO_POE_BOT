#pragma once

#include <filesystem>
#include <system_error>

namespace poebot::config::detail {

// Replace an already-written temp file with the final path. On Windows, some
// filesystems reject rename() when the target exists; remove and retry once.
//
// On failure, `ec` reflects the LAST rename attempt — not the first one.
// The first rename's error is intentionally discarded after we've handled
// it by retrying; callers see the "the rename truly couldn't go through"
// reason which is the more actionable one for logs.
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
