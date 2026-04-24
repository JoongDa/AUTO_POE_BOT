#include <poebot/log/log_sink.hpp>

#include <spdlog/pattern_formatter.h>

#include <atomic>
#include <utility>

namespace poebot::log {

namespace {
// Active sink pointer for the free-function push(). Owned by App; tasks on
// worker threads read it. atomic so cross-thread publishes are safe without
// a mutex; the sink itself takes its own lock in push().
std::atomic<ImGuiSink*> g_activeSink{nullptr};
}  // namespace

void setActiveSink(ImGuiSink* sink) {
    g_activeSink.store(sink, std::memory_order_release);
}

void push(LogEntry e) {
    if (auto* s = g_activeSink.load(std::memory_order_acquire)) {
        s->push(std::move(e));
    }
}

ImGuiSink::ImGuiSink(std::size_t capacity)
    : capacity_(capacity == 0 ? 1 : capacity) {}

void ImGuiSink::sink_it_(const spdlog::details::log_msg& msg) {
    // Format the message using base_sink's configured formatter.
    spdlog::memory_buf_t formatted;
    spdlog::sinks::base_sink<spdlog::details::null_mutex>::formatter_->format(msg, formatted);

    std::string text(formatted.data(), formatted.size());
    // Formatters usually append a trailing newline; strip it so ImGui can
    // render each entry as its own line.
    if (!text.empty() && text.back() == '\n') text.pop_back();
    if (!text.empty() && text.back() == '\r') text.pop_back();

    LogEntry entry;
    entry.text = std::move(text);
    entry.level = static_cast<int>(msg.level);

    std::lock_guard<std::mutex> lock(mu_);
    buffer_.push_back(std::move(entry));
    while (buffer_.size() > capacity_) {
        buffer_.pop_front();
    }
}

std::vector<LogEntry> ImGuiSink::snapshot() const {
    std::lock_guard<std::mutex> lock(mu_);
    return std::vector<LogEntry>(buffer_.begin(), buffer_.end());
}

void ImGuiSink::clear() {
    std::lock_guard<std::mutex> lock(mu_);
    buffer_.clear();
}

std::size_t ImGuiSink::size() const {
    std::lock_guard<std::mutex> lock(mu_);
    return buffer_.size();
}

void ImGuiSink::push(LogEntry e) {
    std::lock_guard<std::mutex> lock(mu_);
    buffer_.push_back(std::move(e));
    while (buffer_.size() > capacity_) {
        buffer_.pop_front();
    }
}

}  // namespace poebot::log
