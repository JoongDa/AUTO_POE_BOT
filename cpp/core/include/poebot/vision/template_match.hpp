#pragma once
#include <poebot/sys/screen_capture.hpp>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

// Template matching for the currency-locator pipeline. Given a captured
// screen frame and a currency icon template, find where the template sits
// in the frame and how confidently we matched.
//
// We hand-roll Normalised Cross-Correlation (NCC) instead of pulling in
// OpenCV: the algorithm is ~50 lines, sub-100 ms on a 64×64 template
// against a 1920×1080 frame, and avoids dragging an 80 MB dependency
// before we're sure we want it. When we get to YOLO (the project's
// memory-noted long-term plan), OpenCV becomes unavoidable and this
// module's interface stays the same — only the implementation swaps.
namespace poebot::vision {

// A 4-channel BGRA bitmap. Same layout produced by
// poebot::sys::CapturedImage but kept as its own type so callers
// constructing templates don't have to fake a "captured" image.
struct ImageBGRA {
    int                  width  = 0;
    int                  height = 0;
    int                  stride = 0;       // bytes/row, == width * 4
    std::vector<uint8_t> pixels;            // BGRA, top-down

    // Lightweight constructor from a captured frame. No copy of pixels —
    // the moved-from CapturedImage is consumed.
    static ImageBGRA fromCaptured(poebot::sys::CapturedImage&& src) {
        ImageBGRA out;
        out.width  = src.width;
        out.height = src.height;
        out.stride = src.stride;
        out.pixels = std::move(src.pixels);
        return out;
    }
};

struct MatchResult {
    int   x = 0;          // top-left of the matched region in haystack coords
    int   y = 0;
    float score = 0.0f;   // NCC score in [-1, 1]; 1.0 == perfect match
    float scale = 1.0f;   // Which template scale produced this match
                          // (multi-scale only; single-scale always 1.0).
};

// Single-scale match: scan `template` over every position in `haystack`
// and return the highest-scoring location. Returns nullopt if either
// image is empty or template is larger than haystack.
//
// `searchArea` optionally restricts the scan to a sub-rect of haystack
// (in haystack pixel coords). Useful when we know inventory grid
// boundaries and want to skip the rest of the screen — drops match
// time from ~100 ms to <5 ms on a 1920×1080 frame.
struct Rect { int x, y, w, h; };

std::optional<MatchResult> match(
    const ImageBGRA& haystack,
    const ImageBGRA& templ,
    const Rect*      searchArea = nullptr);

// Multi-scale match: tries each of the supplied scale factors against
// `templ` (resampled with simple bilinear), returns the best match
// across all scales. Pass `{1.0f}` for single-scale.
//
// The icon-scale self-learning path runs this once with a wide range
// (e.g. {0.7, 0.85, 1.0, 1.15, 1.3}) and caches the winning scale; the
// runtime path then uses single-scale match() with that cached factor.
std::optional<MatchResult> matchMultiScale(
    const ImageBGRA&          haystack,
    const ImageBGRA&          templ,
    const std::vector<float>& scales,
    const Rect*               searchArea = nullptr);

// Bilinear resample a template to a new size. Exposed so callers can
// pre-resize once and reuse the result instead of paying the bilinear
// cost on every match. Returns an empty image on degenerate sizes.
ImageBGRA resize(const ImageBGRA& src, int newWidth, int newHeight);

}  // namespace poebot::vision
