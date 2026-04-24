#include <poebot/config/affix_library.hpp>

#include <spdlog/spdlog.h>

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>
#include <system_error>

namespace poebot::config {

namespace {

// Path extension used for every library file. Kept as a constant so any
// future change to format (e.g. switch to .json) touches exactly one line.
constexpr const char* kLibExt = ".txt";

// Case-insensitive less-than for stable sorted dropdown display.
bool iless(const std::string& a, const std::string& b) {
    return std::lexicographical_compare(
        a.begin(), a.end(), b.begin(), b.end(),
        [](unsigned char x, unsigned char y) {
            return std::tolower(x) < std::tolower(y);
        });
}

std::filesystem::path pathFor(const std::filesystem::path& dir,
                              std::string_view name) {
    std::filesystem::path p = dir;
    p /= std::string(name) + kLibExt;
    return p;
}

}  // namespace

bool ensureAffixLibraryDir(const std::filesystem::path& dir) {
    std::error_code ec;
    if (std::filesystem::exists(dir, ec) && std::filesystem::is_directory(dir, ec)) {
        return true;
    }
    std::filesystem::create_directories(dir, ec);
    if (ec) {
        spdlog::warn("affix_library: mkdir failed {}: {}", dir.string(), ec.message());
        return std::filesystem::exists(dir);  // last chance (race-tolerant)
    }
    return true;
}

std::vector<std::string> listAffixLibraries(const std::filesystem::path& dir) {
    std::vector<std::string> out;
    std::error_code ec;
    if (!std::filesystem::exists(dir, ec) || !std::filesystem::is_directory(dir, ec)) {
        return out;
    }
    for (const auto& entry : std::filesystem::directory_iterator(dir, ec)) {
        if (ec) break;
        if (!entry.is_regular_file()) continue;
        const auto& p = entry.path();
        if (p.extension() != kLibExt) continue;
        out.push_back(p.stem().string());
    }
    std::sort(out.begin(), out.end(), iless);
    return out;
}

std::string loadAffixLibrary(const std::filesystem::path& dir,
                             std::string_view name) {
    if (!isValidAffixLibraryName(name)) return {};
    std::ifstream f(pathFor(dir, name), std::ios::binary);
    if (!f) return {};
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

bool saveAffixLibrary(const std::filesystem::path& dir,
                      std::string_view name,
                      std::string_view content) {
    if (!isValidAffixLibraryName(name)) {
        spdlog::warn("affix_library: save rejected — invalid name '{}'", std::string(name));
        return false;
    }
    if (!ensureAffixLibraryDir(dir)) return false;

    // Atomic write: tmp + rename. Mirrors saveSettings() so a crash mid-
    // write can't leave the user with a half-truncated library file.
    std::filesystem::path final = pathFor(dir, name);
    std::filesystem::path tmp   = final;
    tmp += ".tmp";

    try {
        {
            std::ofstream f(tmp, std::ios::binary | std::ios::trunc);
            if (!f) {
                spdlog::warn("affix_library: cannot open {}", tmp.string());
                return false;
            }
            f.write(content.data(), static_cast<std::streamsize>(content.size()));
            if (!f) {
                spdlog::warn("affix_library: write failed {}", tmp.string());
                return false;
            }
        }  // close flushes to disk

        std::error_code ec;
        std::filesystem::rename(tmp, final, ec);
        if (ec) {
            // rename can fail on Windows if target exists on some filesystems;
            // remove and retry once before giving up.
            std::filesystem::remove(final, ec);
            std::filesystem::rename(tmp, final, ec);
        }
        if (ec) {
            spdlog::warn("affix_library: rename {} -> {} failed: {}",
                         tmp.string(), final.string(), ec.message());
            std::error_code rm_ec;
            std::filesystem::remove(tmp, rm_ec);
            return false;
        }
        return true;
    } catch (const std::exception& e) {
        spdlog::warn("affix_library: save exception: {}", e.what());
        return false;
    }
}

bool renameAffixLibrary(const std::filesystem::path& dir,
                        std::string_view from,
                        std::string_view to) {
    if (!isValidAffixLibraryName(from) || !isValidAffixLibraryName(to)) return false;
    if (from == to) return true;
    std::error_code ec;
    auto src = pathFor(dir, from);
    auto dst = pathFor(dir, to);
    if (!std::filesystem::exists(src, ec)) return false;
    if (std::filesystem::exists(dst, ec)) return false;  // don't silently clobber
    std::filesystem::rename(src, dst, ec);
    if (ec) {
        spdlog::warn("affix_library: rename {} -> {} failed: {}",
                     src.string(), dst.string(), ec.message());
        return false;
    }
    return true;
}

bool deleteAffixLibrary(const std::filesystem::path& dir,
                        std::string_view name) {
    if (!isValidAffixLibraryName(name)) return false;
    std::error_code ec;
    auto p = pathFor(dir, name);
    if (!std::filesystem::exists(p, ec)) return true;  // already gone
    std::filesystem::remove(p, ec);
    if (ec) {
        spdlog::warn("affix_library: remove {} failed: {}", p.string(), ec.message());
        return false;
    }
    return true;
}

bool isValidAffixLibraryName(std::string_view name) {
    if (name.empty() || name.size() > 64) return false;
    // Forbid the Windows reserved characters plus path separators. We don't
    // need to worry about reserved device names (CON, PRN, …) because the
    // library lives in a subdirectory, not in the root, which neutralizes
    // the legacy device-name resolution.
    for (char ch : name) {
        switch (ch) {
            case '\\': case '/':  case ':': case '*':
            case '?':  case '"':  case '<': case '>':
            case '|':  case '\0':
                return false;
            default: break;
        }
        if (static_cast<unsigned char>(ch) < 0x20) return false;  // control chars
    }
    // Windows strips trailing dots/spaces on rename — refuse up-front.
    if (name.back() == '.' || name.back() == ' ') return false;
    if (name.front() == ' ') return false;
    return true;
}

}  // namespace poebot::config
