#include <poebot/gui/panels/main_layout.hpp>

#include <poebot/gui/capture_service.hpp>
#include <poebot/i18n/i18n.hpp>
#include <poebot/win/window.hpp>

#include <imgui.h>

namespace poebot::gui::panels {

namespace {

constexpr float kSidebarWidth  = 130.0f;
constexpr float kLogPanelWidth = 360.0f;

void renderMenuBar(PanelContext& ctx) {
    using poebot::i18n::tr;
    if (!ImGui::BeginMenuBar()) return;

    // Profile segmented control on the left of the bar — one click swaps
    // between PoE 1 / PoE 2 instead of the previous two-click dropdown.
    // Selected segment uses the accent color; unselected is transparent.
    // Adjacent segments touch (ItemSpacing.x = 0) so they read as one
    // segmented control rather than two independent buttons.
    if (ctx.settings) {
        const ImVec4 accent = ImGui::GetStyle().Colors[ImGuiCol_ButtonActive];
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,
                            ImVec2(0, ImGui::GetStyle().ItemSpacing.y));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
        bool first = true;
        for (auto& [key, prof] : ctx.settings->profiles) {
            if (!first) ImGui::SameLine(0, 0);
            first = false;

            const bool selected = (ctx.settings->activeProfile == key);
            const char* label   = prof.displayName.empty()
                                  ? key.c_str()
                                  : prof.displayName.c_str();

            if (selected) {
                ImGui::PushStyleColor(ImGuiCol_Button,        accent);
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, accent);
                ImGui::PushStyleColor(ImGuiCol_ButtonActive,  accent);
            } else {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
            }
            ImGui::PushID(key.c_str());
            if (ImGui::Button(label)) {
                if (!selected) {
                    ctx.settings->activeProfile = key;
                    ctx.dirty = true;
                }
            }
            ImGui::PopID();
            ImGui::PopStyleColor(selected ? 3 : 1);
        }
        ImGui::PopStyleVar(2);
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

    // Hover wash — gray rounded rect behind the glyph, only while the
    // cursor is over the FAB. Idle state has no chip; the two pills
    // float on the sidebar background.
    if (hovered) {
        const ImVec2 bgMax(cursor.x + kBtnSize, cursor.y + kBtnSize);
        dl->AddRectFilled(cursor, bgMax,
                          ImGui::GetColorU32(ImGuiCol_ButtonHovered),
                          ImGui::GetStyle().FrameRounding);
    }

    // Two stacked toggle-pill glyphs — top tracks Appearance, bottom
    // tracks Language. Both pills are pure ImDrawList primitives so the
    // glyph survives the merged Segoe UI + YaHei atlas (which only loads
    // the ChineseSimplifiedCommon range and would render any Geometric
    // Shapes codepoint as a missing-glyph box).
    //
    // Drawing two pills instead of one signals "multiple toggles live
    // here" — matches the popup contents (Appearance + Language) and
    // gives the user at-a-glance feedback about both states without
    // having to open the menu.
    // Each pill is the original single-pill design (W=0.70, H=0.40 of
    // kBtnSize) scaled uniformly by 0.5 — same aspect ratio (1.75:1),
    // half the dimensions — so two of them stack into roughly the
    // visual footprint the single pill used to occupy.
    const float pillW = kBtnSize * 0.35f;   // capsule width  (half of 0.70)
    const float pillH = kBtnSize * 0.20f;   // capsule height (half of 0.40)
    const float pillR = pillH * 0.5f;       // full-pill rounding
    const float gapY  = 2.0f;                // vertical gap between pills

    const float topY    = center.y - pillH - gapY * 0.5f;
    const float bottomY = center.y          + gapY * 0.5f;

    const bool light = ctx.settings->appearance == "light";
    const bool dark  = ctx.settings->appearance == "dark";
    const bool zh    = ctx.settings->language == "zh";
    const bool en    = ctx.settings->language == "en";

    // Outline capsule + outline thumb circle, both in TextDisabled gray
    // so the icon reads as quiet UI furniture rather than competing with
    // primary text. The thumb circle's radius = pillR, so its outer edge
    // sits flush with the capsule's rounded end on whichever side the
    // current state points to.
    const ImU32 pillCol = ImGui::GetColorU32(ImGuiCol_TextDisabled);
    auto drawPill = [&](float yTop, bool dotOnRight) {
        const ImVec2 a(center.x - pillW * 0.5f, yTop);
        const ImVec2 b(center.x + pillW * 0.5f, yTop + pillH);
        dl->AddRect(a, b, pillCol, pillR, 0, 1.0f);
        const ImVec2 dot(dotOnRight ? b.x - pillR : a.x + pillR,
                         yTop + pillH * 0.5f);
        dl->AddCircle(dot, pillR, pillCol, 0, 1.0f);
    };

    // Top: Appearance — Light = left dot (default), Dark = right dot.
    // Bottom: Language — convention follows the popup's menu order
    // (中文 listed first → left dot for zh, right dot for en).
    drawPill(topY,    /*dotOnRight=*/dark);
    drawPill(bottomY, /*dotOnRight=*/en);

    if (clicked) ImGui::OpenPopup("##AppearancePopup");

    if (ImGui::BeginPopup("##AppearancePopup")) {
        // Appearance section ----------------------------------------------
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

        // Language section ------------------------------------------------
        // Folded into the same popup (was its own top-level menu) because
        // language is a once-per-install setting, not a frequent toggle.
        // Labels stay in their own language so each is recognizable
        // regardless of the currently active table.
        ImGui::Spacing();
        ImGui::TextDisabled("%s", tr("menu.language"));
        ImGui::Separator();

        // zh/en already computed above (used by the bottom toggle pill).
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
                      PanelContext& ctx) {
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

    renderMenuBar(ctx);

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
