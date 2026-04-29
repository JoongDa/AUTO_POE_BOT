#include <poebot/gui/affix_library_widget.hpp>

#include <poebot/config/affix_library.hpp>
#include <poebot/i18n/i18n.hpp>
#include <poebot/sys/encoding.hpp>

#include <imgui.h>
#include <spdlog/spdlog.h>

#include <windows.h>   // ShellExecuteW (WIN32_LEAN_AND_MEAN is set project-wide)
#include <shellapi.h>

#include <cstdio>
#include <filesystem>
#include <map>
#include <string>
#include <system_error>

namespace poebot::gui {

namespace {

constexpr const char* kPoeReURL = "https://poe.re/";

// Fire-and-forget URL / file open via the shell. Both the affix file and the
// poe.re link share the same code path — the shell figures out whether to
// hand off to the default text editor (.txt → notepad/VSCode) or the default
// browser (https → browser). Failures are silent: this is a convenience
// trigger, not a critical path.
void shellOpen(const std::wstring& target) {
    ::ShellExecuteW(nullptr, L"open", target.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}
void shellOpen(const char* utf8) {
    int n = ::MultiByteToWideChar(CP_UTF8, 0, utf8, -1, nullptr, 0);
    if (n <= 0) return;
    std::wstring w(static_cast<size_t>(n - 1), L'\0');
    ::MultiByteToWideChar(CP_UTF8, 0, utf8, -1, w.data(), n);
    shellOpen(w);
}
void shellOpen(const std::filesystem::path& target) {
#ifdef _WIN32
    shellOpen(target.wstring());
#else
    shellOpen(target.string().c_str());
#endif
}

std::filesystem::path libraryFilePath(const std::filesystem::path& dir,
                                      std::string_view name) {
    std::filesystem::path path = dir;
    const std::string filename = std::string(name) + ".txt";
#ifdef _WIN32
    path /= poebot::sys::utf8ToWide(filename);
#else
    path /= filename;
#endif
    return path;
}

// Render text that looks and acts like a browser link: accent color,
// underline on hover, pointer cursor, click opens the URL.
void renderLink(const char* visibleText, const char* url) {
    const ImVec4 linkColor(0.20f, 0.55f, 0.98f, 1.0f);  // macOS systemBlue-ish
    ImGui::PushStyleColor(ImGuiCol_Text, linkColor);
    ImGui::TextUnformatted(visibleText);
    ImGui::PopStyleColor();

    if (ImGui::IsItemHovered()) {
        ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
        const ImVec2 lo = ImGui::GetItemRectMin();
        const ImVec2 hi = ImGui::GetItemRectMax();
        ImGui::GetWindowDrawList()->AddLine(
            ImVec2(lo.x, hi.y), ImVec2(hi.x, hi.y),
            ImGui::GetColorU32(linkColor));
    }
    if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
        shellOpen(url);
    }
}

// Count newlines in a buffer for the dropdown's "(N)" suffix and the inline
// status line. One pass, no allocation. Empty buffer reads as 0 rules.
int countLines(const std::string& s) {
    if (s.empty()) return 0;
    int n = 1;
    for (char c : s) if (c == '\n') ++n;
    // Trailing '\n' shouldn't inflate the count (text files conventionally
    // end with one). Treat the post-'\n' empty tail as not-a-rule.
    if (s.back() == '\n') --n;
    return n;
}

}  // namespace

void affixLibraryWidget(const std::filesystem::path& dir,
                        std::string& selected,
                        std::string& content,
                        const char* idScope,
                        bool* outChanged) {
    using poebot::i18n::tr;
    namespace al = poebot::config;

    bool changed = false;
    ImGui::PushID(idScope);

    // Refresh library list every frame — directory of a few tiny .txt files,
    // free relative to the rest of frame work, and avoids any "did the user
    // create/delete/rename a file outside the app?" staleness.
    const auto libs = dir.empty() ? std::vector<std::string>{}
                                  : al::listAffixLibraries(dir);
    const bool dirUsable = !dir.empty();

    // --- File watcher ----------------------------------------------------
    // The user edits libraries in their preferred external editor (notepad,
    // VSCode, …). On every frame, compare the cached mtime of the bound file
    // with what's on disk; on change, re-read into `content` and surface a
    // log line so the user sees the round-trip succeeded.
    //
    // The map is keyed by (idScope, name) so craft and map watchers don't
    // collide even if both happen to bind a same-named library. First sighting
    // of a key reloads unconditionally — that's how we pick up edits made
    // while the program was closed (settings.json may carry stale `content`
    // from the previous session).
    static std::map<std::string, std::filesystem::file_time_type> watchTimes;
    if (!selected.empty() && dirUsable) {
        const auto path = libraryFilePath(dir, selected);
        std::error_code ec;
        const auto t = std::filesystem::last_write_time(path, ec);
        if (!ec) {
            const std::string key = std::string(idScope) + "/" + selected;
            auto it = watchTimes.find(key);
            const bool firstSeen = (it == watchTimes.end());
            const bool mtimeMoved = !firstSeen && (it->second != t);
            if (firstSeen || mtimeMoved) {
                watchTimes[key] = t;
                content = al::loadAffixLibrary(dir, selected);
                if (mtimeMoved) {
                    spdlog::info("affix_lib: '{}' reloaded from disk ({} bytes)",
                                 selected, content.size());
                    changed = true;  // mark settings dirty so cache catches up
                }
            }
        }
    }

    // --- Row 1: label + library combo + status --------------------------
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(tr("affix_lib.label"));
    ImGui::SameLine();

    ImGui::SetNextItemWidth(220.0f);
    const char* comboPreview = selected.empty()
        ? tr("affix_lib.none")
        : selected.c_str();
    if (ImGui::BeginCombo("##lib", comboPreview)) {
        // "(none)" clears the selection (disables matching for this panel
        // until the user picks again).
        const bool noneSel = selected.empty();
        if (ImGui::Selectable(tr("affix_lib.none"), noneSel)) {
            if (!noneSel) {
                selected.clear();
                content.clear();
                changed = true;
            }
        }
        if (!libs.empty()) ImGui::Separator();
        for (const auto& n : libs) {
            const bool isSel = (selected == n);
            // Right-align line count for a quick sense of library size.
            const std::string body = al::loadAffixLibrary(dir, n);
            const std::string label = n + "  (" + std::to_string(countLines(body)) + ")";
            if (ImGui::Selectable(label.c_str(), isSel)) {
                if (!isSel) {
                    selected = n;
                    content  = body;
                    changed  = true;
                }
            }
        }
        ImGui::EndCombo();
    }

    // Inline rule count next to the combo so the user can tell at a glance
    // whether the bound library is empty or full.
    if (!selected.empty()) {
        ImGui::SameLine();
        char rules[64];
        std::snprintf(rules, sizeof(rules), tr("affix_lib.lines_fmt"),
                      countLines(content));
        ImGui::TextDisabled("%s", rules);
    }

    // --- Row 2: action buttons -------------------------------------------
    // Edit  → open the bound .txt with the system default text editor
    // Reload → force re-read from disk (manual nudge if file watcher missed)
    // Folder → reveal the affix_libraries directory in Explorer; user does
    //          new / rename / delete / copy via the file system (everyone
    //          who runs a Bot is comfortable with that, and the app doesn't
    //          need to grow its own file manager).
    ImGui::BeginDisabled(!dirUsable);

    ImGui::BeginDisabled(selected.empty());
    if (ImGui::Button(tr("affix_lib.edit"))) {
        const auto path = libraryFilePath(dir, selected);
        shellOpen(path);
    }
    ImGui::SameLine();
    if (ImGui::Button(tr("affix_lib.reload"))) {
        content = al::loadAffixLibrary(dir, selected);
        changed = true;
        spdlog::info("affix_lib: '{}' manually reloaded ({} bytes)",
                     selected, content.size());
    }
    ImGui::EndDisabled();  // selected.empty()

    ImGui::SameLine();
    if (ImGui::Button(tr("affix_lib.folder"))) {
        // ensureAffixLibraryDir is cheap and idempotent — guarantees the
        // user gets a real folder even if onActiveProfileChanged hasn't
        // run yet for this scope.
        al::ensureAffixLibraryDir(dir);
        shellOpen(dir);
    }

    ImGui::EndDisabled();  // !dirUsable

    // --- Row 3: poe.re hint ---------------------------------------------
    ImGui::TextDisabled("%s", tr("affix_lib.poere_prefix"));
    ImGui::SameLine(0.0f, 4.0f);
    renderLink(kPoeReURL, kPoeReURL);

    ImGui::PopID();
    if (outChanged) *outChanged = changed;
}

}  // namespace poebot::gui
