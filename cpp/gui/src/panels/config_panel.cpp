#include <poebot/gui/panels/config_panel.hpp>

#include <poebot/config/profile.hpp>
#include <poebot/coords.hpp>
#include <poebot/gui/capture_service.hpp>
#include <poebot/i18n/i18n.hpp>

#include <imgui.h>
#include <spdlog/spdlog.h>

namespace poebot::gui::panels {

namespace {

// One coord row: field name, value, hotkey-label button. To clear or change
// a coord, press the hotkey (or click the hotkey button) and re-capture —
// it overwrites in place. There's no per-row Reset because the global
// "Reset profile to defaults" button already covers the clear-everything
// case, and re-capture covers the clear-one case.
//
// The `name` argument is the stable field id (English, used as the capture
// key and as the ImGui PushID id); it's also shown verbatim in the row label
// since these are technical identifiers rather than user-facing copy.
void coordRow(const char* name, const char* hotkeyLabel,
              poebot::ClientPoint& p, PanelContext& ctx) {
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

    ImGui::SameLine(250.0f);
    if (ImGui::SmallButton(hotkeyLabel) && ctx.capture) {
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

    ImGui::Text(tr("config.active_profile_fmt"),
                prof->displayName.c_str(), prof->name.c_str());
    ImGui::Separator();
    ImGui::Spacing();

    // Coord rows don't need to accumulate a dirty flag: CaptureService marks
    // panelCtx_.dirty when a capture commits (see App::run loop).
    auto& c = prof->coords;

    ImGui::TextUnformatted(tr("config.section.orbs"));
    coordRow("orb1",     "Alt+1", c.orb1, ctx);
    coordRow("orb2",     "Alt+2", c.orb2, ctx);
    coordRow("orb3",     "Alt+3", c.orb3, ctx);

    ImGui::Spacing();
    ImGui::TextUnformatted(tr("config.section.anchors"));
    coordRow("baseItem", "Alt+0", c.baseItem, ctx);
    coordRow("p01Item",  "Alt+8", c.p01Item,  ctx);
    coordRow("p10Item",  "Alt+9", c.p10Item,  ctx);

    ImGui::Spacing();
    ImGui::TextUnformatted(tr("config.section.inventory"));
    coordRow("invBase",  "Alt+4", c.invBase, ctx);
    coordRow("invP01",   "Alt+5", c.invP01,  ctx);
    coordRow("invP10",   "Alt+6", c.invP10,  ctx);

    // --- Reset ------------------------------------------------------------
    // Save is gone: settings auto-persist on dirty (App::saveSettingsIfDirty
    // throttles writes ~every 800ms) and on exit, so an explicit Save button
    // duplicates work the user can't tell apart. Reset stays but now requires
    // a confirm step — it wipes coords + craft + map + deposit + stats in one
    // shot and there's no undo.
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    if (ImGui::Button(tr("config.button.reset_profile"))) {
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
