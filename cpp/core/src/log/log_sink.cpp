#include <poebot/log/log_sink.hpp>

#include <spdlog/pattern_formatter.h>

#include <utility>

namespace poebot::log {

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

}  // namespace poebot::log
