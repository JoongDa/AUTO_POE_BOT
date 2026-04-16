#pragma once
#include <poebot/gui/d3d11_backend.hpp>
#include <poebot/gui/main_window.hpp>
#include <poebot/gui/panel.hpp>

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
    void shutdownImGui();

    void loadOrDefaultSettings();
    void saveSettingsIfDirty();

    void renderFrame();

    MainWindow                              window_;
    D3D11Backend                            backend_;
    poebot::config::Settings                settings_{};
    std::shared_ptr<poebot::log::ImGuiSink> logSink_;
    std::filesystem::path                   settingsPath_;

    std::vector<std::unique_ptr<Panel>> panels_;
    PanelContext                        panelCtx_{};

    bool                                        wantExit_    = false;
    std::chrono::steady_clock::time_point       lastSaveAt_{};
};

}  // namespace poebot::gui
