#pragma once
#include <poebot/gui/capture_service.hpp>
#include <poebot/gui/d3d11_backend.hpp>
#include <poebot/gui/main_window.hpp>
#include <poebot/gui/panel.hpp>

#include <poebot/hotkey/hotkey_manager.hpp>
#include <poebot/task/task_runner.hpp>
#include <poebot/win/window.hpp>

#include <chrono>
#include <filesystem>
#include <memory>
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
    void applyMacStyle();
    void shutdownImGui();

    void loadOrDefaultSettings();
    void saveSettingsIfDirty();

    void registerHotkeys();
    void refreshGameWindow();

    void renderFrame();

    MainWindow                              window_;
    D3D11Backend                            backend_;
    poebot::config::Settings                settings_{};
    std::shared_ptr<poebot::log::ImGuiSink> logSink_;
    std::filesystem::path                   settingsPath_;

    std::vector<std::unique_ptr<Panel>> panels_;
    PanelContext                        panelCtx_{};

    // Phase 3.1
    poebot::hotkey::HotkeyManager           hotkeyMgr_;
    poebot::win::GameWindow                 gameWindow_;
    CaptureService                          capture_;

    // Phase 3.2
    poebot::task::TaskRunner                taskRunner_;

    bool                                        wantExit_    = false;
    std::chrono::steady_clock::time_point       lastSaveAt_{};
    std::chrono::steady_clock::time_point       lastWindowRefresh_{};
};

}  // namespace poebot::gui
