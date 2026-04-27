#include <poebot/gui/app.hpp>

#include <poebot/gui/panels/config_panel.hpp>
#include <poebot/gui/panels/craft_panel.hpp>
#include <poebot/gui/panels/deposit_panel.hpp>
#include <poebot/gui/panels/log_panel.hpp>
#include <poebot/gui/panels/main_layout.hpp>
#include <poebot/gui/panels/map_panel.hpp>

#include <poebot/config/affix_library.hpp>
#include <poebot/config/settings_io.hpp>
#include <poebot/i18n/i18n.hpp>
#include <poebot/version.hpp>

#include <imgui.h>
#include <imgui_impl_dx11.h>
#include <imgui_impl_win32.h>

#include <spdlog/spdlog.h>
#include <spdlog/sinks/msvc_sink.h>
#include <spdlog/sinks/rotating_file_sink.h>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

namespace poebot::gui {

namespace {
constexpr auto kSaveDebounce        = std::chrono::milliseconds(500);
constexpr auto kWindowRefreshPeriod = std::chrono::seconds(1);
// How often we re-assert HWND_TOPMOST. 1Hz is plenty — the only reason we
// re-pin at all is to defend against other topmost windows that occasionally
// stack above us (RGB utilities, OBS captures, etc.). A more aggressive
// period would burn CPU for no observable benefit.
constexpr auto kTopmostPinPeriod    = std::chrono::seconds(1);
}

int App::run(HINSTANCE hInstance, int nCmdShow) {
    initLogging();
    spdlog::info("{} v{} starting", poebot::kAppName, poebot::kAppVersion);

    loadOrDefaultSettings();
    poebot::i18n::setLanguage(settings_.language);

    if (!window_.create(hInstance, L"POE Bot v0.1.0", 1200, 760)) {
        spdlog::error("failed to create main window");
        return 1;
    }

    // Message filter: WM_HOTKEY is ours; everything else goes to ImGui.
    window_.setMessageFilter([this](HWND h, UINT m, WPARAM w, LPARAM l) {
        if (m == WM_HOTKEY) {
            return hotkeyMgr_.dispatch(w);
        }
        return ImGui_ImplWin32_WndProcHandler(h, m, w, l) != 0;
    });
    window_.setResizeHandler([this](UINT w, UINT h) {
        backend_.requestResize(w, h);
    });

    if (!backend_.init(window_.hwnd())) {
        spdlog::error("failed to init D3D11 backend");
        return 2;
    }

    // Ignore the OS-supplied nCmdShow on purpose: SW_SHOWNORMAL would
    // activate the window on startup and yank focus from the game (which
    // is exactly what the overlay flags are trying to prevent). SW_SHOWNA
    // displays at the same z-position without activating, matching how F9
    // un-hides during a session — startup behaves like un-hiding.
    (void)nCmdShow;
    window_.show(SW_SHOWNA);

    initImGui();
    registerHotkeys();

    // Wire CaptureService to game window + settings.
    capture_.setGameWindow(&gameWindow_);
    capture_.setSettings(&settings_);

    panels_.push_back(std::make_unique<panels::ConfigPanel>());
    panels_.push_back(std::make_unique<panels::CraftPanel>());
    panels_.push_back(std::make_unique<panels::MapPanel>());
    panels_.push_back(std::make_unique<panels::DepositPanel>());
    panels_.push_back(std::make_unique<panels::LogPanel>());

    panelCtx_.settings        = &settings_;
    panelCtx_.settingsRoot    = &settingsRoot_;
    panelCtx_.affixLibraryDir = &affixLibraryDir_;
    panelCtx_.logSink         = logSink_.get();
    panelCtx_.gameWindow   = &gameWindow_;
    panelCtx_.capture      = &capture_;
    panelCtx_.taskRunner   = &taskRunner_;
    panelCtx_.hotkeys      = &settings_.hotkeys;
    panelCtx_.onAppearanceChanged = [this]() { applyAppearance(); };
    panelCtx_.onRebindHotkey = [this](const std::string& id,
                                      poebot::hotkey::HotkeyBinding b) {
        return rebindHotkey(id, b);
    };

    while (!wantExit_) {
        if (!window_.pumpMessages()) {
            wantExit_ = true;
            break;
        }
        backend_.applyPendingResize();
        refreshGameWindow();

        // Re-pin to top of the z-order ~1Hz. Using SWP_NOACTIVATE so this
        // never steals focus; cheap when the window is already topmost.
        if (windowVisible_ && window_.hwnd()) {
            auto now = std::chrono::steady_clock::now();
            if (now - lastTopmostPin_ >= kTopmostPinPeriod) {
                ::SetWindowPos(window_.hwnd(), HWND_TOPMOST, 0, 0, 0, 0,
                               SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
                lastTopmostPin_ = now;
            }
        }

        // Tick capture countdown; if a coord was just written, mark dirty.
        if (capture_.tick()) {
            panelCtx_.dirty = true;
        }

        // If the user switched profile via the menu, the active profile's
        // affix library path + auto-loaded textbox content need to update
        // so the picker shows the new game's templates.
        if (settings_.activeProfile != lastActiveProfile_) {
            onActiveProfileChanged();
        }

        // Join finished tasks promptly so the thread is released.
        if (taskRunner_.state() == poebot::task::RunnerState::Finished) {
            taskRunner_.join();
        }

        renderFrame();
        saveSettingsIfDirty();
    }

    // Force-save on exit so any last edits aren't lost to the debounce window.
    if (panelCtx_.dirty) {
        poebot::config::saveLayout(settings_, settingsRoot_);
        panelCtx_.dirty = false;
    }

    taskRunner_.requestStop();
    taskRunner_.join();
    hotkeyMgr_.clear();
    shutdownImGui();
    backend_.shutdown();
    spdlog::info("Exit");
    return 0;
}

// --- private helpers --------------------------------------------------------

void App::initLogging() {
    try {
        auto msvc_sink = std::make_shared<spdlog::sinks::msvc_sink_mt>();
        auto file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
            "poe-bot.log", 5 * 1024 * 1024, 3);

        logSink_ = std::make_shared<poebot::log::ImGuiSink>(1000);
        logSink_->set_level(spdlog::level::trace);

        // Publish the GUI sink as the "active" one so task code can push
        // structured entries (highlighted hit text) via poebot::log::push
        // without threading a sink pointer through every call site.
        poebot::log::setActiveSink(logSink_.get());

        std::vector<spdlog::sink_ptr> sinks{msvc_sink, file_sink, logSink_};
        auto logger = std::make_shared<spdlog::logger>("poe-bot", sinks.begin(), sinks.end());
        logger->set_level(spdlog::level::info);
        logger->flush_on(spdlog::level::info);
        spdlog::set_default_logger(logger);
    } catch (const spdlog::spdlog_ex& e) {
        spdlog::default_logger()->warn("logger init failed: {}", e.what());
    }
}

void App::loadOrDefaultSettings() {
    settingsRoot_ = std::filesystem::current_path();

    // loadLayout handles the one-shot migration from the legacy flat layout
    // (exe/settings.json + exe/affix_libraries/) into per-profile dirs, then
    // reads the new layout. If nothing exists we get defaults.
    settings_ = poebot::config::loadLayout(settingsRoot_);

    // Ensure the layout has at least one on-disk snapshot so next launch
    // can round-trip via app.json. Cheap if everything already exists.
    poebot::config::saveLayout(settings_, settingsRoot_);

    // Set up directory state for the currently active profile. After this
    // runs, lastActiveProfile_ matches settings_.activeProfile, so the main
    // loop only reacts when the user actually picks a different profile.
    onActiveProfileChanged();

    spdlog::info("settings: root={}, active={}, profiles={}",
                 settingsRoot_.string(), settings_.activeProfile,
                 settings_.profiles.size());
}

void App::onActiveProfileChanged() {
    const std::string& name = settings_.activeProfile;
    affixLibraryDir_ = poebot::config::affixLibraryDirFor(settingsRoot_, name);

    // Each profile keeps its own craft/ and map/ library folders. The seed
    // call is a no-op once any *.txt exists, so it only fires on first run
    // (or after the user nukes the folder) — never overwrites their work.
    const auto craftDir = affixLibraryDir_ / "craft";
    const auto mapDir   = affixLibraryDir_ / "map";
    poebot::config::ensureAffixLibraryDir(craftDir);
    poebot::config::ensureAffixLibraryDir(mapDir);
    poebot::config::seedDefaultAffixLibraries(craftDir, "craft");
    poebot::config::seedDefaultAffixLibraries(mapDir,   "map");

    // Auto-bind + auto-load the active profile's library content. If no
    // library is bound (fresh profile) but one exists on disk (e.g. the
    // just-seeded "default"), pick the first one so the user starts with a
    // valid selection instead of having to manually open the dropdown.
    if (auto* prof = settings_.active()) {
        auto bindFirstAndLoad = [](std::string& bound, std::string& content,
                                   const std::filesystem::path& d) {
            if (bound.empty()) {
                auto libs = poebot::config::listAffixLibraries(d);
                if (!libs.empty()) bound = libs.front();
            }
            if (!bound.empty()) {
                auto s = poebot::config::loadAffixLibrary(d, bound);
                if (!s.empty()) content = std::move(s);
            }
        };
        bindFirstAndLoad(prof->craft.affixLibrary, prof->craft.affixes, craftDir);
        bindFirstAndLoad(prof->map.affixLibrary,   prof->map.affixes,   mapDir);
    }

    lastActiveProfile_ = name;
}

void App::saveSettingsIfDirty() {
    if (!panelCtx_.dirty) return;
    auto now = std::chrono::steady_clock::now();
    if (now - lastSaveAt_ < kSaveDebounce) return;

    if (poebot::config::saveLayout(settings_, settingsRoot_)) {
        spdlog::debug("settings saved");
    }
    panelCtx_.dirty = false;
    lastSaveAt_     = now;
}

// Per-action callbacks. Extracted out of registerHotkeys so rebindHotkey()
// can rebuild the same callback for any action without duplicating code.

void App::triggerCapture(const std::string& coordName) {
    if (capture_.captureNow(coordName)) {
        panelCtx_.dirty = true;
    }
}

void App::triggerStop() {
    if (taskRunner_.state() == poebot::task::RunnerState::Running) {
        taskRunner_.requestStop();
        spdlog::info("hotkey: stop requested");
    }
}

void App::toggleOverlay() {
    HWND h = window_.hwnd();
    if (!h) return;
    windowVisible_ = !windowVisible_;
    // SW_SHOWNA never activates → game keeps foreground when un-hiding.
    ::ShowWindow(h, windowVisible_ ? SW_SHOWNA : SW_HIDE);
    if (windowVisible_) {
        ::SetWindowPos(h, HWND_TOPMOST, 0, 0, 0, 0,
                       SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
        lastTopmostPin_ = std::chrono::steady_clock::now();
    }
    spdlog::info("hotkey: overlay {}", windowVisible_ ? "shown" : "hidden");
}

poebot::hotkey::HotkeyManager::Callback App::makeCallbackFor(const std::string& id) {
    if (id == "task.stop")      return [this]() { triggerStop(); };
    if (id == "overlay.toggle") return [this]() { toggleOverlay(); };
    // capture.* actions: the suffix matches the coord field name (orb1,
    // baseItem, invP10, …). Capturing it by value keeps the lambda
    // self-sufficient even if the registry array shifts later.
    if (id.rfind("capture.", 0) == 0) {
        std::string coord = id.substr(8);
        return [this, coord]() { triggerCapture(coord); };
    }
    return nullptr;
}

void App::registerHotkeys() {
    hotkeyMgr_.attach(window_.hwnd());

    // Backfill: ensure every action from the registry has SOME entry in
    // settings_.hotkeys. Older app.json files (pre-customizable hotkeys)
    // simply lack the new keys — defaults restore parity without touching
    // anything the user already changed.
    for (const auto& a : poebot::hotkey::allHotkeyActions()) {
        if (settings_.hotkeys.find(a.id) == settings_.hotkeys.end()) {
            settings_.hotkeys[a.id] = a.defaultBinding;
        }
    }

    using Mod = poebot::hotkey::Mod;
    int ok = 0, failed = 0;
    for (const auto& a : poebot::hotkey::allHotkeyActions()) {
        const auto& binding = settings_.hotkeys[a.id];
        if (!binding.valid()) continue;  // user explicitly unbound this action

        auto cb = makeCallbackFor(a.id);
        if (!cb) {
            spdlog::warn("hotkey: unknown action '{}' — no callback factory", a.id);
            continue;
        }

        const UINT mods = binding.modifiers | static_cast<UINT>(Mod::NoRepeat);
        int hkid = 0;
        try {
            hkid = hotkeyMgr_.registerHotkey(static_cast<Mod>(mods), binding.vk, std::move(cb));
        } catch (const std::exception& e) {
            spdlog::error("hotkey '{}' ({}) threw on register: {}",
                          a.id, binding.format(), e.what());
            ++failed;
            continue;
        }
        if (hkid) {
            hotkeyIds_[a.id] = hkid;
            ++ok;
        } else {
            spdlog::warn("hotkey '{}' ({}) unavailable — another app (AHK?) owns it",
                         a.id, binding.format());
            ++failed;
        }
    }
    spdlog::info("hotkeys registered: {}/{}", ok, ok + failed);
    if (failed > 0) {
        spdlog::warn("hotkey: {} binding(s) failed — rebind them via Settings panel "
                     "or close conflicting apps", failed);
    }
}

bool App::rebindHotkey(const std::string& id,
                       poebot::hotkey::HotkeyBinding newBinding) {
    using Mod = poebot::hotkey::Mod;

    // Reject up-front if another action in our own map already owns this
    // combo. Comparing valid() filters out the "unbind everyone" case
    // (modifiers=0, vk=0) which is allowed and never conflicts.
    if (newBinding.valid()) {
        for (const auto& [otherId, otherBinding] : settings_.hotkeys) {
            if (otherId == id) continue;
            if (otherBinding == newBinding) {
                spdlog::warn("rebind '{}' -> {}: already bound to '{}'",
                             id, newBinding.format(), otherId);
                return false;
            }
        }
    }

    // Snapshot the previous binding so we can roll back on failure.
    const auto oldBindingIt = settings_.hotkeys.find(id);
    const poebot::hotkey::HotkeyBinding oldBinding =
        (oldBindingIt != settings_.hotkeys.end())
        ? oldBindingIt->second
        : poebot::hotkey::defaultBindingFor(id);

    // Drop the existing OS-level registration. Doing this BEFORE the new
    // register attempt avoids a race where new and old briefly co-exist
    // on the same combo (RegisterHotKey would fail in that window).
    if (auto it = hotkeyIds_.find(id); it != hotkeyIds_.end()) {
        hotkeyMgr_.unregisterHotkey(it->second);
        hotkeyIds_.erase(it);
    }

    if (newBinding.valid()) {
        auto cb = makeCallbackFor(id);
        if (!cb) {
            spdlog::warn("rebind '{}': no callback factory", id);
            return false;
        }
        const UINT mods = newBinding.modifiers | static_cast<UINT>(Mod::NoRepeat);
        int hkid = 0;
        try {
            hkid = hotkeyMgr_.registerHotkey(static_cast<Mod>(mods), newBinding.vk, std::move(cb));
        } catch (const std::exception& e) {
            spdlog::error("rebind '{}' threw: {}", id, e.what());
        }
        if (!hkid) {
            // Rollback: re-register the old binding so the user isn't left
            // with a silently broken action. Best-effort — if even the old
            // one can't be put back, log and move on.
            if (oldBinding.valid()) {
                if (auto oldCb = makeCallbackFor(id)) {
                    const UINT oldMods = oldBinding.modifiers | static_cast<UINT>(Mod::NoRepeat);
                    int rb = 0;
                    try {
                        rb = hotkeyMgr_.registerHotkey(static_cast<Mod>(oldMods),
                                                       oldBinding.vk, std::move(oldCb));
                    } catch (...) {}
                    if (rb) hotkeyIds_[id] = rb;
                }
            }
            spdlog::warn("rebind '{}' -> {}: combo unavailable", id, newBinding.format());
            return false;
        }
        hotkeyIds_[id] = hkid;
    }
    // newBinding empty == "unbind". Old already dropped above; nothing else.

    settings_.hotkeys[id] = newBinding;
    panelCtx_.dirty = true;
    spdlog::info("rebind '{}': {} -> {}", id,
                 oldBinding.format(), newBinding.format());
    return true;
}

void App::refreshGameWindow() {
    auto now = std::chrono::steady_clock::now();
    if (now - lastWindowRefresh_ < kWindowRefreshPeriod) return;
    lastWindowRefresh_ = now;

    // If the handle is stale (game was closed), clear and re-find.
    if (!gameWindow_.valid()) {
        gameWindow_.clear();
        auto* prof = settings_.active();
        if (prof && !prof->windowTitlePattern.empty()) {
            gameWindow_.tryFind(prof->windowTitlePattern);
        }
    }
}

void App::initImGui() {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;  // all config lives in settings.json
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    applyAppearance();
    loadFonts();

    ImGui_ImplWin32_Init(window_.hwnd());
    ImGui_ImplDX11_Init(backend_.device(), backend_.context());
}

// ---------------------------------------------------------------------------
// macOS-inspired themes. True vibrancy/blur isn't reachable in a raw D3D11
// swap chain, so we approximate with soft grays, generous rounding, and a
// single accent blue. Spacing / rounding / borders are identical across
// Light and Dark; only the palette changes.
// ---------------------------------------------------------------------------

namespace {

ImVec4 rgba(int r, int g, int b, float a = 1.0f) {
    return ImVec4(r / 255.0f, g / 255.0f, b / 255.0f, a);
}

}  // namespace

void App::applyAppearance() {
    const bool dark = (settings_.appearance == "dark");

    applyMacBase();
    if (dark) applyMacDark();
    else      applyMacLight();

    // Keep the swap-chain clear color matched to WindowBg so resize flashes
    // don't show the opposite theme for a frame.
    const ImVec4& bg = ImGui::GetStyle().Colors[ImGuiCol_WindowBg];
    clearColor_[0] = bg.x;
    clearColor_[1] = bg.y;
    clearColor_[2] = bg.z;
    clearColor_[3] = 1.0f;

    window_.setDarkTitleBar(dark);
}

void App::applyMacBase() {
    ImGuiStyle& s = ImGui::GetStyle();

    // --- Spacing ------------------------------------------------------------
    s.WindowPadding    = ImVec2(16, 14);
    s.FramePadding     = ImVec2(12, 6);
    s.CellPadding      = ImVec2(8, 6);
    s.ItemSpacing      = ImVec2(10, 8);
    s.ItemInnerSpacing = ImVec2(8, 6);
    s.IndentSpacing    = 20.0f;
    s.ScrollbarSize    = 12.0f;
    s.GrabMinSize      = 10.0f;

    // --- Rounding (macOS-ish: larger on surfaces, medium on controls) -------
    s.WindowRounding    = 10.0f;
    s.ChildRounding     = 8.0f;
    s.FrameRounding     = 6.0f;
    s.PopupRounding     = 8.0f;
    s.ScrollbarRounding = 12.0f;
    s.GrabRounding      = 6.0f;
    s.TabRounding       = 6.0f;

    // --- Borders (intentionally absent — macOS uses color, not lines) ------
    s.WindowBorderSize = 0.0f;
    s.ChildBorderSize  = 0.0f;
    s.PopupBorderSize  = 0.0f;
    s.FrameBorderSize  = 0.0f;
    s.TabBorderSize    = 0.0f;
}

void App::applyMacDark() {
    ImGui::StyleColorsDark();  // safe baseline
    ImVec4* c = ImGui::GetStyle().Colors;

    // macOS Dark Mode system accent
    const ImVec4 accent  = rgba(10, 132, 255);
    const ImVec4 accentD = rgba(10, 132, 255, 0.78f);
    const ImVec4 accentF = rgba(64, 156, 255);

    // Surfaces
    c[ImGuiCol_WindowBg]  = rgba(30, 30, 32);
    c[ImGuiCol_ChildBg]   = rgba(36, 36, 38);
    c[ImGuiCol_PopupBg]   = rgba(44, 44, 46, 0.98f);
    c[ImGuiCol_MenuBarBg] = rgba(28, 28, 30);

    // Text
    c[ImGuiCol_Text]           = rgba(235, 235, 235);
    c[ImGuiCol_TextDisabled]   = rgba(142, 142, 147);
    c[ImGuiCol_TextSelectedBg] = rgba(10, 132, 255, 0.32f);

    // Borders / separators
    c[ImGuiCol_Border]           = rgba(56, 56, 58, 0.6f);
    c[ImGuiCol_BorderShadow]     = ImVec4(0, 0, 0, 0);
    c[ImGuiCol_Separator]        = rgba(56, 56, 58, 0.8f);
    c[ImGuiCol_SeparatorHovered] = rgba(110, 110, 115);
    c[ImGuiCol_SeparatorActive]  = rgba(140, 140, 145);

    // Input frames
    c[ImGuiCol_FrameBg]        = rgba(58, 58, 60);
    c[ImGuiCol_FrameBgHovered] = rgba(72, 72, 75);
    c[ImGuiCol_FrameBgActive]  = rgba(88, 88, 92);

    // Buttons
    c[ImGuiCol_Button]        = rgba(58, 58, 60);
    c[ImGuiCol_ButtonHovered] = rgba(72, 72, 75);
    c[ImGuiCol_ButtonActive]  = accent;

    // Headers / selectables
    c[ImGuiCol_Header]        = accentD;
    c[ImGuiCol_HeaderHovered] = accentF;
    c[ImGuiCol_HeaderActive]  = accent;

    // Checkboxes / sliders
    c[ImGuiCol_CheckMark]        = accent;
    c[ImGuiCol_SliderGrab]       = accent;
    c[ImGuiCol_SliderGrabActive] = accentF;

    // Scrollbar (macOS: thin, translucent, no track)
    c[ImGuiCol_ScrollbarBg]          = ImVec4(0, 0, 0, 0);
    c[ImGuiCol_ScrollbarGrab]        = rgba(110, 110, 115, 0.50f);
    c[ImGuiCol_ScrollbarGrabHovered] = rgba(140, 140, 145, 0.75f);
    c[ImGuiCol_ScrollbarGrabActive]  = rgba(170, 170, 175, 0.90f);

    // Tabs (docking)
    c[ImGuiCol_Tab]                = rgba(44, 44, 46);
    c[ImGuiCol_TabHovered]         = rgba(58, 58, 60);
    c[ImGuiCol_TabActive]          = accent;
    c[ImGuiCol_TabUnfocused]       = rgba(36, 36, 38);
    c[ImGuiCol_TabUnfocusedActive] = rgba(58, 58, 60);

    // Title bar (rarely visible — host window has no ImGui title)
    c[ImGuiCol_TitleBg]          = rgba(28, 28, 30);
    c[ImGuiCol_TitleBgActive]    = rgba(28, 28, 30);
    c[ImGuiCol_TitleBgCollapsed] = rgba(28, 28, 30, 0.6f);

    // Resize grip
    c[ImGuiCol_ResizeGrip]        = rgba(120, 120, 125, 0.25f);
    c[ImGuiCol_ResizeGripHovered] = rgba(10, 132, 255, 0.55f);
    c[ImGuiCol_ResizeGripActive]  = rgba(10, 132, 255, 0.85f);

    // Nav / drag-drop
    c[ImGuiCol_NavHighlight]   = accent;
    c[ImGuiCol_DragDropTarget] = accent;
}

void App::applyMacLight() {
    ImGui::StyleColorsLight();  // baseline
    ImVec4* c = ImGui::GetStyle().Colors;

    // macOS Light Mode system accent (slightly deeper blue than Dark's)
    const ImVec4 accent  = rgba(0, 122, 255);
    const ImVec4 accentD = rgba(0, 122, 255, 0.82f);
    const ImVec4 accentF = rgba(51, 149, 255);

    // Surfaces: Aqua uses a cool white canvas with white panels on top.
    c[ImGuiCol_WindowBg]  = rgba(245, 245, 247);
    c[ImGuiCol_ChildBg]   = rgba(255, 255, 255);
    c[ImGuiCol_PopupBg]   = rgba(250, 250, 252, 0.98f);
    c[ImGuiCol_MenuBarBg] = rgba(236, 236, 238);

    // Text (macOS never uses pure black; ~85% on white reads as "dark gray")
    c[ImGuiCol_Text]           = rgba(28, 28, 30);
    c[ImGuiCol_TextDisabled]   = rgba(142, 142, 147);
    c[ImGuiCol_TextSelectedBg] = rgba(0, 122, 255, 0.28f);

    // Borders / separators
    c[ImGuiCol_Border]           = rgba(210, 210, 215, 0.60f);
    c[ImGuiCol_BorderShadow]     = ImVec4(0, 0, 0, 0);
    c[ImGuiCol_Separator]        = rgba(210, 210, 215, 0.90f);
    c[ImGuiCol_SeparatorHovered] = rgba(160, 160, 165);
    c[ImGuiCol_SeparatorActive]  = rgba(120, 120, 125);

    // Input frames (slightly darker than ChildBg so they read as wells)
    c[ImGuiCol_FrameBg]        = rgba(232, 232, 237);
    c[ImGuiCol_FrameBgHovered] = rgba(220, 220, 225);
    c[ImGuiCol_FrameBgActive]  = rgba(208, 208, 213);

    // Buttons
    c[ImGuiCol_Button]        = rgba(232, 232, 237);
    c[ImGuiCol_ButtonHovered] = rgba(220, 220, 225);
    c[ImGuiCol_ButtonActive]  = accent;

    // Headers / selectables
    c[ImGuiCol_Header]        = accentD;
    c[ImGuiCol_HeaderHovered] = accentF;
    c[ImGuiCol_HeaderActive]  = accent;

    // Checkboxes / sliders
    c[ImGuiCol_CheckMark]        = accent;
    c[ImGuiCol_SliderGrab]       = accent;
    c[ImGuiCol_SliderGrabActive] = accentF;

    // Scrollbar: dark thumb on light background, translucent
    c[ImGuiCol_ScrollbarBg]          = ImVec4(0, 0, 0, 0);
    c[ImGuiCol_ScrollbarGrab]        = rgba(0, 0, 0, 0.28f);
    c[ImGuiCol_ScrollbarGrabHovered] = rgba(0, 0, 0, 0.45f);
    c[ImGuiCol_ScrollbarGrabActive]  = rgba(0, 0, 0, 0.60f);

    // Tabs (docking)
    c[ImGuiCol_Tab]                = rgba(232, 232, 237);
    c[ImGuiCol_TabHovered]         = rgba(220, 220, 225);
    c[ImGuiCol_TabActive]          = accent;
    c[ImGuiCol_TabUnfocused]       = rgba(240, 240, 242);
    c[ImGuiCol_TabUnfocusedActive] = rgba(220, 220, 225);

    // Title bar
    c[ImGuiCol_TitleBg]          = rgba(246, 246, 248);
    c[ImGuiCol_TitleBgActive]    = rgba(246, 246, 248);
    c[ImGuiCol_TitleBgCollapsed] = rgba(246, 246, 248, 0.70f);

    // Resize grip
    c[ImGuiCol_ResizeGrip]        = rgba(0, 0, 0, 0.15f);
    c[ImGuiCol_ResizeGripHovered] = rgba(0, 122, 255, 0.55f);
    c[ImGuiCol_ResizeGripActive]  = rgba(0, 122, 255, 0.85f);

    // Nav / drag-drop
    c[ImGuiCol_NavHighlight]   = accent;
    c[ImGuiCol_DragDropTarget] = accent;
}

void App::loadFonts() {
    // Use the Windows system default stack: Segoe UI for Latin glyphs, with
    // Microsoft YaHei merged on top for CJK coverage. Both ship with every
    // modern Windows install and are what Explorer / Settings themselves use.
    ImGuiIO& io = ImGui::GetIO();
    constexpr float kSize = 18.0f;

    auto exists = [](const char* p) {
        std::error_code ec;
        return std::filesystem::exists(p, ec);
    };

    const char* latinPath = "C:\\Windows\\Fonts\\segoeui.ttf";
    const char* cjkPath   = "C:\\Windows\\Fonts\\msyh.ttc";

    ImFontConfig base;
    base.OversampleH = 2;
    base.OversampleV = 1;

    if (exists(latinPath)) {
        io.Fonts->AddFontFromFileTTF(latinPath, kSize, &base);
    } else {
        io.Fonts->AddFontDefault();
    }

    if (exists(cjkPath)) {
        ImFontConfig merge = base;
        merge.MergeMode = true;
        io.Fonts->AddFontFromFileTTF(cjkPath, kSize, &merge,
                                     io.Fonts->GetGlyphRangesChineseSimplifiedCommon());
        spdlog::info("font: Segoe UI + Microsoft YaHei (merged)");
    } else {
        spdlog::warn("font: msyh.ttc missing — Chinese will render as '?'");
    }
}

void App::shutdownImGui() {
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
}

void App::renderFrame() {
    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    panels::renderMainLayout(panels_, panelCtx_);

    ImGui::Render();
    backend_.beginFrame(clearColor_);
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    backend_.endFrame();
}

}  // namespace poebot::gui
