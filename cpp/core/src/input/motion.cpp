#include <poebot/input/motion.hpp>

#include <windows.h>

#include <algorithm>
#include <cmath>
#include <random>
#include <thread>

namespace poebot::input::motion {

namespace {

// Thread-local PRNG so tasks on different threads don't serialize on a shared
// generator. Seeded from random_device once per thread. Deterministic replay
// isn't a goal here — humanization is just about breaking perfect timing.
std::mt19937& rng() {
    thread_local std::mt19937 g{std::random_device{}()};
    return g;
}

}  // namespace

double gaussRand(double stdDev) {
    if (stdDev <= 0.0) return 0.0;
    std::normal_distribution<double> dist(0.0, stdDev);
    return dist(rng());
}

int gaussInt(double mean, double stdDev, int floor) {
    const double v = mean + gaussRand(stdDev);
    const int iv = static_cast<int>(std::lround(v));
    return std::max(iv, floor);
}

int uniformInt(int lo, int hi) {
    if (hi < lo) std::swap(lo, hi);
    std::uniform_int_distribution<int> dist(lo, hi);
    return dist(rng());
}

bool gaussSleep(std::atomic<bool>& stop,
                std::chrono::milliseconds mean,
                std::chrono::milliseconds stdDev,
                std::chrono::milliseconds floor) {
    using namespace std::chrono;
    const double meanMs = static_cast<double>(mean.count());
    const double stdMs  = stdDev.count() > 0 ? static_cast<double>(stdDev.count())
                                             : meanMs * 0.15;
    const int    flMs   = floor.count() > 0 ? static_cast<int>(floor.count())
                                            : static_cast<int>(std::lround(meanMs * 0.5));
    const int dur = gaussInt(meanMs, stdMs, flMs);

    constexpr auto kPoll = milliseconds(50);
    const auto end = steady_clock::now() + milliseconds(dur);
    while (steady_clock::now() < end) {
        if (stop.load(std::memory_order_relaxed)) return false;
        const auto remaining =
            duration_cast<milliseconds>(end - steady_clock::now());
        std::this_thread::sleep_for(std::min(remaining, kPoll));
    }
    return !stop.load(std::memory_order_relaxed);
}

bool bezierMoveTo(std::atomic<bool>& stop, ScreenPoint target) {
    POINT cur{};
    if (!::GetCursorPos(&cur)) {
        // Fallback: if we can't read the current position, just teleport.
        ::SetCursorPos(target.x, target.y);
        return !stop.load(std::memory_order_relaxed);
    }

    const int x1 = cur.x, y1 = cur.y;
    const int x2 = target.x, y2 = target.y;

    // Very short hops don't need a curve — avoids the case where the
    // random control point bounces the cursor visibly for a 5-pixel move.
    const int dx = x2 - x1, dy = y2 - y1;
    const double dist = std::sqrt(static_cast<double>(dx) * dx +
                                  static_cast<double>(dy) * dy);
    if (dist < 6.0) {
        ::SetCursorPos(x2, y2);
        return !stop.load(std::memory_order_relaxed);
    }

    // Step count scales with distance — long travel gets more samples so the
    // curve stays smooth, short travel stays snappy. Clamp both ends.
    const int steps = std::clamp(static_cast<int>(dist / 25.0), 6, 24);

    // Random control point offset from the midpoint. Larger offsets for longer
    // moves (proportional to distance) so the arc curvature looks consistent.
    const double offsetScale = std::clamp(dist * 0.25, 15.0, 60.0);
    const double cpX = (x1 + x2) / 2.0 + gaussRand(offsetScale);
    const double cpY = (y1 + y2) / 2.0 + gaussRand(offsetScale);

    for (int i = 1; i <= steps; ++i) {
        if (stop.load(std::memory_order_relaxed)) return false;

        const double t = static_cast<double>(i) / steps;
        const double omt = 1.0 - t;
        const double px = omt * omt * x1 + 2.0 * t * omt * cpX + t * t * x2;
        const double py = omt * omt * y1 + 2.0 * t * omt * cpY + t * t * y2;
        ::SetCursorPos(static_cast<int>(std::lround(px)),
                       static_cast<int>(std::lround(py)));

        // Ease-in/out: slower at start and end, faster in the middle. Total
        // motion runs ~30-80ms — short enough to feel responsive, long enough
        // to look like a deliberate mouse move rather than a teleport.
        const double ease = 1.0 + 3.0 * std::sin(t * 3.14159265);
        const int sleepMs = std::max(1, static_cast<int>(std::lround(ease)));
        std::this_thread::sleep_for(std::chrono::milliseconds(sleepMs));
    }

    // Final snap in case rounding drifted us by a pixel.
    ::SetCursorPos(x2, y2);
    return !stop.load(std::memory_order_relaxed);
}

}  // namespace poebot::input::motion
