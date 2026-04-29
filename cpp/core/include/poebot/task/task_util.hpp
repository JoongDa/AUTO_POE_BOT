#pragma once
#include <poebot/input/keyboard.hpp>
#include <poebot/input/motion.hpp>
#include <poebot/sys/clipboard.hpp>
#include <poebot/task/task.hpp>

#include <chrono>
#include <cctype>
#include <optional>
#include <string>
#include <string_view>
#include <thread>

namespace poebot::task {

// Clear clipboard → Ctrl+C → poll until non-empty text arrives.
// Returns nullopt on timeout or if stopRequested.  Used by CraftTask /
// MapTask after hovering over an item.
inline std::optional<std::string> readItemClipboard(
    std::atomic<bool>& stop,
    std::chrono::milliseconds timeout = std::chrono::milliseconds(500)) {
    poebot::sys::clipboard::clear();
    poebot::input::keyboard::sendCtrlC();

    const auto end = std::chrono::steady_clock::now() + timeout;
    while (std::chrono::steady_clock::now() < end) {
        if (stop.load(std::memory_order_relaxed)) return std::nullopt;
        auto text = poebot::sys::clipboard::readText();
        if (text && !text->empty()) return text;
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    return std::nullopt;
}

// Parse "Stack Size: 123/20" (or "Stack Size: 1,234/20") and return the
// current stack count (the first number). Returns nullopt on parse failure.
inline std::optional<int> parseStackSize(std::string_view text) {
    constexpr std::string_view kKey = "Stack Size:";
    const auto pos = text.find(kKey);
    if (pos == std::string_view::npos) return std::nullopt;

    std::size_t i = pos + kKey.size();
    while (i < text.size() && std::isspace(static_cast<unsigned char>(text[i]))) ++i;

    int value = 0;
    bool any = false;
    while (i < text.size()) {
        const unsigned char ch = static_cast<unsigned char>(text[i]);
        if (std::isdigit(ch)) {
            any = true;
            value = value * 10 + (ch - '0');
        } else if (ch == ',') {
            // thousands separator
        } else {
            break;
        }
        ++i;
    }
    if (!any) return std::nullopt;
    return value;
}

// Hover a currency stack, Ctrl+C, and parse "Stack Size". Returns nullopt on
// timeout/stop/parse failure.
inline std::optional<int> readOrbStack(
    std::atomic<bool>& stop,
    ScreenPoint orbSP,
    std::chrono::milliseconds timeout = std::chrono::milliseconds(500)) {
    // Nudge up then back to make sure the tooltip refreshes.
    if (!poebot::input::motion::bezierMoveTo(stop, ScreenPoint{orbSP.x, orbSP.y - 40})) {
        return std::nullopt;
    }
    if (!interruptibleSleep(stop, std::chrono::milliseconds(40))) return std::nullopt;
    if (!poebot::input::motion::bezierMoveTo(stop, orbSP)) return std::nullopt;
    if (!interruptibleSleep(stop, std::chrono::milliseconds(80))) return std::nullopt;

    auto clip = readItemClipboard(stop, timeout);
    if (!clip) return std::nullopt;
    return parseStackSize(*clip);
}

}  // namespace poebot::task
