#include <poebot/gui/panels/config_panel.hpp>

#include <poebot/config/profile.hpp>
#include <poebot/coords.hpp>
#include <poebot/gui/capture_service.hpp>
#include <poebot/i18n/i18n.hpp>

#include <imgui.h>
#include <spdlog/spdlog.h>

#include <cstdio>
#include <string_view>

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

}  // namespace

void ConfigPanel::requestRebind(const char* actionId) {
    rebindingId_ = actionId;
    rebindError_.clear();
    capture_.start();
}

// One coord row: field name, captured value, and a button labeled with
// the live binding for that coord's capture hotkey. Clicking the button
// rebinds — capture itself only fires when the user presses the bound
// hotkey in-game. The "armed" green tint lights up while a capture is
// pending (3-second window after the hotkey press), giving visible
// feedback that the hotkey was received.
void ConfigPanel::coordRow(PanelContext& ctx, const char* name,
                           poebot::ClientPoint& p) {
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

    // Compose "capture.<name>" once — used both for the displayed
    // binding label and for the rebind target if the user clicks.
    char actionId[32];
    std::snprintf(actionId, sizeof(actionId), "capture.%s", name);
    const std::string label = bindingLabel(ctx, actionId);

    ImGui::SameLine(250.0f);
    if (ImGui::SmallButton(label.c_str())) {
        requestRebind(actionId);
    }
    ImGui::PopID();
}

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
        // Only the global / non-capture actions live here — the nine
        // capture.* rows used to be duplicated on this tab AND inline
        // with each coord row in tab 2. Now they're shown only with
        // their coord row, removing the redundancy. Tab 1 keeps the
        // five actions that have no associated coord (start craft /
        // map / deposit, stop, overlay).
        if (ImGui::BeginTabItem(tr("settings.tab.hotkeys"))) {
            ImGui::Spacing();

            for (const auto& a : poebot::hotkey::allHotkeyActions()) {
                const std::string_view id{a.id};
                if (id.rfind("capture.", 0) == 0) continue;  // shown in tab 2

                ImGui::PushID(a.id);
                ImGui::AlignTextToFramePadding();
                ImGui::Text("%s", tr(a.labelKey));
                ImGui::SameLine(220.0f);
                const std::string label = bindingLabel(ctx, a.id);
                if (ImGui::SmallButton(label.c_str())) {
                    requestRebind(a.id);
                }
                ImGui::PopID();
            }

            ImGui::Spacing();
            // Reset only the actions this tab shows — the nine capture.*
            // hotkeys are owned by the Coordinates tab now, so resetting
            // them here would silently churn buttons the user can't even
            // see. The same capture.* filter as the row loop above.
            if (ImGui::SmallButton(tr("settings.hotkeys.reset_all"))) {
                if (ctx.onRebindHotkey) {
                    for (const auto& a : poebot::hotkey::allHotkeyActions()) {
                        if (std::string_view(a.id).rfind("capture.", 0) == 0) continue;
                        ctx.onRebindHotkey(a.id, a.defaultBinding);
                    }
                    spdlog::info("hotkeys: reset task/overlay hotkeys to defaults");
                }
            }

            ImGui::EndTabItem();
        }

        // ----- Tab 2: Coordinates ---------------------------------------
        // Each coord row's button now opens the rebind modal for that
        // coord's capture hotkey (capture.orb1, capture.baseItem, …).
        // The actual capture flow only fires when the user presses the
        // bound hotkey in-game — there's no manual-trigger button now.
        if (ImGui::BeginTabItem(tr("settings.tab.coords"))) {
            ImGui::Spacing();

            // Coord rows don't need to accumulate a dirty flag —
            // CaptureService marks panelCtx_.dirty when a capture commits.
            auto& c = prof->coords;

            ImGui::TextUnformatted(tr("config.section.orbs"));
            coordRow(ctx, "orb1",     c.orb1);
            coordRow(ctx, "orb2",     c.orb2);
            coordRow(ctx, "orb3",     c.orb3);

            ImGui::Spacing();
            ImGui::TextUnformatted(tr("config.section.anchors"));
            coordRow(ctx, "baseItem", c.baseItem);
            coordRow(ctx, "p01Item",  c.p01Item);
            coordRow(ctx, "p10Item",  c.p10Item);

            ImGui::Spacing();
            ImGui::TextUnformatted(tr("config.section.inventory"));
            coordRow(ctx, "invBase",  c.invBase);
            coordRow(ctx, "invP01",   c.invP01);
            coordRow(ctx, "invP10",   c.invP10);

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
                // Success path covers both "no conflict" and the swap-on-
                // conflict case (App handles the swap internally).
                rebindingId_.clear();
                rebindError_.clear();
                ImGui::CloseCurrentPopup();
            } else {
                // Only failure path left: another *application* owns the
                // combo (RegisterHotKey returned 0). Show the inline error
                // and rearm capture so the user can try a different combo
                // without re-clicking the Rebind button.
                char buf[256];
                std::snprintf(buf, sizeof(buf),
                              tr("settings.hotkeys.rebind_err_unavailable_fmt"),
                              newBinding.format().c_str());
                rebindError_ = buf;
                capture_.start();
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
            // The capture.* hotkey buttons live on this tab too — restore
            // them to defaults along with the coord rows so "reset" really
            // means "everything on this view, gone". App.rebindHotkey early-
            // exits no-ops, so combos already at default cost nothing.
            // (Hotkeys are global in app.json, so this also resets the
            // bindings used by the *other* profile — acceptable since both
            // profiles share the same physical keyboard anyway.)
            if (ctx.onRebindHotkey) {
                for (const auto& a : poebot::hotkey::allHotkeyActions()) {
                    if (std::string_view(a.id).rfind("capture.", 0) == 0) {
                        ctx.onRebindHotkey(a.id, a.defaultBinding);
                    }
                }
            }
            ctx.dirty = true;
            spdlog::info("config: profile '{}' reset to defaults (incl. capture hotkeys)",
                         prof->name);
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
