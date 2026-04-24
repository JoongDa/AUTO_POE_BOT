#include <poebot/gui/panels/log_panel.hpp>

#include <poebot/i18n/i18n.hpp>

#include <imgui.h>

#include <algorithm>

namespace poebot::gui::panels {

namespace {

// spdlog level values: trace=0, debug=1, info=2, warn=3, err=4, critical=5.
// Info uses the current theme's text color so it reads correctly on both
// Light and Dark backgrounds; everything else is a saturated hue that stays
// legible against either.
ImVec4 colorForLevel(int lvl) {
    switch (lvl) {
        case 0:  return ImVec4(0.55f, 0.55f, 0.55f, 1.0f);  // trace — grey
        case 1:  return ImVec4(0.20f, 0.55f, 0.95f, 1.0f);  // debug — blue
        case 2:  return ImGui::GetStyle().Colors[ImGuiCol_Text];  // info — theme default
        case 3:  return ImVec4(0.90f, 0.60f, 0.00f, 1.0f);  // warn — orange
        case 4:  return ImVec4(0.90f, 0.20f, 0.20f, 1.0f);  // error — red
        case 5:  return ImVec4(0.90f, 0.15f, 0.50f, 1.0f);  // crit — magenta
        default: return ImGui::GetStyle().Colors[ImGuiCol_Text];
    }
}

// Paint one run of bytes. Emits SameLine(0,0) between segments on the same
// logical line so the highlighted and normal spans flow inline.
void emitSegment(const char* begin, const char* end, ImVec4 color, bool& firstOnLine) {
    if (begin >= end) return;
    if (!firstOnLine) ImGui::SameLine(0.0f, 0.0f);
    ImGui::PushStyleColor(ImGuiCol_Text, color);
    ImGui::TextUnformatted(begin, end);
    ImGui::PopStyleColor();
    firstOnLine = false;
}

// Render one logical line [lineStart, lineEnd) of `text`, colouring byte
// ranges that fall inside highlight spans with `hit`, the rest with `base`.
// `hs` must be sorted by offset.
void renderLineWithHighlights(const std::string& text,
                              std::size_t lineStart, std::size_t lineEnd,
                              const std::vector<poebot::log::LogHighlight>& hs,
                              ImVec4 base, ImVec4 hit) {
    std::size_t pos = lineStart;
    bool firstOnLine = true;

    for (const auto& h : hs) {
        const std::size_t hStart = h.offset;
        const std::size_t hEnd   = h.offset + h.length;
        if (hEnd <= pos) continue;         // already past this highlight
        if (hStart >= lineEnd) break;      // highlight starts after this line

        const std::size_t segStart = std::max(hStart, pos);
        const std::size_t segEnd   = std::min(hEnd, lineEnd);

        if (segStart > pos) {
            emitSegment(text.data() + pos, text.data() + segStart, base, firstOnLine);
        }
        emitSegment(text.data() + segStart, text.data() + segEnd, hit, firstOnLine);
        pos = segEnd;
    }
    if (pos < lineEnd) {
        emitSegment(text.data() + pos, text.data() + lineEnd, base, firstOnLine);
    }
    if (firstOnLine) {
        // Blank line — still needs a line break so subsequent lines don't
        // collapse upward.
        ImGui::NewLine();
    }
}

void renderEntry(const poebot::log::LogEntry& e) {
    const ImVec4 base = colorForLevel(e.level);

    if (e.highlights.empty()) {
        ImGui::PushStyleColor(ImGuiCol_Text, base);
        ImGui::TextUnformatted(e.text.c_str());
        ImGui::PopStyleColor();
        return;
    }

    // Highlight color: a saturated yellow that reads on both Light and Dark
    // without clashing with warn/err rows (those don't carry highlights).
    const ImVec4 hit(1.00f, 0.82f, 0.20f, 1.0f);

    // Sort highlights by offset (stable across render calls — done once per
    // entry per frame; entries with highlights are rare so cost is trivial).
    auto hs = e.highlights;
    std::sort(hs.begin(), hs.end(),
              [](const poebot::log::LogHighlight& a, const poebot::log::LogHighlight& b) {
                  return a.offset < b.offset;
              });

    // Walk line-by-line so newlines in the clipboard text stay as line breaks.
    const std::string& text = e.text;
    std::size_t lineStart = 0;
    for (std::size_t i = 0; i <= text.size(); ++i) {
        const bool atEnd = (i == text.size());
        if (atEnd || text[i] == '\n') {
            renderLineWithHighlights(text, lineStart, i, hs, base, hit);
            lineStart = i + 1;
        }
    }
}

}  // namespace

void LogPanel::render(PanelContext& ctx) {
    using poebot::i18n::tr;
    ImGui::TextUnformatted(tr("log.title"));
    ImGui::SameLine();
    ImGui::Checkbox(tr("log.auto_scroll"), &autoScroll_);
    ImGui::SameLine();
    if (ImGui::Button(tr("log.clear")) && ctx.logSink) {
        ctx.logSink->clear();
        lastSize_ = 0;
    }
    ImGui::SameLine();
    if (ctx.logSink) {
        ImGui::Text(tr("log.entries_fmt"), ctx.logSink->size());
    }
    ImGui::Separator();

    ImGui::BeginChild("##LogScroll", ImVec2(0, 0), false,
                      ImGuiWindowFlags_HorizontalScrollbar);
    if (ctx.logSink) {
        auto entries = ctx.logSink->snapshot();
        for (const auto& e : entries) {
            renderEntry(e);
        }
        if (autoScroll_ && entries.size() != lastSize_) {
            ImGui::SetScrollHereY(1.0f);
            lastSize_ = entries.size();
        }
    }
    ImGui::EndChild();
}

}  // namespace poebot::gui::panels
