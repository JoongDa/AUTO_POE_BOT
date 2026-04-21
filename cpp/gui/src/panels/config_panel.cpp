#include <poebot/gui/panels/config_panel.hpp>

#include <poebot/config/profile.hpp>
#include <poebot/config/settings_io.hpp>
#include <poebot/coords.hpp>
#include <poebot/gui/capture_service.hpp>

#include <imgui.h>
#include <spdlog/spdlog.h>

namespace poebot::gui::panels {

namespace {

// One coord row: field name, value, hotkey-label button, Reset button.
// Clicking the hotkey button is equivalent to pressing the hotkey itself:
// it starts a 3-second countdown via CaptureService.
bool coordRow(const char* name, const char* hotkeyLabel,
              poebot::ClientPoint& p, PanelContext& ctx) {
    bool changed = false;
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
        ImGui::TextDisabled("(unset)");
    } else {
        ImGui::Text("(%d, %d)", p.x, p.y);
    }
    if (armed) ImGui::PopStyleColor();

    ImGui::SameLine(250.0f);
    if (ImGui::SmallButton(hotkeyLabel) && ctx.capture) {
        ctx.capture->startCapture(name);
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("Reset")) {
        if (armed && ctx.capture) ctx.capture->cancel();
        p = {};
        changed = true;
    }
    ImGui::PopID();
    return changed;
}

}  // namespace

void ConfigPanel::render(PanelContext& ctx) {
    if (!ctx.settings) {
        ImGui::TextUnformatted("No settings loaded.");
        return;
    }
    auto* prof = ctx.settings->active();
    if (!prof) {
        ImGui::Text("No active profile ('%s' not found).",
                    ctx.settings->activeProfile.c_str());
        return;
    }

    ImGui::Text("Active profile: %s  [%s]",
                prof->displayName.c_str(), prof->name.c_str());
    ImGui::TextDisabled("Window match: \"%s\"", prof->windowTitlePattern.c_str());
    ImGui::TextDisabled("Tip: hover the target in the game, then press the hotkey. "
                        "You have 3 seconds to switch windows.");
    ImGui::Separator();
    ImGui::Spacing();

    bool dirty = false;
    auto& c = prof->coords;

    ImGui::TextUnformatted("Currency / orb slots");
    dirty |= coordRow("orb1",     "Alt+1", c.orb1, ctx);
    dirty |= coordRow("orb2",     "Alt+2", c.orb2, ctx);
    dirty |= coordRow("orb3",     "Alt+3", c.orb3, ctx);

    ImGui::Spacing();
    ImGui::TextUnformatted("Craft / map item grid anchors");
    dirty |= coordRow("baseItem", "Alt+0", c.baseItem, ctx);
    dirty |= coordRow("p01Item",  "Alt+8", c.p01Item,  ctx);
    dirty |= coordRow("p10Item",  "Alt+9", c.p10Item,  ctx);

    ImGui::Spacing();
    ImGui::TextUnformatted("Inventory deposit anchors");
    dirty |= coordRow("invBase",  "Alt+4", c.invBase, ctx);
    dirty |= coordRow("invP01",   "Alt+5", c.invP01,  ctx);
    dirty |= coordRow("invP10",   "Alt+6", c.invP10,  ctx);

    if (dirty) ctx.dirty = true;

    // --- Save / Reset -----------------------------------------------------
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    if (ImGui::Button("Save")) {
        if (ctx.settingsPath) {
            if (poebot::config::saveSettings(*ctx.settings, *ctx.settingsPath)) {
                ctx.dirty = false;
                spdlog::info("config: settings saved to {}", ctx.settingsPath->string());
            }
        }
    }

    ImGui::SameLine();
    if (ImGui::Button("Reset profile to defaults")) {
        if (ctx.capture) ctx.capture->cancel();
        auto def = poebot::config::defaultProfileFor(prof->name);
        prof->coords  = def.coords;
        prof->craft   = def.craft;
        prof->map     = def.map;
        prof->deposit = def.deposit;
        prof->stats   = def.stats;
        ctx.dirty = true;
        spdlog::info("config: profile '{}' reset to defaults", prof->name);
    }
}

}  // namespace poebot::gui::panels
