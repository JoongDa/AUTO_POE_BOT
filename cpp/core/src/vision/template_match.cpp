#include <poebot/vision/template_match.hpp>

#include <opencv2/imgproc.hpp>

#include <spdlog/spdlog.h>

#include <algorithm>
#include <cstring>

namespace poebot::vision {

namespace {

// Wrap an ImageBGRA pixel buffer as a cv::Mat without copying.
// The const_cast is safe: the resulting Mat is only ever read from.
cv::Mat toMat(const ImageBGRA& img) {
    return cv::Mat(img.height, img.width, CV_8UC4,
                   const_cast<uint8_t*>(img.pixels.data()),
                   static_cast<std::size_t>(img.stride));
}

}  // namespace

ImageBGRA resize(const ImageBGRA& src, int newWidth, int newHeight) {
    if (src.width <= 0 || src.height <= 0 || newWidth <= 0 || newHeight <= 0)
        return {};

    cv::Mat dst;
    cv::resize(toMat(src), dst, cv::Size(newWidth, newHeight), 0.0, 0.0, cv::INTER_LINEAR);

    ImageBGRA out;
    out.width  = newWidth;
    out.height = newHeight;
    out.stride = newWidth * 4;
    out.pixels.resize(static_cast<std::size_t>(newWidth) * newHeight * 4);
    // Copy row by row to respect dst.step[0] — OpenCV may pad rows for
    // alignment, so a single memcpy of the whole buffer would interleave
    // padding bytes into our pixel data.
    const std::size_t rowBytes = static_cast<std::size_t>(newWidth) * 4;
    for (int y = 0; y < newHeight; ++y) {
        std::memcpy(out.pixels.data() + y * out.stride,
                    dst.data + y * dst.step[0],
                    rowBytes);
    }
    return out;
}

std::optional<MatchResult> match(
    const ImageBGRA& haystack,
    const ImageBGRA& templ,
    const Rect*      searchArea) {

    if (haystack.width <= 0 || haystack.height <= 0 ||
        templ.width    <= 0 || templ.height    <= 0) {
        return std::nullopt;
    }
    if (templ.width > haystack.width || templ.height > haystack.height) {
        spdlog::warn("template_match: template ({}x{}) larger than haystack ({}x{})",
                     templ.width, templ.height, haystack.width, haystack.height);
        return std::nullopt;
    }

    // Convert both images to grayscale — single-channel is faster for
    // matchTemplate and consistent with the old NCC grayscale behavior.
    cv::Mat hayGray, tplGray;
    cv::cvtColor(toMat(haystack), hayGray, cv::COLOR_BGRA2GRAY);
    cv::cvtColor(toMat(templ),    tplGray, cv::COLOR_BGRA2GRAY);

    // Restrict the search to an optional sub-rect of the haystack.
    cv::Mat   roi      = hayGray;
    cv::Point roiOffset{0, 0};
    if (searchArea) {
        const int x = std::max(0, searchArea->x);
        const int y = std::max(0, searchArea->y);
        const int w = std::min(hayGray.cols - x, searchArea->w);
        const int h = std::min(hayGray.rows - y, searchArea->h);
        if (w < templ.width || h < templ.height) return std::nullopt;
        roi       = hayGray(cv::Rect(x, y, w, h));
        roiOffset = {x, y};
    }

    // TM_CCOEFF_NORMED scores are in [-1, 1]; 1.0 == perfect match.
    cv::Mat result;
    cv::matchTemplate(roi, tplGray, result, cv::TM_CCOEFF_NORMED);

    double    maxVal{};
    cv::Point maxLoc{};
    cv::minMaxLoc(result, nullptr, &maxVal, nullptr, &maxLoc);

    MatchResult mr;
    mr.x     = maxLoc.x + roiOffset.x;
    mr.y     = maxLoc.y + roiOffset.y;
    mr.score = static_cast<float>(maxVal);
    mr.scale = 1.0f;
    return mr;
}

std::optional<MatchResult> matchMultiScale(
    const ImageBGRA&          haystack,
    const ImageBGRA&          templ,
    const std::vector<float>& scales,
    const Rect*               searchArea) {

    if (scales.empty()) return match(haystack, templ, searchArea);

    std::optional<MatchResult> overall;
    for (const float s : scales) {
        const int sw = std::max(1, static_cast<int>(std::lround(templ.width  * s)));
        const int sh = std::max(1, static_cast<int>(std::lround(templ.height * s)));
        if (sw > haystack.width || sh > haystack.height) continue;

        const ImageBGRA scaled = (s == 1.0f) ? templ : resize(templ, sw, sh);
        if (scaled.width == 0) continue;

        auto r = match(haystack, scaled, searchArea);
        if (!r) continue;
        r->scale = s;
        if (!overall || r->score > overall->score) overall = r;
    }
    return overall;
}

}  // namespace poebot::vision
