#include <poebot/gui/panels/main_layout.hpp>

#include <poebot/gui/capture_service.hpp>
#include <poebot/i18n/i18n.hpp>
#include <poebot/win/window.hpp>

#include <imgui.h>

namespace poebot::gui::panels {

namespace {

constexpr float kSidebarWidth  = 130.0f;
constexpr float kLogPanelWidth = 360.0f;

void renderMenuBar(PanelContext& ctx, bool& wantExit) {
    using poebot::i18n::tr;
    if (!ImGui::BeginMenuBar()) return;

    if (ImGui::BeginMenu(tr("menu.profile"))) {
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

    if (ImGui::BeginMenu(tr("menu.language"))) {
        if (ctx.settings) {
            const bool zh = ctx.settings->language == "zh";
            const bool en = ctx.settings->language == "en";
            // Language labels stay in their own language so each is always
            // recognizable regardless of the currently active table.
            if (ImGui::MenuItem(tr("menu.language.zh"), nullptr, zh)) {
                if (!zh) {
                    ctx.settings->language = "zh";
                    poebot::i18n::setLanguage("zh");
                    ctx.dirty = true;
                }
            }
            if (ImGui::MenuItem(tr("menu.language.en"), nullptr, en)) {
                if (!en) {
                    ctx.settings->language = "en";
                    poebot::i18n::setLanguage("en");
                    ctx.dirty = true;
                }
            }
        }
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu(tr("menu.app"))) {
        // Appearance lives in the bottom-right floating button (renderAppearanceFab).
        if (ImGui::MenuItem(tr("menu.app.exit"))) wantExit = true;
        ImGui::EndMenu();
    }

    // Right-aligned status: capture countdown + game window state.
    const float padRight = 20.0f;
    float offset = ImGui::GetWindowWidth() - padRight;

    // Game window label (always shown, rightmost). Apple's "systemGreen"
    // (#34C759, ≈0.20,0.78,0.35) reads correctly on both Light and Dark.
    const bool found = ctx.gameWindow && ctx.gameWindow->valid();
    const char* wndLabel = found ? tr("status.poe.found") : tr("status.poe.missing");
    const ImVec4 wndColor = found ? ImVec4(0.20f, 0.78f, 0.35f, 1.0f)
                                  : ImVec4(0.55f, 0.55f, 0.55f, 1.0f);
    offset -= ImGui::CalcTextSize(wndLabel).x;

    // Capture countdown (left of window label when active).
    if (ctx.capture && ctx.capture->active()) {
        char buf[128];
        std::snprintf(buf, sizeof(buf), tr("status.recording_fmt"),
                      static_cast<int>(ctx.capture->activeName().size()),
                      ctx.capture->activeName().data(),
                      ctx.capture->remainingSeconds());
        const float w = ImGui::CalcTextSize(buf).x;
        ImGui::SameLine(offset - w - 20.0f);
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.20f, 0.78f, 0.35f, 1.0f));
        ImGui::TextUnformatted(buf);
        ImGui::PopStyleColor();
    }

    ImGui::SameLine(offset);
    ImGui::PushStyleColor(ImGuiCol_Text, wndColor);
    ImGui::TextUnformatted(wndLabel);
    ImGui::PopStyleColor();

    ImGui::EndMenuBar();
}

// Theme-switch FAB. Must be called INSIDE the sidebar BeginChild — it pins
// itself to the bottom-right corner of whatever window is currently active.
//
// The icon is hand-drawn with ImDrawList rather than a Unicode glyph (e.g.
// ◐ U+25D0). Our merged Segoe UI + YaHei atlas only loads the
// ChineseSimplifiedCommon glyph range, which omits the Geometric Shapes
// block — using the codepoint would render as the ImGui "missing glyph"
// box, which is what we saw on the previous attempt.
void renderAppearanceFab(PanelContext& ctx) {
    using poebot::i18n::tr;
    if (!ctx.settings) return;

    constexpr float kBtnSize = 28.0f;

    // Pin to the bottom-right of the current window (the sidebar). Using
    // window-local content-region coords, not absolute viewport coords, so
    // the FAB tracks the sidebar even when the host window is resized.
    //
    // `contentMax` already sits one WindowPadding (12px) inside the panel
    // border. Parking the FAB flush against that boundary leaves a 12px
    // gap to the border, which reads as too much whitespace for a control
    // that's supposed to feel tucked into the corner. `kOverhang` pushes
    // the FAB half-way back into the padding so the visible gap is ~6px.
    constexpr float kOverhang = 6.0f;
    const ImVec2 contentMax = ImGui::GetWindowContentRegionMax();
    ImGui::SetCursorPos(ImVec2(contentMax.x - kBtnSize + kOverhang,
                               contentMax.y - kBtnSize + kOverhang));

    const ImVec2 cursor = ImGui::GetCursorScreenPos();
    const ImVec2 center(cursor.x + kBtnSize * 0.5f, cursor.y + kBtnSize * 0.5f);

    ImGui::InvisibleButton("##FabBtn", ImVec2(kBtnSize, kBtnSize));
    const bool hovered = ImGui::IsItemHovered();
    const bool clicked = ImGui::IsItemClicked();

    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImU32 textCol = ImGui::GetColorU32(ImGuiCol_Text);

    // Hover wash — a rounded rectangle behind the glyph, matching the
    // project's FrameRounding so it reads as a toolbar button rather
    // than a floating circular highlight. Only drawn on hover; clears
    // the moment the cursor leaves.
    if (hovered) {
        const ImVec2 bgMax(cursor.x + kBtnSize, cursor.y + kBtnSize);
        dl->AddRectFilled(cursor, bgMax,
                          ImGui::GetColorU32(ImGuiCol_ButtonHovered),
                          ImGui::GetStyle().FrameRounding);
    }

    // Appearance glyph: a horizontal capsule (pill) outline with a small
    // filled dot — drawn like a physical toggle. The dot position tracks
    // the current theme (light = left / off, dark = right / on) so the
    // icon's resting state matches reality instead of always reading as
    // "on". Draws with the current text color so it auto-inverts between
    // Light and Dark themes.
    const float pillW = kBtnSize * 0.70f;   // capsule width
    const float pillH = kBtnSize * 0.40f;   // capsule height
    const float pillR = pillH * 0.5f;       // full-pill rounding

    const ImVec2 a(center.x - pillW * 0.5f, center.y - pillH * 0.5f);
    const ImVec2 b(center.x + pillW * 0.5f, center.y + pillH * 0.5f);
    dl->AddRect(a, b, textCol, pillR, 0, 1.4f);

    // Dot on the left for Light mode (default), on the right for Dark.
    // Computed once and reused by the popup below so the toggle glyph and
    // the menu-item checkmarks can't drift out of sync.
    const bool light = ctx.settings->appearance == "light";
    const bool dark  = ctx.settings->appearance == "dark";
    const ImVec2 dotCenter(dark ? b.x - pillR : a.x + pillR, center.y);
    dl->AddCircleFilled(dotCenter, pillR * 0.62f, textCol);

    if (clicked) ImGui::OpenPopup("##AppearancePopup");

    if (ImGui::BeginPopup("##AppearancePopup")) {
        ImGui::TextDisabled("%s", tr("menu.app.appearance"));
        ImGui::Separator();

        if (ImGui::MenuItem(tr("menu.app.appearance.light"), nullptr, light)) {
            if (!light) {
                ctx.settings->appearance = "light";
                ctx.dirty = true;
                if (ctx.onAppearanceChanged) ctx.onAppearanceChanged();
            }
        }
        if (ImGui::MenuItem(tr("menu.app.appearance.dark"), nullptr, dark)) {
            if (!dark) {
                ctx.settings->appearance = "dark";
                ctx.dirty = true;
                if (ctx.onAppearanceChanged) ctx.onAppearanceChanged();
            }
        }
        ImGui::EndPopup();
    }
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
    const ImVec4 accent = ImGui::GetStyle().Colors[ImGuiCol_ButtonActive];

    // Wash color has to flip with the theme: white-on-dark, black-on-light.
    // Rec.601 luma of the window background is the cheapest way to decide
    // without plumbing the theme flag through the panel context.
    const ImVec4& wbg = ImGui::GetStyle().Colors[ImGuiCol_WindowBg];
    const float   luma = 0.299f * wbg.x + 0.587f * wbg.y + 0.114f * wbg.z;
    const bool    isDark = luma < 0.5f;
    const ImVec4  hoverWash  = isDark ? ImVec4(1, 1, 1, 0.06f)
                                      : ImVec4(0, 0, 0, 0.05f);
    const ImVec4  activeWash = isDark ? ImVec4(1, 1, 1, 0.10f)
                                      : ImVec4(0, 0, 0, 0.08f);

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

        // Push name() as the stable ImGui id so the button doesn't lose its
        // state when label() changes with the active language.
        ImGui::PushID(p->name());
        if (ImGui::Button(p->label(), ImVec2(-FLT_MIN, 34.0f))) {
            ctx.activePanel = p->name();
        }
        ImGui::PopID();

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

    // macOS-style edge breathing room: panels never touch the window frame,
    // and content never touches the panel borders.
    //   kEdgePad  → host inset (gap between window frame and child panels)
    //   kPanelPad → inner inset (gap between panel border and its content)
    //   kColumnGap → horizontal channel between the three columns
    constexpr float kEdgePad   = 14.0f;
    constexpr float kPanelPad  = 12.0f;
    constexpr float kColumnGap = 12.0f;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding,   0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,    ImVec2(kEdgePad, kEdgePad));
    ImGui::Begin("##MainHost", nullptr, kHostFlags);
    ImGui::PopStyleVar(3);

    renderMenuBar(ctx, wantExit);

    // Re-push WindowPadding so each child window gets an internal inset
    // (BeginChild inherits the *current* style; the host's padding was
    // already popped above). Wider column gap than the default ItemSpacing
    // gives the three panels visible "channels" between them.
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,
                        ImVec2(kPanelPad, kPanelPad));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,
                        ImVec2(kColumnGap, ImGui::GetStyle().ItemSpacing.y));

    // Three-column layout: [sidebar | active panel | log panel].
    // The sidebar carries the FAB at its bottom-right; the active panel
    // fills whatever's between, the log panel hugs the right edge.
    //
    // ImGuiChildFlags_AlwaysUseWindowPadding is REQUIRED here. Without it,
    // ImGui silently zeros WindowPadding for any child whose effective
    // border size is 0 (and our macOS-ish style sets ChildBorderSize=0 to
    // avoid hairline borders). The Borders flag is intentionally omitted —
    // we rely on ChildBg color contrast, not lines, for panel separation.
    constexpr ImGuiChildFlags kPanelChildFlags =
        ImGuiChildFlags_AlwaysUseWindowPadding;

    // Sidebar is purely a fixed navigation strip — no scrolling, ever.
    //   NoScrollbar       hides the bar (FAB overhangs contentMax by a few
    //                     px which would otherwise auto-mount one).
    //   NoScrollWithMouse blocks wheel input. Without it, hovering + wheel
    //                     still translates content vertically, causing the
    //                     tabs to jitter even though the bar is hidden.
    // Both flags are needed; NoScrollbar alone only hides the visual.
    constexpr ImGuiWindowFlags kSidebarFlags =
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
    ImGui::BeginChild("##Sidebar", ImVec2(kSidebarWidth, 0),
                      kPanelChildFlags, kSidebarFlags);
    renderSidebar(panels, ctx);
    renderAppearanceFab(ctx);
    ImGui::EndChild();

    ImGui::SameLine();

    const float availWidth  = ImGui::GetContentRegionAvail().x;
    const float middleWidth = availWidth - kLogPanelWidth - kColumnGap;

    ImGui::BeginChild("##ActivePanel", ImVec2(middleWidth, 0), kPanelChildFlags);
    if (Panel* active = resolveActiveTabPanel(panels, ctx.activePanel)) {
        active->render(ctx);
    }
    ImGui::EndChild();

    ImGui::SameLine();

    ImGui::BeginChild("##LogRegion", ImVec2(0, 0), kPanelChildFlags);
    for (auto& p : panels) {
        if (p && p->kind() == PanelKind::Log) {
            p->render(ctx);
            break;
        }
    }
    ImGui::EndChild();

    ImGui::PopStyleVar(2);  // ItemSpacing + WindowPadding

    ImGui::End();
}

}  // namespace poebot::gui::panels
