#include <poebot/vision/template_match.hpp>

#include <spdlog/spdlog.h>

#include <algorithm>
#include <cmath>

namespace poebot::vision {

namespace {

// Convert a BGRA pixel to grayscale luminance using Rec.601 weights.
// NCC operates on a single channel; using luminance instead of one
// arbitrary color channel keeps the matcher resilient against the
// slight color shifts PoE sometimes applies (e.g. the brief flash
// when an item is hovered).
inline float lumaAt(const ImageBGRA& img, int x, int y) {
    const std::uint8_t* p = &img.pixels[y * img.stride + x * 4];
    // BGRA layout from the BitBlt path → indices 2/1/0 = R/G/B.
    return 0.299f * p[2] + 0.587f * p[1] + 0.114f * p[0];
}

// Compute mean and inverse stddev of a region of `img`. Used for
// per-window NCC normalisation. Returns false (and zeros) if the
// region has near-zero variance — those degenerate windows can't be
// scored meaningfully and would otherwise produce divide-by-zero NaNs.
bool windowStats(const ImageBGRA& img, int x0, int y0, int tw, int th,
                 float& outMean, float& outInvStd) {
    double sum = 0.0;
    double sumSq = 0.0;
    const int n = tw * th;
    for (int y = 0; y < th; ++y) {
        for (int x = 0; x < tw; ++x) {
            const float v = lumaAt(img, x0 + x, y0 + y);
            sum   += v;
            sumSq += static_cast<double>(v) * v;
        }
    }
    const double mean = sum / n;
    const double var  = sumSq / n - mean * mean;
    if (var < 1e-3) {            // flat patch
        outMean   = static_cast<float>(mean);
        outInvStd = 0.0f;
        return false;
    }
    outMean   = static_cast<float>(mean);
    outInvStd = static_cast<float>(1.0 / std::sqrt(var));
    return true;
}

// NCC of `templ` placed at (x0, y0) in `haystack`. tplMean / tplInvStd
// are precomputed (template stats don't change between candidate windows).
float nccAt(const ImageBGRA& haystack,
            const ImageBGRA& templ,
            int x0, int y0,
            float tplMean, float tplInvStd) {
    const int tw = templ.width;
    const int th = templ.height;

    float winMean = 0.0f, winInvStd = 0.0f;
    if (!windowStats(haystack, x0, y0, tw, th, winMean, winInvStd)) {
        return 0.0f;
    }

    double dot = 0.0;
    for (int y = 0; y < th; ++y) {
        for (int x = 0; x < tw; ++x) {
            const float h = lumaAt(haystack, x0 + x, y0 + y) - winMean;
            const float t = lumaAt(templ,    x,      y     ) - tplMean;
            dot += static_cast<double>(h) * t;
        }
    }
    const int n = tw * th;
    return static_cast<float>(dot * winInvStd * tplInvStd / n);
}

// Sliding-window scan over `haystack` (bounded by `area`) computing NCC
// at every position; returns the best-scoring location. Caller has
// already validated dimensions.
MatchResult bestInArea(const ImageBGRA& haystack,
                       const ImageBGRA& templ,
                       const Rect&      area,
                       float            tplMean,
                       float            tplInvStd) {
    MatchResult best;
    best.score = -2.0f;  // any real NCC ∈ [-1, 1] beats this sentinel

    const int xMax = area.x + area.w - templ.width;
    const int yMax = area.y + area.h - templ.height;

    for (int y = area.y; y <= yMax; ++y) {
        for (int x = area.x; x <= xMax; ++x) {
            const float s = nccAt(haystack, templ, x, y, tplMean, tplInvStd);
            if (s > best.score) {
                best.score = s;
                best.x     = x;
                best.y     = y;
            }
        }
    }
    return best;
}

}  // namespace

ImageBGRA resize(const ImageBGRA& src, int newWidth, int newHeight) {
    ImageBGRA out;
    if (src.width <= 0 || src.height <= 0 ||
        newWidth   <= 0 || newHeight  <= 0) {
        return out;
    }
    out.width  = newWidth;
    out.height = newHeight;
    out.stride = newWidth * 4;
    out.pixels.assign(static_cast<std::size_t>(newWidth) * newHeight * 4, 0);

    // Standard bilinear: for each destination pixel, sample 4 source
    // pixels and weighted-average. Good enough at the small scales we
    // use here (|src/dst − 1| < 0.5) — at extreme rescales we'd want
    // a proper resampler, but multi-scale matching never asks for that.
    const float xRatio = static_cast<float>(src.width  - 1) / std::max(1, newWidth  - 1);
    const float yRatio = static_cast<float>(src.height - 1) / std::max(1, newHeight - 1);

    for (int dy = 0; dy < newHeight; ++dy) {
        const float sy = dy * yRatio;
        const int   y0 = static_cast<int>(sy);
        const int   y1 = std::min(y0 + 1, src.height - 1);
        const float fy = sy - y0;

        for (int dx = 0; dx < newWidth; ++dx) {
            const float sx = dx * xRatio;
            const int   x0 = static_cast<int>(sx);
            const int   x1 = std::min(x0 + 1, src.width - 1);
            const float fx = sx - x0;

            std::uint8_t* dstP = &out.pixels[dy * out.stride + dx * 4];
            for (int c = 0; c < 4; ++c) {
                const float p00 = src.pixels[y0 * src.stride + x0 * 4 + c];
                const float p10 = src.pixels[y0 * src.stride + x1 * 4 + c];
                const float p01 = src.pixels[y1 * src.stride + x0 * 4 + c];
                const float p11 = src.pixels[y1 * src.stride + x1 * 4 + c];

                const float top = p00 * (1 - fx) + p10 * fx;
                const float bot = p01 * (1 - fx) + p11 * fx;
                dstP[c] = static_cast<std::uint8_t>(top * (1 - fy) + bot * fy + 0.5f);
            }
        }
    }
    return out;
}

std::optional<MatchResult> match(
    const ImageBGRA& haystack,
    const ImageBGRA& templ,
    const Rect*      searchArea) {

    if (haystack.width  <= 0 || haystack.height <= 0 ||
        templ.width     <= 0 || templ.height    <= 0) {
        return std::nullopt;
    }
    if (templ.width > haystack.width || templ.height > haystack.height) {
        spdlog::warn("template_match: template ({}x{}) larger than haystack ({}x{})",
                     templ.width, templ.height,
                     haystack.width, haystack.height);
        return std::nullopt;
    }

    // Precompute template stats once — they're invariant across the scan.
    float tplMean = 0.0f, tplInvStd = 0.0f;
    if (!windowStats(templ, 0, 0, templ.width, templ.height,
                     tplMean, tplInvStd)) {
        spdlog::warn("template_match: template has no contrast; cannot match");
        return std::nullopt;
    }

    Rect area;
    if (searchArea) {
        // Clamp caller's area to image bounds so a slightly oversized
        // hint doesn't read OOB.
        area.x = std::max(0, searchArea->x);
        area.y = std::max(0, searchArea->y);
        area.w = std::min(haystack.width  - area.x, searchArea->w);
        area.h = std::min(haystack.height - area.y, searchArea->h);
    } else {
        area = Rect{0, 0, haystack.width, haystack.height};
    }
    if (area.w < templ.width || area.h < templ.height) {
        return std::nullopt;
    }

    auto best = bestInArea(haystack, templ, area, tplMean, tplInvStd);
    best.scale = 1.0f;
    return best;
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
