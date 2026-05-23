# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build

Windows-only. Requires MSVC 2022, CMake 3.25+, Ninja, and vcpkg at `C:\vcpkg`.

```powershell
# Enter a shell with MSVC + CMake + Ninja on PATH
cpp\scripts\dev-shell.bat

# Configure (once)
cmake --preset msvc -S cpp

# Build
cmake --build cpp/build --config Debug
cmake --build cpp/build --config Release

# Binary lands at cpp/build/bin/{Debug,Release}/poe-bot.exe
```

Triplet is `x64-windows-static-md` (static libs, dynamic CRT). vcpkg manifest mode — dependencies install automatically on first configure.

No test framework is wired up yet. No lint CI. Format with `clang-format -i` (Google-based, 4-space indent, 110 col limit — see `cpp/.clang-format`).

## Architecture

Two CMake targets: `poebot-core` (static lib, headless) and `poe-bot` (Win32 GUI exe).

### core (`cpp/core/`)

All game logic, no UI dependencies. Namespace root: `poebot::`.

| Module | Namespace | Purpose |
|--------|-----------|---------|
| config/ | `poebot::config` | Settings, GameProfile, affix libraries. Split layout: `app.json` (top-level) + `<profile>/settings.json` per game version |
| task/ | `poebot::task` | Automation tasks (Craft, Map, Deposit). Each extends `Task` base class |
| input/ | `poebot::input` | Mouse/keyboard via SendInput, Bezier cursor motion, Gaussian-randomized sleep |
| item/ | `poebot::item` | `AffixMatcher` — compiled regex for pipe-separated affix patterns |
| hotkey/ | `poebot::hotkey` | System-wide hotkeys via RegisterHotKey. `HotkeyBinding` serializes to `"Alt+1"` strings |
| i18n/ | `poebot::i18n` | `tr(key)` lookup, zh/en |
| win/ | `poebot::win` | `GameWindow` — HWND finder + client/screen coordinate conversion |
| sys/ | `poebot::sys` | Clipboard, encoding (UTF-8/UTF-16), screen capture |
| log/ | `poebot::log` | `ImGuiSink` — spdlog sink backed by a ring buffer for the log panel |
| vision/ | `poebot::vision` | Template matching (placeholder for future OpenCV/YOLO) |

### gui (`cpp/gui/`)

Dear ImGui over D3D11. Namespace: `poebot::gui`.

- **App** — orchestrator. Owns MainWindow, D3D11Backend, Settings, TaskRunner, HotkeyManager, all panels. `run()` is the message loop.
- **Panel** — abstract base. Each panel gets a `PanelContext&` (shared mutable state: settings pointer, task runner, capture service, dirty flag). Panels: ConfigPanel, CraftPanel, MapPanel, DepositPanel, LogPanel.
- **MainLayout** — sidebar nav + content area. Writes `ctx.activePanel`.
- **D3D11Backend** — RAII device/swapchain/RTV. Falls back to WARP in VMs.
- **CaptureService** — countdown-based coordinate capture workflow (hotkey → 3s delay → read cursor → write to profile).

## Key design patterns

**Strong-typed coordinates** (`coords.hpp`): `ClientPoint`, `ScreenPoint`, `ImagePoint` are distinct types. You cannot mix them — conversions go through `GameWindow::clientToScreen()` / `screenToClient()`. `ClientOffset` for deltas; `isUnset()` checks the `(0,0)` sentinel.

**Task threading**: `TaskRunner` owns one worker thread. `Task::run()` receives `atomic<bool>& stopRequested` and must poll it at every safe point. Use `interruptibleSleep()` / `gaussSleep()` — never raw `sleep_for`. Tasks copy all params at construction (snapshot) so the UI thread can keep rendering.

**Human-like input**: `bezierMoveTo()` for curved mouse paths, `gaussSleep()` / `gaussInt()` for randomized timing. These must stay human-like — do not "optimize" into linear moves or fixed delays.

**Settings persistence**: Config is GUI-only (never hand-edited). JSON via nlohmann::json. All file writes use atomic temp-file + rename (`atomicReplaceFile`). Schema has `"version": N` for migration. Split layout puts per-profile data in subdirectories.

**Affix libraries**: Reusable `.txt` files of pipe-separated regex patterns, stored per-profile under `<root>/<profile>/affix_libraries/{craft,map}/`. Independent from the active textbox content.

**i18n**: All UI strings go through `tr("key")`. Keys are English identifiers; zh translations are in the i18n table. Missing keys fall through to English, then to the key itself.

**Hotkey system**: Actions registered in `allHotkeyActions()` — adding an entry there surfaces it in UI, defaults, persistence, and registration automatically. Bindings serialize as `"Alt+1"` strings in JSON.

## Naming conventions

- Types: `PascalCase` (`CraftTask`, `GameProfile`)
- Functions/methods: `camelCase` (`tryStartCraft`, `findCoordByName`)
- Member variables: trailing underscore (`hwnd_`, `settings_`)
- Namespaces: `lowercase` (`poebot::config`)
- Constants: `kPascalCase` (`kPoll`)

## Multi-profile model

Two built-in profiles: `poe1` and `poe2`. `Settings::activeProfile` selects one. Each profile has its own coordinates, task settings, stats, and affix library directory. The window title pattern differentiates POE2 from POE1 (probe longer pattern first — "Path of Exile 2" contains "Path of Exile").

## Future direction

Vision pipeline planned: traditional CV first (template matching, HSV, OCR via OpenCV), YOLO only where necessary (train in Python/Ultralytics, export ONNX, infer via ONNX Runtime with DirectML EP). The `vision/` and `ImagePoint` type are scaffolding for this.
