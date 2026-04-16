#include <poebot/gui/panels/config_panel.hpp>

#include <poebot/coords.hpp>
#include <poebot/gui/capture_service.hpp>

#include <imgui.h>

namespace poebot::gui::panels {

namespace {

// Render a single coordinate row with label, value, Capture, and Reset.
// Returns true if the value was edited.
bool coordRow(const char* label, poebot::ClientPoint& p, PanelContext& ctx) {
    bool changed = false;
    ImGui::PushID(label);
    ImGui::AlignTextToFramePadding();

    // Highlight the row when it's the currently armed capture target.
    const bool armed = ctx.capture && ctx.capture->isArmedFor(&p);
    if (armed) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.2f, 1.0f, 0.4f, 1.0f));
    }

    ImGui::Text("%-12s", label);
    ImGui::SameLine(140.0f);
    if (poebot::isUnset(p)) {
        ImGui::TextDisabled("(unset)");
    } else {
        ImGui::Text("(%d, %d)", p.x, p.y);
    }
    if (armed) {
        ImGui::SameLine();
        ImGui::TextUnformatted("<< press F8 over the game window");
        ImGui::PopStyleColor();
    }

    ImGui::SameLine(armed ? 0.0f : 260.0f);
    if (!armed) {
        if (ImGui::SmallButton("Capture") && ctx.capture) {
            ctx.capture->arm(&p, label);
        }
    } else {
        if (ImGui::SmallButton("Cancel") && ctx.capture) {
            ctx.capture->cancel();
        }
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
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::TextUnformatted("Currency / orb slots");
    bool dirty = false;
    auto& c = prof->coords;
    dirty |= coordRow("orb1",     c.orb1, ctx);
    dirty |= coordRow("orb2",     c.orb2, ctx);
    dirty |= coordRow("orb3",     c.orb3, ctx);

    ImGui::Spacing();
    ImGui::TextUnformatted("Craft / map item grid anchors");
    dirty |= coordRow("baseItem", c.baseItem, ctx);
    dirty |= coordRow("p01Item",  c.p01Item,  ctx);
    dirty |= coordRow("p10Item",  c.p10Item,  ctx);

    ImGui::Spacing();
    ImGui::TextUnformatted("Inventory deposit anchors");
    dirty |= coordRow("invBase",  c.invBase, ctx);
    dirty |= coordRow("invP01",   c.invP01,  ctx);
    dirty |= coordRow("invP10",   c.invP10,  ctx);

    if (dirty) ctx.dirty = true;
}

}  // namespace poebot::gui::panels
