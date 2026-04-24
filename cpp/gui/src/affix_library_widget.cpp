#include <poebot/gui/affix_library_widget.hpp>

#include <poebot/config/affix_library.hpp>
#include <poebot/i18n/i18n.hpp>

#include <imgui.h>
#include <spdlog/spdlog.h>

#include <windows.h>   // ShellExecuteA (WIN32_LEAN_AND_MEAN is set project-wide)
#include <shellapi.h>

#include <algorithm>
#include <cstdio>
#include <cstring>

namespace poebot::gui {

namespace {

constexpr size_t kNameBufCap = 96;   // matches isValidAffixLibraryName's 64 + slack
constexpr const char* kPoeReURL = "https://poe.re/";

// Fire-and-forget URL open. Returns silently on failure — the link is a
// convenience, not a critical path, so we don't raise anything in-UI.
void openExternalURL(const char* url) {
    ::ShellExecuteA(nullptr, "open", url, nullptr, nullptr, SW_SHOWNORMAL);
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
        openExternalURL(url);
    }
}

// Open a named modal by ID. ImGui requires BeginPopupModal to be called
// unconditionally every frame; we only call OpenPopup on the triggering
// frame and drive visibility through ImGui's internal state.
void openModal(const char* id) { ImGui::OpenPopup(id); }

// Small helper: draw a centered modal with a single-line name input and
// OK/Cancel. Returns true on OK with a valid name; fills `outName` with the
// user's input (truncated to kNameBufCap). `initialName` prefills the box
// on open (used by Rename). Must be called every frame while the popup
// might be open; only writes to outName on confirm.
bool modalNamePrompt(const char* id,
                     const char* title,
                     const char* initialName,
                     std::string* outName) {
    // Per-modal storage so typing doesn't clobber on other modals.
    static char buf_new[kNameBufCap]    = {};
    static char buf_rename[kNameBufCap] = {};
    char* buf = (std::strcmp(id, "##AffixLibNew") == 0) ? buf_new : buf_rename;

    // Center the modal on the viewport.
    ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(),
                            ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

    bool confirmed = false;
    if (ImGui::BeginPopupModal(id, nullptr,
                               ImGuiWindowFlags_AlwaysAutoResize |
                               ImGuiWindowFlags_NoSavedSettings)) {
        if (ImGui::IsWindowAppearing()) {
            // Prefill + focus on open.
            std::snprintf(buf, kNameBufCap, "%s", initialName ? initialName : "");
            ImGui::SetKeyboardFocusHere();
        }

        ImGui::TextUnformatted(title);
        ImGui::Spacing();
        ImGui::SetNextItemWidth(260.0f);
        const bool entered = ImGui::InputText(
            "##name", buf, kNameBufCap,
            ImGuiInputTextFlags_EnterReturnsTrue);

        const bool valid = poebot::config::isValidAffixLibraryName(buf);
        if (!valid && buf[0] != '\0') {
            ImGui::TextDisabled("%s", poebot::i18n::tr("affix_lib.invalid_name"));
        }

        ImGui::Spacing();
        ImGui::BeginDisabled(!valid);
        if (ImGui::Button(poebot::i18n::tr("common.ok"), ImVec2(90, 0)) || entered) {
            if (outName) *outName = buf;
            confirmed = true;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndDisabled();

        ImGui::SameLine();
        if (ImGui::Button(poebot::i18n::tr("common.cancel"), ImVec2(90, 0)) ||
            ImGui::IsKeyPressed(ImGuiKey_Escape)) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
    return confirmed;
}

// Yes/No confirm modal — used for Delete.
bool modalConfirm(const char* id, const char* message) {
    ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(),
                            ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    bool confirmed = false;
    if (ImGui::BeginPopupModal(id, nullptr,
                               ImGuiWindowFlags_AlwaysAutoResize |
                               ImGuiWindowFlags_NoSavedSettings)) {
        ImGui::TextUnformatted(message);
        ImGui::Spacing();
        if (ImGui::Button(poebot::i18n::tr("common.ok"), ImVec2(90, 0))) {
            confirmed = true;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button(poebot::i18n::tr("common.cancel"), ImVec2(90, 0)) ||
            ImGui::IsKeyPressed(ImGuiKey_Escape)) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
    return confirmed;
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

    // Refresh library list every frame — it's a directory scan of ~dozens
    // of tiny files, and doing it on-demand avoids every "did the user
    // rename a file outside the app?" edge case.
    const auto libs = dir.empty() ? std::vector<std::string>{}
                                  : al::listAffixLibraries(dir);
    const bool dirUsable = !dir.empty();

    // --- Row 1: label + library combo -----------------------------------
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(tr("affix_lib.label"));
    ImGui::SameLine();

    // "(modified)" suffix when current content diverges from the on-disk
    // library — tells the user a Save is pending without a separate icon.
    bool modified = false;
    if (!selected.empty() && dirUsable) {
        const std::string onDisk = al::loadAffixLibrary(dir, selected);
        modified = (onDisk != content);
    }

    ImGui::SetNextItemWidth(240.0f);
    const char* comboPreview = selected.empty()
        ? tr("affix_lib.none")
        : selected.c_str();
    if (ImGui::BeginCombo("##lib", comboPreview)) {
        // "(none)" clears selection without touching content.
        const bool noneSel = selected.empty();
        if (ImGui::Selectable(tr("affix_lib.none"), noneSel)) {
            if (!noneSel) { selected.clear(); changed = true; }
        }
        ImGui::Separator();
        for (const auto& n : libs) {
            const bool isSel = (selected == n);
            // Right-align line count for a quick sense of library size.
            char label[128];
            const std::string body = al::loadAffixLibrary(dir, n);
            int lines = body.empty() ? 0 : 1;
            for (char c : body) if (c == '\n') ++lines;
            std::snprintf(label, sizeof(label), "%s  (%d)", n.c_str(), lines);

            if (ImGui::Selectable(label, isSel)) {
                if (!isSel) {
                    selected = n;
                    content  = body;  // load selected library into textbox
                    changed  = true;
                }
            }
        }
        ImGui::EndCombo();
    }

    if (modified) {
        ImGui::SameLine();
        ImGui::TextDisabled("%s", tr("affix_lib.modified_suffix"));
    }

    // --- Row 2: action buttons ------------------------------------------
    ImGui::BeginDisabled(!dirUsable);

    if (ImGui::Button(tr("affix_lib.new"))) {
        openModal("##AffixLibNew");
    }
    ImGui::SameLine();

    ImGui::BeginDisabled(selected.empty());
    if (ImGui::Button(tr("affix_lib.save"))) {
        // Overwrite the currently-bound library with the textbox content.
        // Nothing to prompt — user explicitly asked, and they can recover
        // by re-picking from the combo if they made a mistake (the dropdown
        // still reflects on-disk state after save).
        if (al::saveAffixLibrary(dir, selected, content)) {
            spdlog::info("affix_lib: saved '{}' ({} bytes)", selected, content.size());
        }
    }
    ImGui::SameLine();
    if (ImGui::Button(tr("affix_lib.rename"))) {
        openModal("##AffixLibRename");
    }
    ImGui::SameLine();
    if (ImGui::Button(tr("affix_lib.delete"))) {
        openModal("##AffixLibDelete");
    }
    ImGui::EndDisabled();  // selected.empty()

    ImGui::EndDisabled();  // !dirUsable

    // --- Row 3: poe.re hint ---------------------------------------------
    ImGui::TextDisabled("%s", tr("affix_lib.poere_prefix"));
    ImGui::SameLine(0.0f, 4.0f);
    renderLink(kPoeReURL, kPoeReURL);

    // --- Modals ----------------------------------------------------------
    {
        std::string nameOut;
        if (modalNamePrompt("##AffixLibNew",
                            tr("affix_lib.prompt_new"),
                            "",
                            &nameOut)) {
            if (al::saveAffixLibrary(dir, nameOut, content)) {
                selected = nameOut;
                changed  = true;
                spdlog::info("affix_lib: created '{}'", nameOut);
            }
        }
    }
    {
        std::string nameOut;
        if (modalNamePrompt("##AffixLibRename",
                            tr("affix_lib.prompt_rename"),
                            selected.c_str(),
                            &nameOut)) {
            if (nameOut != selected &&
                al::renameAffixLibrary(dir, selected, nameOut)) {
                spdlog::info("affix_lib: renamed '{}' -> '{}'", selected, nameOut);
                selected = nameOut;
                changed  = true;
            }
        }
    }
    {
        char msg[256];
        std::snprintf(msg, sizeof(msg), tr("affix_lib.confirm_delete_fmt"),
                      selected.c_str());
        if (modalConfirm("##AffixLibDelete", msg)) {
            if (al::deleteAffixLibrary(dir, selected)) {
                spdlog::info("affix_lib: deleted '{}'", selected);
                selected.clear();
                changed = true;
            }
        }
    }

    ImGui::PopID();
    if (outChanged) *outChanged = changed;
}

}  // namespace poebot::gui
