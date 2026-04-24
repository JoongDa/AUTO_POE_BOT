#pragma once
#include <poebot/coords.hpp>

#include <atomic>
#include <chrono>

namespace poebot::input::motion {

// ---- Randomness helpers ----------------------------------------------------
// Thin wrappers around a thread-local PRNG; safe to call from any thread.

// Box-Muller: returns a sample from N(0, stdDev).
double gaussRand(double stdDev);

// Returns round(mean + N(0, stdDev)), clamped to [floor, INT_MAX].
int gaussInt(double mean, double stdDev, int floor = 0);

// Uniform integer in [lo, hi] inclusive.
int uniformInt(int lo, int hi);

// Sleep for Gaussian-distributed duration; stdDev defaults to 15% of mean,
// floor defaults to 50% of mean (so we never sleep absurdly short). Checks
// `stop` on ~50ms ticks and returns early if set. Returns true if the full
// sleep elapsed (i.e. not stopped).
bool gaussSleep(std::atomic<bool>& stop,
                std::chrono::milliseconds mean,
                std::chrono::milliseconds stdDev = std::chrono::milliseconds(0),
                std::chrono::milliseconds floor  = std::chrono::milliseconds(0));

// ---- Mouse path ------------------------------------------------------------

// Move the OS cursor along a quadratic bezier with a random control point so
// the trajectory looks human. Step sleep is short (~1–2ms) so the whole
// move finishes in tens of ms but the path is no longer a straight teleport.
// Returns false if `stop` was set mid-move.
bool bezierMoveTo(std::atomic<bool>& stop, ScreenPoint target);

}  // namespace poebot::input::motion
