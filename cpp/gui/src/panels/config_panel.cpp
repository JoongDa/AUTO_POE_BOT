#include <poebot/gui/panels/config_panel.hpp>

#include <poebot/config/profile.hpp>
#include <poebot/coords.hpp>
#include <poebot/gui/capture_service.hpp>
#include <poebot/i18n/i18n.hpp>
#include <poebot/sys/screen_capture.hpp>
#include <poebot/vision/template_library.hpp>
#include <poebot/vision/template_match.hpp>

#include <imgui.h>
#include <spdlog/spdlog.h>

#include <d3d11.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace poebot::gui::panels {

namespace {

// Create an immutable D3D11 shader-resource-view from raw BGRA pixels.
// The texture is DXGI_FORMAT_B8G8R8A8_UNORM, matching our CapturedImage
// layout. Returns nullptr on any D3D failure; caller owns the Release().
static ID3D11ShaderResourceView* createBGRASRV(ID3D11Device* dev,
                                               const void*   bgra,
                                               int w, int h, int stride) {
    if (!dev || !bgra || w <= 0 || h <= 0) return nullptr;

    D3D11_TEXTURE2D_DESC td{};
    td.Width              = static_cast<UINT>(w);
    td.Height             = static_cast<UINT>(h);
    td.MipLevels          = 1;
    td.ArraySize          = 1;
    td.Format             = DXGI_FORMAT_B8G8R8A8_UNORM;
    td.SampleDesc.Count   = 1;
    td.Usage              = D3D11_USAGE_IMMUTABLE;
    td.BindFlags          = D3D11_BIND_SHADER_RESOURCE;

    D3D11_SUBRESOURCE_DATA sr{};
    sr.pSysMem            = bgra;
    sr.SysMemPitch        = static_cast<UINT>(stride);

    ID3D11Texture2D* tex = nullptr;
    if (FAILED(dev->CreateTexture2D(&td, &sr, &tex))) return nullptr;

    D3D11_SHADER_RESOURCE_VIEW_DESC svd{};
    svd.Format                    = DXGI_FORMAT_B8G8R8A8_UNORM;
    svd.ViewDimension             = D3D11_SRV_DIMENSION_TEXTURE2D;
    svd.Texture2D.MipLevels       = 1;

    ID3D11ShaderResourceView* srv = nullptr;
    const HRESULT hr = dev->CreateShaderResourceView(tex, &svd, &srv);
    tex->Release();
    return SUCCEEDED(hr) ? srv : nullptr;
}

// Return the (pos, size) of the monitor that contains the centre of the main
// app window. Handles any resolution (1080p / 2K / 4K) and multi-monitor
// setups. Falls back to the primary monitor, then to the app window itself.
static std::pair<ImVec2, ImVec2> appMonitorBounds() {
    const ImGuiPlatformIO& pio = ImGui::GetPlatformIO();
    const ImGuiViewport*   vp  = ImGui::GetMainViewport();

    // Use the app window centre to pick the right monitor.
    const ImVec2 centre(vp->Pos.x + vp->Size.x * 0.5f,
                        vp->Pos.y + vp->Size.y * 0.5f);
    for (int i = 0; i < pio.Monitors.Size; ++i) {
        const ImGuiPlatformMonitor& m = pio.Monitors[i];
        if (centre.x >= m.MainPos.x && centre.x < m.MainPos.x + m.MainSize.x &&
            centre.y >= m.MainPos.y && centre.y < m.MainPos.y + m.MainSize.y) {
            return {m.MainPos, m.MainSize};
        }
    }
    // Fall back to the primary monitor entry, or the app window as last resort.
    if (pio.Monitors.Size > 0)
        return {pio.Monitors[0].MainPos, pio.Monitors[0].MainSize};
    return {vp->Pos, vp->Size};
}

// Estimate the background colour from 2×2 corner clusters, then set A=0 for
// pixels within `threshold` RGB units of that colour. Used only for display
// thumbnails — matching always runs on the original unmodified template image.
static void removeBackground(poebot::vision::ImageBGRA& img,
                             float threshold = 35.0f) {
    if (img.width < 4 || img.height < 4 || img.pixels.empty()) return;

    float sumB = 0, sumG = 0, sumR = 0;
    int   n    = 0;
    const int W = img.width, H = img.height;

    const auto px = [&](int x, int y) -> const uint8_t* {
        return img.pixels.data() +
               static_cast<std::size_t>(y) * img.stride +
               static_cast<std::size_t>(x) * 4;
    };
    for (int dy = 0; dy < 2; ++dy) {
        for (int dx = 0; dx < 2; ++dx) {
            const auto add = [&](int x, int y) {
                const uint8_t* p = px(x, y);
                sumB += p[0]; sumG += p[1]; sumR += p[2]; ++n;
            };
            add(dx,     dy    );
            add(W-1-dx, dy    );
            add(dx,     H-1-dy);
            add(W-1-dx, H-1-dy);
        }
    }

    const float bgB = sumB / n, bgG = sumG / n, bgR = sumR / n;
    const float tSq = threshold * threshold;

    for (int y = 0; y < H; ++y) {
        uint8_t* row = img.pixels.data() +
                       static_cast<std::size_t>(y) * img.stride;
        for (int x = 0; x < W; ++x) {
            uint8_t* p     = row + static_cast<std::size_t>(x) * 4;
            const float db = p[0] - bgB;
            const float dg = p[1] - bgG;
            const float dr = p[2] - bgR;
            p[3] = (db*db + dg*dg + dr*dr < tSq) ? 0u : 255u;
        }
    }
}

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
    ImGui::TableNextRow();
    ImGui::PushID(name);

    const bool armed = ctx.capture && ctx.capture->active() &&
                       ctx.capture->activeName() == name;
    if (armed)
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.2f, 1.0f, 0.4f, 1.0f));

    // Col 0: coord name
    ImGui::TableNextColumn();
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(name);

    // Col 1: captured value + optional qty
    ImGui::TableNextColumn();
    if (poebot::isUnset(p)) {
        ImGui::TextDisabled("%s", tr("config.unset"));
    } else {
        ImGui::Text("(%d, %d)", p.x, p.y);
        if (qty) {
            ImGui::SameLine();
            if (*qty >= 0) ImGui::TextDisabled("x%d", *qty);
            else           ImGui::TextDisabled("x?");
        }
    }

    if (armed) ImGui::PopStyleColor();

    // Col 2: rebind button labeled with the live binding
    ImGui::TableNextColumn();
    char actionId[32];
    std::snprintf(actionId, sizeof(actionId), "capture.%s", name);
    const std::string label = bindingLabel(ctx, actionId);
    if (ImGui::SmallButton(label.c_str()))
        requestRebind(actionId);

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

            ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(4.0f, 1.0f));
            if (ImGui::BeginTable("##hotkeys", 2,
                                  ImGuiTableFlags_NoSavedSettings |
                                  ImGuiTableFlags_SizingFixedFit)) {
                ImGui::TableSetupColumn("##hlabel", ImGuiTableColumnFlags_WidthFixed,   0.0f);
                ImGui::TableSetupColumn("##hbtn",   ImGuiTableColumnFlags_WidthStretch);

                for (const auto& a : poebot::hotkey::allHotkeyActions()) {
                    if (std::string_view(a.id).rfind("capture.", 0) == 0) continue;
                    ImGui::TableNextRow();
                    ImGui::PushID(a.id);
                    ImGui::TableNextColumn();
                    ImGui::AlignTextToFramePadding();
                    ImGui::TextUnformatted(tr(a.labelKey));
                    ImGui::TableNextColumn();
                    const std::string label = bindingLabel(ctx, a.id);
                    if (ImGui::SmallButton(label.c_str()))
                        requestRebind(a.id);
                    ImGui::PopID();
                }

                ImGui::EndTable();
            }
            ImGui::PopStyleVar();

            ImGui::Spacing();
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
            auto& c = prof->coords;

            ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(4.0f, 1.0f));
            if (ImGui::BeginTable("##coords", 3,
                                  ImGuiTableFlags_NoSavedSettings |
                                  ImGuiTableFlags_SizingFixedFit)) {
                ImGui::TableSetupColumn("##cname", ImGuiTableColumnFlags_WidthFixed,   0.0f);
                ImGui::TableSetupColumn("##cval",  ImGuiTableColumnFlags_WidthFixed,   0.0f);
                ImGui::TableSetupColumn("##cbtn",  ImGuiTableColumnFlags_WidthStretch);

                // Section header row: text in col 0, other cols left empty.
                const auto sectionRow = [](const char* label) {
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::Spacing();
                    ImGui::TextUnformatted(label);
                    ImGui::TableNextColumn();
                    ImGui::TableNextColumn();
                };

                sectionRow(tr("config.section.orbs"));
                coordRow(ctx, "orb1",     c.orb1, &c.orb1Qty);
                coordRow(ctx, "orb2",     c.orb2, &c.orb2Qty);
                coordRow(ctx, "orb3",     c.orb3, &c.orb3Qty);

                sectionRow(tr("config.section.anchors"));
                coordRow(ctx, "baseItem", c.baseItem);
                coordRow(ctx, "p01Item",  c.p01Item);
                coordRow(ctx, "p10Item",  c.p10Item);

                sectionRow(tr("config.section.inventory"));
                coordRow(ctx, "invBase",  c.invBase);
                coordRow(ctx, "invP01",   c.invP01);
                coordRow(ctx, "invP10",   c.invP10);

                ImGui::EndTable();
            }
            ImGui::PopStyleVar();

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();
            if (ImGui::Button(tr("config.button.reset_profile")))
                requestResetConfirm = true;

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

    // Template crop modal — driven by cropState_.active (set inside the Auto
    // Calibrate tab when the user clicks a template name button).
    renderCropModal(ctx);

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
//
// Renders the read-only view of the template pool and the per-profile
// `calibrated` coord pool. Each template = one entry in the pool when
// matched 0 or 1 times; multi-match templates expand to N rows whose
// keys are "<basename>_1" … "<basename>_N".
//
// The actual matching is in runAutoCalibrate(). Wider design notes are
// in the conversation history; the short version:
//   - templates live in <exe>/templates/<name>.png and are loaded once
//     at app startup (App owns the library, this panel borrows it)
//   - results are written into prof->calibrated and persisted to JSON
//   - the existing ProfileCoords (orb1, baseItem, …) are unrelated and
//     remain owned by the Coordinates tab
// ============================================================

namespace {

// Match threshold and instance cap shared between renderAutoCalibrate
// and runAutoCalibrate. Tuned by feel; expose to UI later if needed.
constexpr float kMinMatchScore = 0.95f;
constexpr int   kMaxInstances  = 10;
constexpr int   kTargetW       = 1920;
constexpr int   kTargetH       = 1080;

// Decide whether a calibrated entry key belongs to the named template.
// Single-match entries are stored under the bare basename; multi-match
// entries get "_N" suffixes ("chaos_1", "chaos_2", …). Both are
// considered part of the "chaos" group.
//
// Be strict about the suffix shape ("_<digits>") so that a template
// named "orb" doesn't accidentally claim entries from "orb_currency_1".
bool keyBelongsTo(const std::string& key, const std::string& templateName) {
    if (key == templateName) return true;
    if (key.size() <= templateName.size() + 1) return false;
    if (key.compare(0, templateName.size(), templateName) != 0) return false;
    if (key[templateName.size()] != '_') return false;
    for (std::size_t i = templateName.size() + 1; i < key.size(); ++i) {
        if (!std::isdigit(static_cast<unsigned char>(key[i]))) return false;
    }
    return true;
}

// Pull every calibrated entry that belongs to a template, in key order
// so the UI shows _1 before _2 before _3.
std::vector<const std::pair<const std::string, poebot::config::CalibratedCoord>*>
groupedEntries(const poebot::config::GameProfile& prof, const std::string& templateName) {
    std::vector<const std::pair<const std::string, poebot::config::CalibratedCoord>*> out;
    for (const auto& kv : prof.calibrated) {
        if (keyBelongsTo(kv.first, templateName)) out.push_back(&kv);
    }
    std::sort(out.begin(), out.end(),
              [](auto a, auto b) { return a->first < b->first; });
    return out;
}

}  // namespace

void ConfigPanel::renderAutoCalibrate(PanelContext& ctx) {
    using poebot::i18n::tr;

    const auto* lib = ctx.templates;

    // Rebuild thumbnail SRV cache whenever the library pointer changes
    // (profile switch or Reload).
    if (lib != cachedTemplLib_) rebuildTemplThumbs(ctx);

    // Header: templates directory path only (Reload button moved to the
    // action row at the bottom alongside Start Calibrate).
    if (lib && !lib->dir().empty()) {
        ImGui::TextDisabled("%s", lib->dir().string().c_str());
    } else if (ctx.settingsRoot) {
        ImGui::TextDisabled("%s", (*ctx.settingsRoot / "templates").string().c_str());
    } else {
        ImGui::TextDisabled("templates/");
    }
    ImGui::Spacing();

    if (!lib || !lib->anyLoaded()) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.55f, 0.35f, 1.0f));
        ImGui::TextWrapped("%s", tr("auto_cal.no_templates"));
        ImGui::PopStyleColor();
        ImGui::Spacing();
    }

    auto* prof = ctx.settings ? ctx.settings->active() : nullptr;

    if (lib && prof) {
        const float iconSz = ImGui::GetTextLineHeight();

        // 4-column table: [icon | name/key | coord | qty+score]
        // CellPadding y=1 keeps rows tight; SizingFixedFit prevents columns
        // from stretching beyond their content (except the last one).
        ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(4.0f, 1.0f));
        if (ImGui::BeginTable("##templ_list", 4,
                              ImGuiTableFlags_NoSavedSettings |
                              ImGuiTableFlags_SizingFixedFit)) {
            ImGui::TableSetupColumn("##icon",  ImGuiTableColumnFlags_WidthFixed,  iconSz + 4.0f);
            ImGui::TableSetupColumn("##name",  ImGuiTableColumnFlags_WidthFixed,  0.0f);
            ImGui::TableSetupColumn("##coord", ImGuiTableColumnFlags_WidthFixed,  0.0f);
            ImGui::TableSetupColumn("##info",  ImGuiTableColumnFlags_WidthStretch);

            std::size_t thumbIdx = 0;
            for (const auto& tmpl : lib->entries()) {
                ImGui::PushID(tmpl.name.c_str());
                const void* srv = thumbIdx < templThumbSRVs_.size()
                                  ? templThumbSRVs_[thumbIdx] : nullptr;
                ++thumbIdx;

                // Collect matches; empty for unloaded templates.
                decltype(groupedEntries(*prof, tmpl.name)) group;
                if (tmpl.loaded) group = groupedEntries(*prof, tmpl.name);
                const int rowCount = std::max(1, static_cast<int>(group.size()));

                for (int i = 0; i < rowCount; ++i) {
                    ImGui::TableNextRow();

                    // Col 0: thumbnail (first row only)
                    ImGui::TableNextColumn();
                    if (i == 0) {
                        if (srv)
                            ImGui::Image(reinterpret_cast<ImTextureID>(srv),
                                         ImVec2(iconSz, iconSz));
                        else
                            ImGui::Dummy(ImVec2(iconSz, iconSz));
                    }

                    // Col 1: name button (row 0) or key label (rows 1+)
                    ImGui::TableNextColumn();
                    if (i == 0) {
                        if (ImGui::SmallButton(tmpl.name.c_str())) {
                            closeCropModal();
                            cropState_.templateName = tmpl.name;
                            const bool hasGame = ctx.gameWindow && ctx.gameWindow->valid();
                            std::optional<poebot::sys::CapturedImage> capOpt = hasGame
                                ? poebot::sys::captureClient(ctx.gameWindow->hwnd())
                                : poebot::sys::capturePrimaryScreen();
                            if (capOpt && !capOpt->pixels.empty()) {
                                auto img = poebot::vision::ImageBGRA::fromCaptured(
                                               std::move(*capOpt));
                                if (img.width != kTargetW || img.height != kTargetH)
                                    img = poebot::vision::resize(img, kTargetW, kTargetH);
                                if (!img.pixels.empty()) {
                                    cropState_.scrW      = img.width;
                                    cropState_.scrH      = img.height;
                                    cropState_.scrStride = img.stride;
                                    cropState_.pixels    = std::move(img.pixels);
                                    cropState_.active    = true;
                                }
                            }
                        }
                    } else {
                        ImGui::AlignTextToFramePadding();
                        ImGui::TextDisabled("%s", group[static_cast<std::size_t>(i)]->first.c_str());
                    }

                    // Col 2: coordinates / error / unset
                    ImGui::TableNextColumn();
                    if (!tmpl.loaded) {
                        ImGui::PushStyleColor(ImGuiCol_Text,
                                              ImVec4(1.0f, 0.42f, 0.42f, 1.0f));
                        ImGui::TextUnformatted(tr("auto_cal.template_missing"));
                        ImGui::PopStyleColor();
                    } else if (group.empty()) {
                        ImGui::TextDisabled("%s", tr("config.unset"));
                    } else {
                        const auto& cal = group[static_cast<std::size_t>(i)]->second;
                        ImGui::Text("(%d, %d)", cal.pos.x, cal.pos.y);
                    }

                    // Col 3: qty + score
                    ImGui::TableNextColumn();
                    if (tmpl.loaded && group.empty()) {
                        ImGui::TextDisabled("qty —");
                    } else if (tmpl.loaded) {
                        const auto& cal = group[static_cast<std::size_t>(i)]->second;
                        ImGui::TextDisabled("qty %d", cal.qty);
                        if (cal.score > 0.0f) {
                            ImGui::SameLine();
                            ImGui::TextDisabled("· %.2f", cal.score);
                        }
                    }
                }

                ImGui::PopID();
            }

            ImGui::EndTable();
        }
        ImGui::PopStyleVar();
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Require only that templates are loaded — the game window is optional.
    // When no game window is found the runner falls back to a full virtual-
    // screen capture so the feature stays usable in debug / headless sessions.
    const bool canStart = lib && lib->anyLoaded();

    if (!canStart) ImGui::BeginDisabled();
    if (ImGui::Button(tr("auto_cal.start"))) {
        runAutoCalibrate(ctx);
    }
    if (!canStart) ImGui::EndDisabled();

    ImGui::SameLine();
    if (ImGui::Button(tr("auto_cal.load"))) {
        // The library is owned by App and exposed read-only through ctx, so
        // the panel can't mutate it directly. The const_cast here is the
        // pragmatic escape hatch: nothing else writes the library and the
        // single thread invariant holds (this is the UI thread).
        if (auto* mut = const_cast<poebot::vision::TemplateLibrary*>(lib)) {
            mut->reload();
            calibStatus_.clear();
        }
    }

    if (!calibStatus_.empty()) {
        ImGui::SameLine();
        ImGui::TextDisabled("%s", calibStatus_.c_str());
    }
}

void ConfigPanel::runAutoCalibrate(PanelContext& ctx) {
    using poebot::i18n::tr;

    calibStatus_.clear();

    auto* prof = ctx.settings ? ctx.settings->active() : nullptr;
    if (!prof || !ctx.templates) return;

    // 1. Capture: prefer the game client area (client-space coords); fall back
    //    to the full virtual screen when no game window is present so the
    //    feature stays usable during debug without the game running.
    const bool hasGame = ctx.gameWindow && ctx.gameWindow->valid();
    std::optional<poebot::sys::CapturedImage> capOpt;
    if (hasGame) {
        capOpt = poebot::sys::captureClient(ctx.gameWindow->hwnd());
        spdlog::info("auto_cal: capturing game client area");
    } else {
        // Debug fallback: primary screen only (not the full virtual desktop).
        // SM_CXSCREEN × SM_CYSCREEN gives the primary monitor dimensions, so
        // back-projected coordinates land in (0..SM_CXSCREEN, 0..SM_CYSCREEN)
        // and match Win32 Screen coordinates directly — easily verified with
        // any spy tool without accounting for multi-monitor offsets.
        capOpt = poebot::sys::capturePrimaryScreen();
        spdlog::info("auto_cal: no game window — capturing primary screen (debug)");
    }
    if (!capOpt) {
        calibStatus_ = tr("auto_cal.err_capture");
        return;
    }
    const int captureW = capOpt->width;
    const int captureH = capOpt->height;

    // 2. Wrap as ImageBGRA (zero-copy move of the pixel buffer).
    auto haystack = poebot::vision::ImageBGRA::fromCaptured(std::move(*capOpt));

    // 3. Normalize to 1920×1080. Templates are authored at this resolution
    //    so one set works across 1080p / 1440p / 4K game windows.
    if (haystack.width != kTargetW || haystack.height != kTargetH) {
        haystack = poebot::vision::resize(haystack, kTargetW, kTargetH);
    }
    if (haystack.pixels.empty()) {
        calibStatus_ = tr("auto_cal.err_capture");
        return;
    }

    // 4. Wipe any prior entries for templates we're about to re-match. We
    //    do NOT clear entries for templates that aren't loaded right now
    //    — those might just be missing temporarily and the user shouldn't
    //    lose their previous calibration over it.
    auto& cal = prof->calibrated;
    for (const auto& tmpl : ctx.templates->entries()) {
        if (!tmpl.loaded) continue;
        for (auto it = cal.begin(); it != cal.end();) {
            if (keyBelongsTo(it->first, tmpl.name)) {
                it = cal.erase(it);
            } else {
                ++it;
            }
        }
    }

    // 5. Match each template (multi-instance, NMS-deduplicated) and
    //    record one entry per match.
    int totalMatches    = 0;
    int touchedTemplates = 0;
    for (const auto& tmpl : ctx.templates->entries()) {
        if (!tmpl.loaded) continue;

        // Template can't be larger than haystack — resize() handles the
        // common 4K/1440p → 1080p case but small game windows might still
        // produce a too-small haystack. matchAll handles this by returning
        // an empty vector, so just skip on empty result.
        const auto matches = poebot::vision::matchAll(
            haystack, tmpl.image, kMinMatchScore, kMaxInstances);
        if (matches.empty()) {
            spdlog::debug("auto_cal: '{}' no match above {:.2f}",
                          tmpl.name, kMinMatchScore);
            continue;
        }

        ++touchedTemplates;
        const int qty = static_cast<int>(matches.size());

        for (std::size_t i = 0; i < matches.size(); ++i) {
            const auto& m = matches[i];

            // m.x/m.y is already the template centre in 1080p coords.
            // Back-project to actual client coords (captureW × captureH).
            poebot::config::CalibratedCoord c;
            c.pos.x = static_cast<int>(m.x * captureW / kTargetW);
            c.pos.y = static_cast<int>(m.y * captureH / kTargetH);
            c.qty   = qty;      // group count — identical across the group
            c.score = m.score;

            // Key: bare basename when only one match, "<basename>_N" when many.
            std::string key = tmpl.name;
            if (matches.size() > 1) {
                key += "_";
                key += std::to_string(i + 1);
            }

            spdlog::info("auto_cal: '{}' -> ({}, {})  score={:.2f}  qty={}",
                         key, c.pos.x, c.pos.y, c.score, c.qty);
            cal.emplace(std::move(key), c);
            ++totalMatches;
        }
    }

    // 6. Persist and report.
    if (totalMatches > 0) {
        ctx.dirty = true;
        char buf[128];
        std::snprintf(buf, sizeof(buf), tr("auto_cal.done_fmt"),
                      totalMatches, touchedTemplates);
        calibStatus_ = buf;
    } else {
        calibStatus_ = tr("auto_cal.no_match");
    }
}

// ============================================================
// Template thumbnail SRV cache
// ============================================================

ConfigPanel::~ConfigPanel() {
    releaseTemplThumbs();
}

void ConfigPanel::releaseTemplThumbs() {
    for (void* srv : templThumbSRVs_) {
        if (srv) static_cast<ID3D11ShaderResourceView*>(srv)->Release();
    }
    templThumbSRVs_.clear();
    cachedTemplLib_ = nullptr;
}

void ConfigPanel::rebuildTemplThumbs(PanelContext& ctx) {
    releaseTemplThumbs();
    const auto* lib = ctx.templates;
    if (!lib || !ctx.d3dDevice) return;
    auto* dev = static_cast<ID3D11Device*>(ctx.d3dDevice);
    templThumbSRVs_.reserve(lib->entries().size());
    for (const auto& tmpl : lib->entries()) {
        void* srv = nullptr;
        if (tmpl.loaded) {
            // Display copy: background removed so the thumbnail shows only the
            // item outline. The original tmpl.image is untouched — matching
            // always runs on the full unprocessed image.
            poebot::vision::ImageBGRA disp = tmpl.image;
            removeBackground(disp);
            srv = createBGRASRV(dev,
                                disp.pixels.data(),
                                disp.width,
                                disp.height,
                                disp.stride);
        }
        templThumbSRVs_.push_back(srv);
    }
    cachedTemplLib_ = lib;
}

// ============================================================
// Template crop modal
//
// Opened when the user clicks a template name button. Captures the current
// screen (primary monitor or game client), normalizes to 1920×1080 so
// templates are always authored at the same scale, and lets the user
// drag-select a region. On confirm the region is PNG-encoded and written
// to the template directory (atomic), then the library reloads.
// ============================================================

void ConfigPanel::closeCropModal() {
    if (cropState_.texSRV) {
        static_cast<ID3D11ShaderResourceView*>(cropState_.texSRV)->Release();
    }
    cropState_ = {};
}

void ConfigPanel::saveCroppedTemplate(PanelContext& ctx) {
    const float scale = cropState_.dispScale;
    if (scale <= 0.0f || cropState_.scrW <= 0) return;

    // Map display-space selection back to 1920×1080 pixel coords.
    const float dax = std::min(cropState_.selAx, cropState_.selBx);
    const float day = std::min(cropState_.selAy, cropState_.selBy);
    const float dbx = std::max(cropState_.selAx, cropState_.selBx);
    const float dby = std::max(cropState_.selAy, cropState_.selBy);

    const int sx = static_cast<int>(dax / scale);
    const int sy = static_cast<int>(day / scale);
    const int sw = static_cast<int>((dbx - dax) / scale);
    const int sh = static_cast<int>((dby - day) / scale);

    // Clamp to image bounds.
    const int cx = std::clamp(sx, 0, cropState_.scrW - 1);
    const int cy = std::clamp(sy, 0, cropState_.scrH - 1);
    const int cw = std::clamp(sw, 1, cropState_.scrW - cx);
    const int ch = std::clamp(sh, 1, cropState_.scrH - cy);

    // Copy the selected region into a new ImageBGRA.
    poebot::vision::ImageBGRA cropped;
    cropped.width  = cw;
    cropped.height = ch;
    cropped.stride = cw * 4;
    cropped.pixels.resize(static_cast<std::size_t>(cw) * ch * 4);

    for (int row = 0; row < ch; ++row) {
        const uint8_t* src = cropState_.pixels.data() +
                             static_cast<std::size_t>(cy + row) * cropState_.scrStride +
                             static_cast<std::size_t>(cx) * 4;
        uint8_t* dst = cropped.pixels.data() +
                       static_cast<std::size_t>(row) * cropped.stride;
        std::memcpy(dst, src, static_cast<std::size_t>(cw) * 4);
    }

    if (auto* lib = const_cast<poebot::vision::TemplateLibrary*>(ctx.templates)) {
        lib->replaceTemplate(cropState_.templateName, cropped);
    }
}

void ConfigPanel::renderCropModal(PanelContext& ctx) {
    using poebot::i18n::tr;
    if (!cropState_.active) return;

    // Promote this window to its own OS window so it can cover the full
    // monitor independent of where the app window sits.
    // ImGuiConfigFlags_ViewportsEnable must be set (done in App::initImGui).
    ImGuiWindowClass wc{};
    wc.ViewportFlagsOverrideSet = ImGuiViewportFlags_NoAutoMerge
                                | ImGuiViewportFlags_NoDecoration
                                | ImGuiViewportFlags_NoTaskBarIcon;
    ImGui::SetNextWindowClass(&wc);

    // Cover the full monitor that currently hosts the app window.
    // appMonitorBounds() queries ImGui's platform monitor list so this adapts
    // automatically to any resolution (1080p / 2K / 4K) and multi-monitor
    // setups — no hardcoded pixel values anywhere.
    const auto [monPos, monSize] = appMonitorBounds();
    ImGui::SetNextWindowPos (monPos,  ImGuiCond_Always);
    ImGui::SetNextWindowSize(monSize, ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(1.0f);

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding,   0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,    ImVec2(12.0f, 12.0f));

    const bool cropOpen = ImGui::Begin("##CropFullscreen", nullptr,
                                       ImGuiWindowFlags_NoDecoration   |
                                       ImGuiWindowFlags_NoMove         |
                                       ImGuiWindowFlags_NoNav          |
                                       ImGuiWindowFlags_NoSavedSettings);
    ImGui::PopStyleVar(3);

    if (!cropOpen) {
        ImGui::End();
        closeCropModal();
        return;
    }

    bool doAutoSave = false;
    {   // braces keep the former BeginPopupModal indent level unchanged below

        // Lazy: create texture on first frame once pixels are ready.
        if (!cropState_.texSRV && ctx.d3dDevice && !cropState_.pixels.empty()) {
            cropState_.texSRV = createBGRASRV(
                static_cast<ID3D11Device*>(ctx.d3dDevice),
                cropState_.pixels.data(),
                cropState_.scrW, cropState_.scrH, cropState_.scrStride);
        }

        // --- Header ---
        ImGui::TextUnformatted("Recapture template:");
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.2f, 0.7f, 1.0f, 1.0f),
                           "%s", cropState_.templateName.c_str());
        ImGui::TextDisabled("Drag to select region — release mouse to save  (normalized to 1920x1080)");
        ImGui::Spacing();

        // --- Screenshot + selection ---
        if (cropState_.texSRV && cropState_.scrW > 0) {
            const float availW = ImGui::GetContentRegionAvail().x;
            const float scale  = availW / static_cast<float>(cropState_.scrW);
            const float dispW  = availW;
            const float dispH  = static_cast<float>(cropState_.scrH) * scale;
            cropState_.dispScale = scale;
            cropState_.dispW     = dispW;
            cropState_.dispH     = dispH;

            const ImVec2 imgPos = ImGui::GetCursorScreenPos();

            // Draw screenshot.
            ImGui::Image(reinterpret_cast<ImTextureID>(cropState_.texSRV),
                         ImVec2(dispW, dispH));

            // Invisible button on top captures mouse interaction.
            ImGui::SetCursorScreenPos(imgPos);
            ImGui::InvisibleButton("##imgsel", ImVec2(dispW, dispH));

            // Drag-select logic.
            const ImVec2 mouse = ImGui::GetMousePos();
            const float  rx    = std::clamp(mouse.x - imgPos.x, 0.0f, dispW);
            const float  ry    = std::clamp(mouse.y - imgPos.y, 0.0f, dispH);

            if (ImGui::IsItemHovered() &&
                ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                cropState_.selAx   = rx;
                cropState_.selAy   = ry;
                cropState_.selBx   = rx;
                cropState_.selBy   = ry;
                cropState_.hasSel  = false;
            }
            if (ImGui::IsItemActive()) {
                cropState_.selBx = rx;
                cropState_.selBy = ry;
                if (std::abs(cropState_.selBx - cropState_.selAx) > 2.0f ||
                    std::abs(cropState_.selBy - cropState_.selAy) > 2.0f) {
                    cropState_.hasSel = true;
                }
            }

            // Auto-save: fire when the mouse is released over a meaningful
            // selection (>4 px in each axis to avoid accidental single-clicks).
            if (ImGui::IsItemDeactivated() && cropState_.hasSel &&
                std::abs(cropState_.selBx - cropState_.selAx) > 4.0f &&
                std::abs(cropState_.selBy - cropState_.selAy) > 4.0f) {
                doAutoSave = true;
            }

            // Draw selection overlay.
            if (cropState_.hasSel) {
                const ImVec2 ra(imgPos.x + std::min(cropState_.selAx, cropState_.selBx),
                                imgPos.y + std::min(cropState_.selAy, cropState_.selBy));
                const ImVec2 rb(imgPos.x + std::max(cropState_.selAx, cropState_.selBx),
                                imgPos.y + std::max(cropState_.selAy, cropState_.selBy));
                ImDrawList* dl = ImGui::GetWindowDrawList();
                dl->AddRectFilled(ra, rb, IM_COL32(255, 100, 100,  45));
                dl->AddRect      (ra, rb, IM_COL32(255, 100, 100, 220), 0.0f, 0, 1.5f);

                // Show 1080p-space coords.
                const int sx = static_cast<int>(std::min(cropState_.selAx, cropState_.selBx) / scale);
                const int sy = static_cast<int>(std::min(cropState_.selAy, cropState_.selBy) / scale);
                const int sw = static_cast<int>(std::abs(cropState_.selBx - cropState_.selAx) / scale);
                const int sh = static_cast<int>(std::abs(cropState_.selBy - cropState_.selAy) / scale);
                ImGui::TextDisabled("(%d, %d)  %d x %d px  (1080p)", sx, sy, sw, sh);
            } else {
                ImGui::TextDisabled(" ");   // keeps layout height stable
            }
        } else {
            ImGui::TextDisabled("Preparing screenshot...");
            ImGui::TextDisabled(" ");
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // OK button removed — selection is saved automatically on mouse release.
        if (ImGui::Button(tr("common.cancel"), ImVec2(90, 0)) ||
            ImGui::IsKeyPressed(ImGuiKey_Escape)) {
            closeCropModal();
        }
    }   // end of former BeginPopupModal block

    // saveCroppedTemplate reads cropState_, so it must run before
    // closeCropModal() zeroes it out.
    if (doAutoSave) {
        saveCroppedTemplate(ctx);
        closeCropModal();
        // The library reloaded its entries in-place (same pointer, new pixels).
        // Invalidate the thumbnail cache so the new image is picked up next frame.
        cachedTemplLib_ = nullptr;
    }
    ImGui::End();
}

}  // namespace poebot::gui::panels
