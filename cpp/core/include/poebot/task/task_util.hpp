#pragma once
#include <poebot/input/keyboard.hpp>
#include <poebot/sys/clipboard.hpp>
#include <poebot/task/task.hpp>

#include <chrono>
#include <optional>
#include <string>
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

}  // namespace poebot::task
