#include <poebot/gui/panels/config_panel.hpp>

#include <poebot/config/profile.hpp>
#include <poebot/coords.hpp>
#include <poebot/gui/capture_service.hpp>
#include <poebot/i18n/i18n.hpp>

#include <imgui.h>
#include <spdlog/spdlog.h>

#include <cstdio>

namespace poebot::gui::panels {

namespace {

// Live preview of the modifier stack as the user holds keys down — looks
// like "Ctrl+Shift+..." so the trailing "..." cues "waiting for the key".
// Falls back to the localized prompt placeholder when no modifiers are held.
std::string formatModifierPreview(UINT mods, const char* placeholder) {
    if (mods == 0) return placeholder;
    std::string s;
    if (mods & MOD_CONTROL) s += "Ctrl+";
    if (mods & MOD_SHIFT)   s += "Shift+";
    if (mods & MOD_ALT)     s += "Alt+";
    if (mods & MOD_WIN)     s += "Win+";
    s += "...";
    return s;
}

// Resolve the binding currently bound to `actionId`, falling back to the
// action's default if the live map doesn't have it. Returns "(unbound)" /
// "Alt+1" / etc.
std::string bindingLabel(const PanelContext& ctx, const char* actionId) {
    if (ctx.hotkeys) {
        if (auto it = ctx.hotkeys->find(actionId); it != ctx.hotkeys->end()) {
            return it->second.format();
        }
    }
    return poebot::hotkey::defaultBindingFor(actionId).format();
}

// One coord row: field name, value, hotkey-trigger button. Click the button
// to fire the capture flow without leaving the keyboard. The button label
// reflects whatever the user has bound to capture.<name>, so it stays in
// sync after a rebind.
//
// `name` is the stable coord field id (English, "orb1" / "baseItem" / …).
void coordRow(const char* name, poebot::ClientPoint& p, PanelContext& ctx) {
    using poebot::i18n::tr;
    ImGui::PushID(name);
    ImGui::AlignTextToFramePadding();

    const bool armed = ctx.capture && ctx.capture->active() &&
                       ctx.capture->activeName() == name;
    if (armed) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.2f, 1.0f, 0.4f, 1.0f));
    }

    ImGui::Text("%-10s", name);
    ImGui::SameLine(110.0f);
    if (poebot::isUnset(p)) {
        ImGui::TextDisabled("%s", tr("config.unset"));
    } else {
        ImGui::Text("(%d, %d)", p.x, p.y);
    }
    if (armed) ImGui::PopStyleColor();

    char actionId[32];
    std::snprintf(actionId, sizeof(actionId), "capture.%s", name);
    const std::string label = bindingLabel(ctx, actionId);

    ImGui::SameLine(250.0f);
    if (ImGui::SmallButton(label.c_str()) && ctx.capture) {
        ctx.capture->startCapture(name);
    }
    ImGui::PopID();
}

}  // namespace

void ConfigPanel::render(PanelContext& ctx) {
    using poebot::i18n::tr;
    if (!ctx.settings) {
        ImGui::TextUnformatted(tr("config.no_settings"));
        return;
    }
    auto* prof = ctx.settings->active();
    if (!prof) {
        ImGui::Text(tr("config.no_active_profile_fmt"),
                    ctx.settings->activeProfile.c_str());
        return;
    }

    // ===== Header — visible above both tabs =================================
    // Hotkeys are global, but the active-profile line still useful here
    // because the Coordinates tab edits per-profile data; pinning the label
    // up top means the user sees which game they're configuring without
    // having to switch tabs first.
    ImGui::Text(tr("config.active_profile_fmt"),
                prof->displayName.c_str(), prof->name.c_str());
    ImGui::Spacing();

    // ===== Tab bar ==========================================================
    // Modal triggers from inside a TabItem land on a different ImGui id
    // stack than the BeginPopupModal call below. Tabs flag what the user
    // wants; the actual OpenPopup happens at panel scope (after EndTabBar)
    // so the popup id matches BeginPopupModal regardless of which tab
    // sourced the request.
    bool requestResetConfirm = false;

    if (ImGui::BeginTabBar("##SettingsTabs", ImGuiTabBarFlags_None)) {
        // ----- Tab 1: Hotkeys -------------------------------------------
        if (ImGui::BeginTabItem(tr("settings.tab.hotkeys"))) {
            ImGui::Spacing();

            for (const auto& a : poebot::hotkey::allHotkeyActions()) {
                ImGui::PushID(a.id);
                ImGui::AlignTextToFramePadding();
                ImGui::Text("%s", tr(a.labelKey));
                ImGui::SameLine(220.0f);
                const std::string label = bindingLabel(ctx, a.id);
                if (ImGui::SmallButton(label.c_str())) {
                    // Set state; the actual OpenPopup runs at panel scope.
                    rebindingId_ = a.id;
                    rebindError_.clear();
                    capture_.start();
                }
                ImGui::PopID();
            }

            ImGui::Spacing();
            if (ImGui::SmallButton(tr("settings.hotkeys.reset_all"))) {
                if (ctx.onRebindHotkey) {
                    for (const auto& a : poebot::hotkey::allHotkeyActions()) {
                        ctx.onRebindHotkey(a.id, a.defaultBinding);
                    }
                    spdlog::info("hotkeys: reset all to defaults");
                }
            }

            ImGui::EndTabItem();
        }

        // ----- Tab 2: Coordinates ---------------------------------------
        if (ImGui::BeginTabItem(tr("settings.tab.coords"))) {
            ImGui::Spacing();

            // Coord rows don't need to accumulate a dirty flag —
            // CaptureService marks panelCtx_.dirty when a capture commits.
            auto& c = prof->coords;

            ImGui::TextUnformatted(tr("config.section.orbs"));
            coordRow("orb1",     c.orb1, ctx);
            coordRow("orb2",     c.orb2, ctx);
            coordRow("orb3",     c.orb3, ctx);

            ImGui::Spacing();
            ImGui::TextUnformatted(tr("config.section.anchors"));
            coordRow("baseItem", c.baseItem, ctx);
            coordRow("p01Item",  c.p01Item,  ctx);
            coordRow("p10Item",  c.p10Item,  ctx);

            ImGui::Spacing();
            ImGui::TextUnformatted(tr("config.section.inventory"));
            coordRow("invBase",  c.invBase, ctx);
            coordRow("invP01",   c.invP01,  ctx);
            coordRow("invP10",   c.invP10,  ctx);

            // Reset profile — wipes coords + craft/map/deposit/stats. The
            // confirm modal stops misclicks; auto-save means we don't ship
            // an explicit Save button anywhere on this page.
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();
            if (ImGui::Button(tr("config.button.reset_profile"))) {
                requestResetConfirm = true;
            }

            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }

    // ===== Modals (panel scope) =============================================
    // Both popups live here so they survive tab switches and so OpenPopup
    // stacks the same id BeginPopupModal expects.

    // Rebind modal — driven by rebindingId_ (set inside the Hotkeys tab).
    if (!rebindingId_.empty() && !ImGui::IsPopupOpen("##RebindModal")) {
        ImGui::OpenPopup("##RebindModal");
    }
    ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(),
                            ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    if (ImGui::BeginPopupModal("##RebindModal", nullptr,
                               ImGuiWindowFlags_AlwaysAutoResize |
                               ImGuiWindowFlags_NoSavedSettings)) {
        const char* labelKey = "";
        for (const auto& a : poebot::hotkey::allHotkeyActions()) {
            if (rebindingId_ == a.id) { labelKey = a.labelKey; break; }
        }
        ImGui::Text(tr("settings.hotkeys.rebind_title_fmt"), tr(labelKey));
        ImGui::Spacing();

        const UINT mods = capture_.previewModifiers();
        const std::string preview = formatModifierPreview(
            mods, tr("settings.hotkeys.rebind_prompt"));
        ImGui::PushStyleColor(ImGuiCol_Text,
                              ImVec4(0.20f, 0.55f, 0.98f, 1.0f));
        ImGui::Text("  %s", preview.c_str());
        ImGui::PopStyleColor();

        ImGui::Spacing();
        ImGui::TextDisabled("%s", tr("settings.hotkeys.rebind_hint"));

        if (!rebindError_.empty()) {
            ImGui::Spacing();
            ImGui::PushStyleColor(ImGuiCol_Text,
                                  ImVec4(1.00f, 0.42f, 0.42f, 1.0f));
            ImGui::TextWrapped("%s", rebindError_.c_str());
            ImGui::PopStyleColor();
        }

        ImGui::Spacing();
        if (ImGui::Button(tr("common.cancel"), ImVec2(90, 0))) {
            capture_.stop();
            rebindingId_.clear();
            rebindError_.clear();
            ImGui::CloseCurrentPopup();
        }

        if (capture_.canceled()) {
            capture_.stop();
            rebindingId_.clear();
            rebindError_.clear();
            ImGui::CloseCurrentPopup();
        } else if (capture_.committed()) {
            const auto newBinding = capture_.result();
            capture_.stop();
            bool ok = false;
            if (ctx.onRebindHotkey) {
                ok = ctx.onRebindHotkey(rebindingId_, newBinding);
            }
            if (ok) {
                rebindingId_.clear();
                rebindError_.clear();
                ImGui::CloseCurrentPopup();
            } else {
                std::string conflictId;
                if (ctx.hotkeys) {
                    for (const auto& [otherId, otherBinding] : *ctx.hotkeys) {
                        if (otherId == rebindingId_) continue;
                        if (otherBinding == newBinding) {
                            conflictId = otherId;
                            break;
                        }
                    }
                }
                if (!conflictId.empty()) {
                    const char* otherLabelKey = conflictId.c_str();
                    for (const auto& a : poebot::hotkey::allHotkeyActions()) {
                        if (conflictId == a.id) { otherLabelKey = a.labelKey; break; }
                    }
                    char buf[256];
                    std::snprintf(buf, sizeof(buf),
                                  tr("settings.hotkeys.rebind_err_conflict_fmt"),
                                  newBinding.format().c_str(),
                                  tr(otherLabelKey));
                    rebindError_ = buf;
                } else {
                    char buf[256];
                    std::snprintf(buf, sizeof(buf),
                                  tr("settings.hotkeys.rebind_err_unavailable_fmt"),
                                  newBinding.format().c_str());
                    rebindError_ = buf;
                }
                capture_.start();  // let user retry without re-clicking
            }
        }

        ImGui::EndPopup();
    } else if (!rebindingId_.empty()) {
        // Modal dismissed by something other than our own paths (e.g. the
        // window losing focus closes popups). Tear down so the hook doesn't
        // keep intercepting input.
        capture_.stop();
        rebindingId_.clear();
        rebindError_.clear();
    }

    // Reset-profile confirm — fires when the Coords tab requested it this frame.
    if (requestResetConfirm) {
        ImGui::OpenPopup("##ResetConfirm");
    }
    ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(),
                            ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    if (ImGui::BeginPopupModal("##ResetConfirm", nullptr,
                               ImGuiWindowFlags_AlwaysAutoResize |
                               ImGuiWindowFlags_NoSavedSettings)) {
        ImGui::TextUnformatted(tr("config.confirm_reset"));
        ImGui::Spacing();
        if (ImGui::Button(tr("common.ok"), ImVec2(90, 0))) {
            if (ctx.capture) ctx.capture->cancel();
            auto def = poebot::config::defaultProfileFor(prof->name);
            prof->coords  = def.coords;
            prof->craft   = def.craft;
            prof->map     = def.map;
            prof->deposit = def.deposit;
            prof->stats   = def.stats;
            ctx.dirty = true;
            spdlog::info("config: profile '{}' reset to defaults", prof->name);
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button(tr("common.cancel"), ImVec2(90, 0)) ||
            ImGui::IsKeyPressed(ImGuiKey_Escape)) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

}  // namespace poebot::gui::panels
