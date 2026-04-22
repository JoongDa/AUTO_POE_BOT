#include <poebot/gui/panels/main_layout.hpp>

#include <poebot/gui/capture_service.hpp>
#include <poebot/win/window.hpp>

#include <imgui.h>

namespace poebot::gui::panels {

namespace {

constexpr float kSidebarWidth   = 110.0f;
constexpr float kLogStripHeight = 180.0f;

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
            if (ImGui::MenuItem("中文", nullptr, zh)) {
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

    // Right-aligned status: capture countdown + game window state.
    const float padRight = 20.0f;
    float offset = ImGui::GetWindowWidth() - padRight;

    // Game window label (always shown, rightmost).
    const bool found = ctx.gameWindow && ctx.gameWindow->valid();
    const char* wndLabel = found ? "POE: FOUND" : "POE: ---";
    const ImVec4 wndColor = found ? ImVec4(0.3f, 1.0f, 0.4f, 1.0f)
                                  : ImVec4(0.6f, 0.6f, 0.6f, 1.0f);
    offset -= ImGui::CalcTextSize(wndLabel).x;

    // Capture countdown (left of window label when active).
    if (ctx.capture && ctx.capture->active()) {
        char buf[96];
        std::snprintf(buf, sizeof(buf), "Recording '%.*s' %.1fs — switch to game",
                      static_cast<int>(ctx.capture->activeName().size()),
                      ctx.capture->activeName().data(),
                      ctx.capture->remainingSeconds());
        const float w = ImGui::CalcTextSize(buf).x;
        ImGui::SameLine(offset - w - 20.0f);
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.2f, 1.0f, 0.4f, 1.0f));
        ImGui::TextUnformatted(buf);
        ImGui::PopStyleColor();
    }

    ImGui::SameLine(offset);
    ImGui::PushStyleColor(ImGuiCol_Text, wndColor);
    ImGui::TextUnformatted(wndLabel);
    ImGui::PopStyleColor();

    ImGui::EndMenuBar();
}

// Pick which panel to render based on activePanel (by Panel::name()).
Panel* resolveActiveTabPanel(const std::vector<std::unique_ptr<Panel>>& panels,
                             const std::string& name) {
    Panel* first = nullptr;
    for (auto& p : panels) {
        if (!p || p->kind() != PanelKind::Tab) continue;
        if (!first) first = p.get();
        if (name == p->name()) return p.get();
    }
    return first;  // fallback: first tab panel if nothing matches
}

void renderSidebar(const std::vector<std::unique_ptr<Panel>>& panels,
                   PanelContext& ctx) {
    // macOS sidebar feel: rows have no fill when idle, a subtle wash on
    // hover, and the accent blue when selected. Text is left-aligned.
    const ImVec4 accent     = ImGui::GetStyle().Colors[ImGuiCol_ButtonActive];
    const ImVec4 hoverWash  = ImVec4(1, 1, 1, 0.06f);
    const ImVec4 activeWash = ImVec4(1, 1, 1, 0.10f);

    ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.0f, 0.5f));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,    ImVec2(12, 8));

    for (auto& p : panels) {
        if (!p || p->kind() != PanelKind::Tab) continue;
        const bool selected = (ctx.activePanel == p->name());

        if (selected) {
            ImGui::PushStyleColor(ImGuiCol_Button,        accent);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, accent);
            ImGui::PushStyleColor(ImGuiCol_ButtonActive,  accent);
        } else {
            ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0, 0, 0, 0));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, hoverWash);
            ImGui::PushStyleColor(ImGuiCol_ButtonActive,  activeWash);
        }

        if (ImGui::Button(p->name(), ImVec2(-FLT_MIN, 34.0f))) {
            ctx.activePanel = p->name();
        }

        ImGui::PopStyleColor(3);
    }

    ImGui::PopStyleVar(2);
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
    if (topHeight < 150.0f) topHeight = 150.0f;

    // Top region = sidebar + active panel.
    ImGui::BeginChild("##TopRegion", ImVec2(0, topHeight), false);
    {
        ImGui::BeginChild("##Sidebar", ImVec2(kSidebarWidth, 0), true);
        renderSidebar(panels, ctx);
        ImGui::EndChild();

        ImGui::SameLine();

        ImGui::BeginChild("##ActivePanel", ImVec2(0, 0), true);
        if (Panel* active = resolveActiveTabPanel(panels, ctx.activePanel)) {
            active->render(ctx);
        }
        ImGui::EndChild();
    }
    ImGui::EndChild();

    // Bottom log strip.
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
