#include <poebot/gui/panels/config_panel.hpp>

#include <poebot/coords.hpp>

#include <imgui.h>
#include <spdlog/spdlog.h>

namespace poebot::gui::panels {

namespace {

// Render a single coordinate row: label + current value + Reset button.
// Returns true if the value was edited this frame.
bool coordRow(const char* label, poebot::ClientPoint& p) {
    bool changed = false;
    ImGui::PushID(label);
    ImGui::AlignTextToFramePadding();
    ImGui::Text("%-12s", label);
    ImGui::SameLine(140.0f);
    if (poebot::isUnset(p)) {
        ImGui::TextDisabled("(unset)");
    } else {
        ImGui::Text("(%d, %d)", p.x, p.y);
    }
    ImGui::SameLine(260.0f);
    if (ImGui::SmallButton("Capture")) {
        // Phase 3 will hook this to a global hotkey / mouse position reader.
        spdlog::info("capture '{}' — not implemented in Phase 2", label);
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("Reset")) {
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
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::TextUnformatted("Currency / orb slots");
    bool dirty = false;
    auto& c = prof->coords;
    dirty |= coordRow("orb1",     c.orb1);
    dirty |= coordRow("orb2",     c.orb2);
    dirty |= coordRow("orb3",     c.orb3);

    ImGui::Spacing();
    ImGui::TextUnformatted("Craft / map item grid anchors");
    dirty |= coordRow("baseItem", c.baseItem);
    dirty |= coordRow("p01Item",  c.p01Item);
    dirty |= coordRow("p10Item",  c.p10Item);

    ImGui::Spacing();
    ImGui::TextUnformatted("Inventory deposit anchors");
    dirty |= coordRow("invBase",  c.invBase);
    dirty |= coordRow("invP01",   c.invP01);
    dirty |= coordRow("invP10",   c.invP10);

    if (dirty) ctx.dirty = true;
}

}  // namespace poebot::gui::panels
