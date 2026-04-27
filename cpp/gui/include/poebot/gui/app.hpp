#pragma once
#include <poebot/gui/capture_service.hpp>
#include <poebot/gui/d3d11_backend.hpp>
#include <poebot/gui/main_window.hpp>
#include <poebot/gui/panel.hpp>

#include <poebot/hotkey/binding.hpp>
#include <poebot/hotkey/hotkey_manager.hpp>
#include <poebot/task/task_runner.hpp>
#include <poebot/win/window.hpp>

#include <chrono>
#include <filesystem>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace poebot::gui {

// Application orchestrator. Owns the main window, the D3D11 backend, the
// ImGui context, the Settings object, and the panel list. Entry point is
// run(), which drains messages and renders frames until WM_QUIT.
class App {
public:
    int run(HINSTANCE hInstance, int nCmdShow);

private:
    void initLogging();
    void initImGui();
    void loadFonts();
    void shutdownImGui();

    // Theme selection: read settings_.appearance, pick Light or Dark palette,
    // update clear-color cache, and nudge DWM to repaint the title bar.
    // Safe to call any time after initImGui().
    void applyAppearance();

    // Apply the shared spacing/rounding/border scaffolding. Colors are left
    // to the caller (applyMacLight / applyMacDark fill them in).
    void applyMacBase();
    void applyMacLight();
    void applyMacDark();

    void loadOrDefaultSettings();
    void saveSettingsIfDirty();

    // React to the user picking a different profile: retarget the affix
    // library directory and auto-load the new profile's bound library. Also
    // called once at startup to seed lastActiveProfile_.
    void onActiveProfileChanged();

    void registerHotkeys();
    void refreshGameWindow();

    // Atomic rebind: drops the old registration for `id`, tries to register
    // the new combo, and rolls back to the previous binding if registration
    // fails (system-wide conflict, e.g. another app owns the combo). Refuses
    // up-front if `newBinding` is already used by another action in our own
    // map. Returns true on success, false on any conflict / failure (the
    // caller renders an inline message; settings remain consistent).
    bool rebindHotkey(const std::string& id,
                      poebot::hotkey::HotkeyBinding newBinding);

    // Per-action callback bodies — extracted from the old registerHotkeys
    // so registerHotkeys and rebindHotkey can rebuild the same callback
    // for any action without duplicating the work.
    void triggerCapture(const std::string& coordName);
    void triggerStop();
    void toggleOverlay();

    poebot::hotkey::HotkeyManager::Callback makeCallbackFor(const std::string& id);

    void renderFrame();

    MainWindow                              window_;
    D3D11Backend                            backend_;
    poebot::config::Settings                settings_{};
    std::shared_ptr<poebot::log::ImGuiSink> logSink_;
    // Root of the split settings layout (<exe>/). Contains app.json and a
    // subfolder per game profile with its own settings.json + libraries.
    std::filesystem::path                   settingsRoot_;
    // Active profile's affix_libraries folder. Recomputed each frame from
    // settings_.activeProfile so switching profiles retargets the picker.
    std::filesystem::path                   affixLibraryDir_;
    // Tracks which profile affixLibraryDir_ was computed for; changing means
    // we need to auto-load the new profile's libraries into its textboxes.
    std::string                             lastActiveProfile_;

    std::vector<std::unique_ptr<Panel>> panels_;
    PanelContext                        panelCtx_{};

    // Phase 3.1
    poebot::hotkey::HotkeyManager           hotkeyMgr_;
    // action id -> HotkeyManager registration id, kept in sync with the
    // bindings in settings_.hotkeys. rebindHotkey() drains and rebuilds
    // a single entry; clear()/registerHotkeys() rebuild the whole map.
    std::unordered_map<std::string, int>    hotkeyIds_;
    poebot::win::GameWindow                 gameWindow_;
    CaptureService                          capture_;

    // Phase 3.2
    poebot::task::TaskRunner                taskRunner_;

    bool                                        wantExit_    = false;
    std::chrono::steady_clock::time_point       lastSaveAt_{};
    std::chrono::steady_clock::time_point       lastWindowRefresh_{};

    // Overlay state. F9 toggles `windowVisible_` (ShowWindow SW_HIDE /
    // SW_SHOWNA — never SW_SHOW, which would activate and steal focus from
    // the game). `lastTopmostPin_` throttles the periodic re-pin that keeps
    // us above any other topmost windows that might have stacked on top
    // (e.g. RGB-control utilities, password managers); without this nudge
    // those can occasionally cover us mid-session.
    bool                                        windowVisible_ = true;
    std::chrono::steady_clock::time_point       lastTopmostPin_{};

    // Cached D3D11 clear color; kept in sync with the active theme so resizes
    // don't flash the opposite color behind the ImGui surface.
    float clearColor_[4] = {0.96f, 0.96f, 0.97f, 1.0f};
};

}  // namespace poebot::gui
