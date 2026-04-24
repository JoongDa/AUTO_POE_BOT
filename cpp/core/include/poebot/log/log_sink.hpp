#pragma once
#include <spdlog/details/null_mutex.h>
#include <spdlog/sinks/base_sink.h>

#include <cstddef>
#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

namespace poebot::log {

// Byte-range inside LogEntry::text that should be rendered with a highlight
// color. Used on craft/map hits so the matched affixes stand out in the
// full copied item text.
struct LogHighlight {
    std::size_t offset = 0;
    std::size_t length = 0;
};

struct LogEntry {
    std::string text;
    int level = 2;  // spdlog::level::info
    std::vector<LogHighlight> highlights;
};

// spdlog sink that stores recent log messages in a bounded ring buffer so the
// ImGui LogPanel can display them. Thread-safe: log calls from any thread,
// snapshot() from the UI thread.
class ImGuiSink : public spdlog::sinks::base_sink<spdlog::details::null_mutex> {
public:
    explicit ImGuiSink(std::size_t capacity = 1000);

    // Returns a copy of the current buffer. Safe to call from any thread.
    std::vector<LogEntry> snapshot() const;

    // Drop all buffered entries.
    void clear();

    // Current entry count (useful for scroll-to-bottom detection).
    std::size_t size() const;

    // Enqueue a pre-built entry. Used for structured events (e.g., a craft
    // "HIT" with the full clipboard text + match ranges) that don't fit the
    // plain-string spdlog path. Bypasses the formatter.
    void push(LogEntry e);

protected:
    void sink_it_(const spdlog::details::log_msg& msg) override;
    void flush_() override {}

private:
    std::size_t capacity_;
    mutable std::mutex mu_;
    std::deque<LogEntry> buffer_;
};

// Install / read the active sink so task code can publish structured entries
// without threading a sink pointer through every call site. The App owns the
// sink; it calls setActive(...) once on startup.
void  setActiveSink(ImGuiSink* sink);
void  push(LogEntry e);  // no-op if no active sink

}  // namespace poebot::log
