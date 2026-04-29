#pragma once
#include <poebot/input/keyboard.hpp>
#include <poebot/input/motion.hpp>
#include <poebot/sys/clipboard.hpp>
#include <poebot/task/task.hpp>

#include <array>
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

// Parse the current stack count out of a copied currency tooltip ("Stack
// Size: 123/20", or "1,234/20"). Tries each known label so non-English
// PoE clients work too — extend the array if a locale we don't cover
// shows up. Returns nullopt on parse failure.
inline std::optional<int> parseStackSize(std::string_view text) {
    // Order doesn't matter for correctness; the English label leads
    // simply because it's the most common in our dataset.
    static constexpr std::array<std::string_view, 4> kKeys = {
        "Stack Size:",     // English (international PoE 1/2)
        "堆叠数量:",       // Simplified Chinese
        "堆疊數量:",       // Traditional Chinese
        "堆疊大小:",       // Traditional Chinese (alt)
    };

    std::size_t pos    = std::string_view::npos;
    std::size_t keyLen = 0;
    for (const auto& k : kKeys) {
        const auto p = text.find(k);
        if (p != std::string_view::npos) { pos = p; keyLen = k.size(); break; }
    }
    if (pos == std::string_view::npos) return std::nullopt;

    std::size_t i = pos + keyLen;
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

// Hover a currency stack, Ctrl+C, parse "Stack Size". Returns nullopt on
// timeout/stop/parse failure.
//
// Earlier versions did an up-then-back jiggle to "force" the tooltip to
// refresh. Removed: PoE redraws the tooltip on hover-enter regardless,
// and the visible cursor wobble was distracting in-game. A single move
// + 150 ms settle is enough on every test we've run.
inline std::optional<int> readOrbStack(
    std::atomic<bool>& stop,
    ScreenPoint orbSP,
    std::chrono::milliseconds timeout = std::chrono::milliseconds(500)) {
    if (!poebot::input::motion::bezierMoveTo(stop, orbSP)) return std::nullopt;
    if (!interruptibleSleep(stop, std::chrono::milliseconds(150))) return std::nullopt;

    auto clip = readItemClipboard(stop, timeout);
    if (!clip) return std::nullopt;
    return parseStackSize(*clip);
}

}  // namespace poebot::task
