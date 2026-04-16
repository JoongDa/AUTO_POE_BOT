#pragma once
#include <spdlog/details/null_mutex.h>
#include <spdlog/sinks/base_sink.h>

#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace poebot::log {

struct LogEntry {
    std::string text;
    int level = 2;  // spdlog::level::info
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

protected:
    void sink_it_(const spdlog::details::log_msg& msg) override;
    void flush_() override {}

private:
    std::size_t capacity_;
    mutable std::mutex mu_;
    std::deque<LogEntry> buffer_;
};

}  // namespace poebot::log
