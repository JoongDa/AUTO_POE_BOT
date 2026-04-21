#include <poebot/gui/app.hpp>

#include <poebot/gui/panels/config_panel.hpp>
#include <poebot/gui/panels/craft_panel.hpp>
#include <poebot/gui/panels/deposit_panel.hpp>
#include <poebot/gui/panels/log_panel.hpp>
#include <poebot/gui/panels/main_layout.hpp>
#include <poebot/gui/panels/map_panel.hpp>

#include <poebot/config/settings_io.hpp>
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
}

int App::run(HINSTANCE hInstance, int nCmdShow) {
    initLogging();
    spdlog::info("{} v{} starting", poebot::kAppName, poebot::kAppVersion);

    loadOrDefaultSettings();

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

    window_.show(nCmdShow);

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

    panelCtx_.settings     = &settings_;
    panelCtx_.settingsPath = &settingsPath_;
    panelCtx_.logSink      = logSink_.get();
    panelCtx_.gameWindow   = &gameWindow_;
    panelCtx_.capture      = &capture_;
    panelCtx_.taskRunner   = &taskRunner_;

    while (!wantExit_) {
        if (!window_.pumpMessages()) {
            wantExit_ = true;
            break;
        }
        backend_.applyPendingResize();
        refreshGameWindow();

        // Tick capture countdown; if a coord was just written, mark dirty.
        if (capture_.tick()) {
            panelCtx_.dirty = true;
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
        poebot::config::saveSettings(settings_, settingsPath_);
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
    settingsPath_ = std::filesystem::current_path() / "settings.json";
    settings_     = poebot::config::loadSettings(settingsPath_);

    if (!std::filesystem::exists(settingsPath_)) {
        poebot::config::saveSettings(settings_, settingsPath_);
    }

    spdlog::info("settings: path={}, active={}, profiles={}",
                 settingsPath_.string(), settings_.activeProfile, settings_.profiles.size());
}

void App::saveSettingsIfDirty() {
    if (!panelCtx_.dirty) return;
    auto now = std::chrono::steady_clock::now();
    if (now - lastSaveAt_ < kSaveDebounce) return;

    if (poebot::config::saveSettings(settings_, settingsPath_)) {
        spdlog::debug("settings saved");
    }
    panelCtx_.dirty = false;
    lastSaveAt_     = now;
}

void App::registerHotkeys() {
    hotkeyMgr_.attach(window_.hwnd());

    // Per-coord capture hotkeys (mirrors the AHK original).
    //   Alt+1/2/3       → orb1/2/3
    //   Alt+0/8/9       → baseItem / p01Item / p10Item
    //   Alt+4/5/6       → invBase / invP01 / invP10
    using Mod = poebot::hotkey::Mod;
    struct Binding { UINT vk; const char* name; const char* label; };
    static constexpr Binding kBindings[] = {
        {'1', "orb1",     "Alt+1"},
        {'2', "orb2",     "Alt+2"},
        {'3', "orb3",     "Alt+3"},
        {'0', "baseItem", "Alt+0"},
        {'8', "p01Item",  "Alt+8"},
        {'9', "p10Item",  "Alt+9"},
        {'4', "invBase",  "Alt+4"},
        {'5', "invP01",   "Alt+5"},
        {'6', "invP10",   "Alt+6"},
    };

    int ok = 0;
    int failed = 0;
    for (const auto& b : kBindings) {
        // Capture `b.name` by pointer — it points into the static kBindings
        // table, outlives the lambda, and keeps the lambda small so it stays
        // inside std::function's small-object buffer (no heap allocation).
        const char* name = b.name;
        int id = 0;
        try {
            id = hotkeyMgr_.registerHotkey(Mod::Alt | Mod::NoRepeat, b.vk,
                                           [this, name]() { capture_.startCapture(name); });
        } catch (const std::exception& e) {
            spdlog::error("registerHotkey({}) threw: {}", b.label, e.what());
            ++failed;
            continue;
        } catch (...) {
            spdlog::error("registerHotkey({}) threw unknown", b.label);
            ++failed;
            continue;
        }
        if (id) {
            ++ok;
        } else {
            ++failed;
            spdlog::warn("hotkey {} not available — some other app (AHK Bot.ahk?) owns it", b.label);
        }
    }
    spdlog::info("capture hotkeys registered: {}/{}", ok, static_cast<int>(std::size(kBindings)));
    if (failed > 0) {
        spdlog::warn("hotkey: {} binding(s) failed — close any running AHK bot and restart",
                     failed);
    }

    // Task stop hotkey.
    try {
        hotkeyMgr_.registerHotkey(Mod::NoRepeat, VK_END, [this]() {
            if (taskRunner_.state() == poebot::task::RunnerState::Running) {
                taskRunner_.requestStop();
                spdlog::info("End pressed — stop requested");
            }
        });
    } catch (const std::exception& e) {
        spdlog::error("registerHotkey(End) threw: {}", e.what());
    }
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
    ImGui::StyleColorsDark();

    loadFonts();

    ImGui_ImplWin32_Init(window_.hwnd());
    ImGui_ImplDX11_Init(backend_.device(), backend_.context());
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

    panels::renderMainLayout(panels_, panelCtx_, wantExit_);

    ImGui::Render();
    const float clear[4] = {0.10f, 0.10f, 0.12f, 1.0f};
    backend_.beginFrame(clear);
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    backend_.endFrame();
}

}  // namespace poebot::gui
