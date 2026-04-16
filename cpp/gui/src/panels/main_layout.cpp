#include <poebot/gui/panels/main_layout.hpp>

#include <poebot/gui/capture_service.hpp>
#include <poebot/win/window.hpp>

#include <imgui.h>

namespace poebot::gui::panels {

namespace {

constexpr float kLogStripHeight = 200.0f;

void renderMenuBar(PanelContext& ctx, bool& wantExit) {
    if (!ImGui::BeginMenuBar()) return;

    if (ImGui::BeginMenu("Profile")) {
        if (ctx.settings) {
            for (auto& [key, prof] : ctx.settings->profiles) {
                const bool selected = (ctx.settings->activeProfile == key);
                const char* label = prof.displayName.empty() ? key.c_str() : prof.displayName.c_str();
                if (ImGui::MenuItem(label, nullptr, selected)) {
                    if (!selected) {
                        ctx.settings->activeProfile = key;
                        ctx.dirty = true;
                    }
                }
            }
        }
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Language")) {
        if (ctx.settings) {
            const bool zh = ctx.settings->language == "zh";
            const bool en = ctx.settings->language == "en";
            if (ImGui::MenuItem("Chinese", nullptr, zh)) {
                if (!zh) { ctx.settings->language = "zh"; ctx.dirty = true; }
            }
            if (ImGui::MenuItem("English", nullptr, en)) {
                if (!en) { ctx.settings->language = "en"; ctx.dirty = true; }
            }
        }
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("App")) {
        if (ImGui::MenuItem("Exit")) wantExit = true;
        ImGui::EndMenu();
    }

    // Right-aligned status indicators.
    {
        // Game-window status.
        const bool found = ctx.gameWindow && ctx.gameWindow->valid();
        const char* wndLabel = found ? "POE: FOUND" : "POE: ---";
        ImVec4 wndColor = found ? ImVec4(0.3f, 1.0f, 0.4f, 1.0f)
                                : ImVec4(0.6f, 0.6f, 0.6f, 1.0f);
        float offset = ImGui::GetWindowWidth() - ImGui::CalcTextSize(wndLabel).x - 20.0f;
        if (ctx.capture && ctx.capture->armed()) {
            const char* capLabel = "F8 capture";
            offset -= ImGui::CalcTextSize(capLabel).x + 16.0f;
            ImGui::SameLine(offset);
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.2f, 1.0f, 0.4f, 1.0f));
            ImGui::TextUnformatted(capLabel);
            ImGui::PopStyleColor();
            offset += ImGui::CalcTextSize(capLabel).x + 16.0f;
        }
        ImGui::SameLine(offset);
        ImGui::PushStyleColor(ImGuiCol_Text, wndColor);
        ImGui::TextUnformatted(wndLabel);
        ImGui::PopStyleColor();
    }

    ImGui::EndMenuBar();
}

}  // namespace

void renderMainLayout(const std::vector<std::unique_ptr<Panel>>& panels,
                      PanelContext& ctx,
                      bool& wantExit) {
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(vp->WorkPos);
    ImGui::SetNextWindowSize(vp->WorkSize);

    constexpr ImGuiWindowFlags kHostFlags =
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_MenuBar;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::Begin("##MainHost", nullptr, kHostFlags);
    ImGui::PopStyleVar(2);

    renderMenuBar(ctx, wantExit);

    const ImVec2 avail = ImGui::GetContentRegionAvail();
    float topHeight = avail.y - kLogStripHeight - ImGui::GetStyle().ItemSpacing.y;
    if (topHeight < 120.0f) topHeight = 120.0f;

    // Tabs for every Tab-kind panel.
    ImGui::BeginChild("##TopRegion", ImVec2(0, topHeight), true);
    if (ImGui::BeginTabBar("##PanelTabs", ImGuiTabBarFlags_Reorderable)) {
        for (auto& p : panels) {
            if (!p || p->kind() != PanelKind::Tab) continue;
            if (ImGui::BeginTabItem(p->name())) {
                p->render(ctx);
                ImGui::EndTabItem();
            }
        }
        ImGui::EndTabBar();
    }
    ImGui::EndChild();

    // Bottom: the (single) Log-kind panel fills the remaining strip.
    ImGui::BeginChild("##LogRegion", ImVec2(0, 0), true);
    for (auto& p : panels) {
        if (p && p->kind() == PanelKind::Log) {
            p->render(ctx);
            break;
        }
    }
    ImGui::EndChild();

    ImGui::End();
}

}  // namespace poebot::gui::panels
