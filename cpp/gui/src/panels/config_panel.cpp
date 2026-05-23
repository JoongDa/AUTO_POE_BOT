#include <poebot/gui/panels/config_panel.hpp>

#include <poebot/config/profile.hpp>
#include <poebot/coords.hpp>
#include <poebot/gui/capture_service.hpp>
#include <poebot/i18n/i18n.hpp>
#include <poebot/sys/screen_capture.hpp>
#include <poebot/vision/template_match.hpp>

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
                           poebot::ClientPoint& p,
                           const int* qty) {
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
        if (qty) {
            ImGui::SameLine(200.0f);
            if (*qty >= 0) ImGui::TextDisabled("x%d", *qty);
            else           ImGui::TextDisabled("x?");
        }
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
            coordRow(ctx, "orb1",     c.orb1, &c.orb1Qty);
            coordRow(ctx, "orb2",     c.orb2, &c.orb2Qty);
            coordRow(ctx, "orb3",     c.orb3, &c.orb3Qty);

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

        // Trailing tab — right-aligned in the tab bar. Contains the OpenCV
        // template-matching auto-calibration workflow.
        if (ImGui::BeginTabItem(tr("settings.tab.auto_calibrate"),
                                nullptr,
                                ImGuiTabItemFlags_Trailing)) {
            renderAutoCalibrate(ctx);
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

// ============================================================
// Auto-calibrate tab
// ============================================================

void ConfigPanel::renderAutoCalibrate(PanelContext& ctx) {
    using poebot::i18n::tr;

    // Lazy-load templates the first time this tab is rendered.
    if (!templatesLoaded_ && ctx.settingsRoot) {
        templateLib_.load(*ctx.settingsRoot / "templates");
        templatesLoaded_ = true;
    }

    // Directory path + reload button on the same line.
    if (ctx.settingsRoot) {
        const auto dir = *ctx.settingsRoot / "templates";
        ImGui::TextDisabled("%s", dir.string().c_str());
    } else {
        ImGui::TextDisabled("templates/");
    }
    ImGui::SameLine();
    if (ImGui::SmallButton(tr("auto_cal.load"))) {
        if (ctx.settingsRoot) {
            templateLib_.load(*ctx.settingsRoot / "templates");
            calibStatus_.clear();
        }
    }
    ImGui::Spacing();

    if (!templateLib_.anyLoaded()) {
        ImGui::TextDisabled("%s", tr("auto_cal.no_templates"));
        ImGui::Spacing();
    }

    // Per-template rows: name | current coord | Manual button
    auto* prof = ctx.settings ? ctx.settings->active() : nullptr;
    for (const auto& entry : templateLib_.entries()) {
        ImGui::PushID(entry.coordName.c_str());
        ImGui::AlignTextToFramePadding();

        // Column 1: coord name
        ImGui::Text("%-10s", entry.coordName.c_str());
        ImGui::SameLine(110.0f);

        // Column 2: current value
        if (!entry.loaded) {
            ImGui::TextDisabled("%s", tr("auto_cal.template_missing"));
        } else if (prof) {
            const auto* coord = poebot::config::findCoordByName(*prof, entry.coordName);
            if (!coord || poebot::isUnset(*coord)) {
                ImGui::TextDisabled("%s", tr("config.unset"));
            } else {
                ImGui::Text("(%d, %d)", coord->x, coord->y);
            }
        } else {
            ImGui::TextDisabled("%s", tr("config.unset"));
        }

        // Column 3: Manual capture button (3-second countdown via CaptureService)
        ImGui::SameLine(220.0f);
        if (ImGui::SmallButton(tr("auto_cal.manual"))) {
            if (ctx.capture) {
                ctx.capture->startCapture(entry.coordName);
            }
        }

        ImGui::PopID();
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // "Start Auto Calibrate" — disabled when the game window is absent or
    // no templates are loaded (nothing to match against).
    const bool canStart =
        templateLib_.anyLoaded() &&
        ctx.gameWindow && ctx.gameWindow->valid();

    if (!canStart) ImGui::BeginDisabled();
    if (ImGui::Button(tr("auto_cal.start"))) {
        runAutoCalibrate(ctx);
    }
    if (!canStart) ImGui::EndDisabled();

    if (!calibStatus_.empty()) {
        ImGui::SameLine();
        ImGui::TextDisabled("%s", calibStatus_.c_str());
    }
}

void ConfigPanel::runAutoCalibrate(PanelContext& ctx) {
    using poebot::i18n::tr;

    calibStatus_.clear();

    // Pre-conditions
    if (!ctx.gameWindow || !ctx.gameWindow->valid()) {
        calibStatus_ = tr("auto_cal.err_no_game");
        return;
    }
    auto* prof = ctx.settings ? ctx.settings->active() : nullptr;
    if (!prof) return;

    // 1. Capture the game client area via BitBlt.
    const HWND hwnd = ctx.gameWindow->hwnd();
    auto capOpt = poebot::sys::captureClient(hwnd);
    if (!capOpt) {
        calibStatus_ = tr("auto_cal.err_capture");
        return;
    }
    const int captureW = capOpt->width;
    const int captureH = capOpt->height;

    // 2. Wrap as ImageBGRA (zero-copy move of the pixel buffer).
    auto haystack = poebot::vision::ImageBGRA::fromCaptured(std::move(*capOpt));

    // 3. Normalize to 1920×1080. Templates are authored at this resolution so
    //    the same set works for 1080p, 1440p, and 4K game windows.
    constexpr int kTargetW = 1920;
    constexpr int kTargetH = 1080;
    if (haystack.width != kTargetW || haystack.height != kTargetH) {
        haystack = poebot::vision::resize(haystack, kTargetW, kTargetH);
    }
    if (haystack.pixels.empty()) {
        calibStatus_ = tr("auto_cal.err_capture");
        return;
    }

    // 4. Match each loaded template; threshold at 0.70 (TM_CCOEFF_NORMED).
    constexpr float kMinScore = 0.70f;
    int updated = 0;

    for (const auto& entry : templateLib_.entries()) {
        if (!entry.loaded) continue;

        const auto res = poebot::vision::match(haystack, entry.image);
        if (!res || res->score < kMinScore) {
            spdlog::debug("auto_cal: '{}' no match (score={:.2f})",
                          entry.coordName, res ? res->score : 0.0f);
            continue;
        }

        // Convert match top-left to template center in 1080p space, then
        // back-project to actual client coordinates.
        const float cx1080 = static_cast<float>(res->x) + entry.image.width  * 0.5f;
        const float cy1080 = static_cast<float>(res->y) + entry.image.height * 0.5f;
        const int   clientX = static_cast<int>(cx1080 * captureW / kTargetW);
        const int   clientY = static_cast<int>(cy1080 * captureH / kTargetH);

        auto* coord = poebot::config::findCoordByName(*prof, entry.coordName);
        if (coord) {
            coord->x = clientX;
            coord->y = clientY;
            ++updated;
            spdlog::info("auto_cal: '{}' → ({}, {})  score={:.2f}",
                         entry.coordName, clientX, clientY, res->score);
        }
    }

    // 5. Persist and report.
    if (updated > 0) {
        ctx.dirty = true;
        char buf[128];
        std::snprintf(buf, sizeof(buf), tr("auto_cal.done_fmt"), updated);
        calibStatus_ = buf;
    } else {
        calibStatus_ = tr("auto_cal.no_match");
    }
}

}  // namespace poebot::gui::panels
