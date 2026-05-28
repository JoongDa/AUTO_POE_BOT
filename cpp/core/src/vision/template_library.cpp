#include <poebot/vision/template_library.hpp>

#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <cctype>
#include <cstring>
#include <fstream>
#include <system_error>

namespace poebot::vision {

namespace {

// Convert any OpenCV Mat to packed 4-channel BGRA ImageBGRA.
ImageBGRA matToImageBGRA(const cv::Mat& src) {
    if (src.empty()) return {};

    cv::Mat bgra;
    switch (src.channels()) {
        case 4: bgra = src;                                    break;
        case 3: cv::cvtColor(src, bgra, cv::COLOR_BGR2BGRA);  break;
        case 1: cv::cvtColor(src, bgra, cv::COLOR_GRAY2BGRA); break;
        default: return {};
    }

    ImageBGRA out;
    out.width  = bgra.cols;
    out.height = bgra.rows;
    out.stride = bgra.cols * 4;
    out.pixels.resize(static_cast<std::size_t>(bgra.cols) * bgra.rows * 4);

    for (int y = 0; y < bgra.rows; ++y) {
        std::memcpy(
            out.pixels.data() + static_cast<std::size_t>(y) * out.stride,
            bgra.data         + static_cast<std::size_t>(y) * bgra.step[0],
            static_cast<std::size_t>(bgra.cols) * 4);
    }
    return out;
}

// Case-insensitive extension check — Windows filenames are case-insensitive,
// so .PNG and .png should both be picked up.
bool hasPngExt(const std::filesystem::path& p) {
    auto ext = p.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return ext == ".png";
}

}  // namespace

bool TemplateLibrary::load(const std::filesystem::path& templatesDir) {
    std::error_code ec;

    // Auto-create the directory on first run (or after the user deleted it).
    // create_directories is a no-op when the path already exists, so this is
    // always safe to call. Failure here (e.g. permission denied) is a hard
    // stop — we can't scan or write templates without the directory.
    std::filesystem::create_directories(templatesDir, ec);
    if (ec) {
        spdlog::warn("template_library: cannot create '{}': {}",
                     templatesDir.string(), ec.message());
        return false;
    }
    if (!std::filesystem::is_directory(templatesDir, ec)) {
        spdlog::warn("template_library: '{}' exists but is not a directory",
                     templatesDir.string());
        return false;
    }

    // Scan — collect PNG paths before touching any member state so that
    // error paths never leave the object half-updated.
    std::vector<std::filesystem::path> pngPaths;
    for (auto it = std::filesystem::directory_iterator(templatesDir, ec);
         it != std::filesystem::directory_iterator(); ++it) {
        if (ec) {
            spdlog::warn("template_library: scan error: {}", ec.message());
            break;
        }
        if (!it->is_regular_file(ec)) continue;
        if (!hasPngExt(it->path())) continue;
        pngPaths.push_back(it->path());
    }

    // Commit the directory path now — after filesystem checks pass but before
    // loading images. This ensures replaceTemplate() can write into the
    // correct folder even when the directory is currently empty (no PNGs yet).
    dir_ = templatesDir;
    entries_.clear();

    if (pngPaths.empty()) {
        spdlog::warn("template_library: no .png files in '{}' — "
                     "directory created and ready for templates",
                     templatesDir.string());
        return false;
    }

    // Stable sort for consistent UI order (directory_iterator is unspecified).
    std::sort(pngPaths.begin(), pngPaths.end());
    entries_.reserve(pngPaths.size());

    bool anyOk = false;
    for (const auto& path : pngPaths) {
        TemplateEntry e;
        e.name = path.stem().string();

        const cv::Mat mat = cv::imread(path.string(), cv::IMREAD_UNCHANGED);
        if (mat.empty()) {
            spdlog::warn("template_library: '{}' decode failed", path.string());
        } else {
            e.image  = matToImageBGRA(mat);
            e.loaded = !e.image.pixels.empty();
            if (e.loaded) {
                spdlog::info("template_library: '{}' {}x{}",
                             e.name, e.image.width, e.image.height);
                anyOk = true;
            }
        }
        entries_.push_back(std::move(e));
    }

    if (!anyOk) {
        spdlog::warn("template_library: no templates loaded from '{}'",
                     templatesDir.string());
    }
    return anyOk;
}

bool TemplateLibrary::anyLoaded() const noexcept {
    return std::any_of(entries_.begin(), entries_.end(),
                       [](const TemplateEntry& e) { return e.loaded; });
}

bool TemplateLibrary::replaceTemplate(const std::string& name,
                                       const ImageBGRA&   image) {
    if (dir_.empty() || image.pixels.empty()) {
        spdlog::warn("template_library: replaceTemplate '{}' — empty dir or image", name);
        return false;
    }

    std::error_code ec;
    std::filesystem::create_directories(dir_, ec);
    if (ec) {
        spdlog::error("template_library: cannot create dir '{}': {}",
                      dir_.string(), ec.message());
        return false;
    }

    // Build a cv::Mat view over the BGRA buffer (zero-copy).
    cv::Mat mat(image.height, image.width, CV_8UC4,
                const_cast<uint8_t*>(image.pixels.data()),
                static_cast<std::size_t>(image.stride));

    std::vector<uchar> buf;
    if (!cv::imencode(".png", mat, buf)) {
        spdlog::error("template_library: PNG encode failed for '{}'", name);
        return false;
    }

    // Atomic write: encode → tmp → rename.
    const auto dest = dir_ / (name + ".png");
    const auto tmp  = dir_ / (name + ".png.tmp");

    {
        std::ofstream f(tmp, std::ios::binary | std::ios::trunc);
        if (!f) {
            spdlog::error("template_library: cannot open '{}' for writing", tmp.string());
            return false;
        }
        f.write(reinterpret_cast<const char*>(buf.data()),
                static_cast<std::streamsize>(buf.size()));
        if (!f) {
            spdlog::error("template_library: write failed for '{}'", tmp.string());
            return false;
        }
    }

    std::filesystem::rename(tmp, dest, ec);
    if (ec) {
        spdlog::error("template_library: rename '{}' → '{}': {}",
                      tmp.string(), dest.string(), ec.message());
        std::filesystem::remove(tmp, ec);
        return false;
    }

    spdlog::info("template_library: saved '{}' ({}×{} px)",
                 dest.string(), image.width, image.height);
    return reload();
}

}  // namespace poebot::vision
