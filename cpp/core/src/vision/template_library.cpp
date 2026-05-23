#include <poebot/vision/template_library.hpp>

#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <cstring>

namespace poebot::vision {

const std::vector<std::string>& TemplateLibrary::coordNames() {
    static const std::vector<std::string> kNames = {
        "orb1", "orb2", "orb3",
        "baseItem", "p01Item", "p10Item",
        "invBase",  "invP01",  "invP10",
    };
    return kNames;
}

namespace {

// Convert any OpenCV Mat to a packed 4-channel BGRA ImageBGRA.
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

}  // namespace

bool TemplateLibrary::load(const std::filesystem::path& templatesDir) {
    dir_ = templatesDir;
    entries_.clear();

    const auto& names = coordNames();
    entries_.reserve(names.size());

    bool anyOk = false;
    for (const auto& name : names) {
        TemplateEntry e;
        e.coordName = name;

        const auto path = templatesDir / (name + ".png");
        const cv::Mat mat = cv::imread(path.string(), cv::IMREAD_UNCHANGED);
        if (mat.empty()) {
            spdlog::debug("template_library: no file for '{}' ({})",
                          name, path.string());
        } else {
            e.image  = matToImageBGRA(mat);
            e.loaded = !e.image.pixels.empty();
            if (e.loaded) {
                spdlog::info("template_library: loaded '{}' ({}x{})",
                             name, e.image.width, e.image.height);
                anyOk = true;
            } else {
                spdlog::warn("template_library: '{}' convert failed ({} ch)",
                             name, mat.channels());
            }
        }
        entries_.push_back(std::move(e));
    }
    return anyOk;
}

bool TemplateLibrary::anyLoaded() const noexcept {
    return std::any_of(entries_.begin(), entries_.end(),
                       [](const TemplateEntry& e) { return e.loaded; });
}

}  // namespace poebot::vision
