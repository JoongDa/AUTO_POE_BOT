#pragma once
#include <poebot/hotkey/binding.hpp>

#include <atomic>

// Modal-time keyboard grabber. Used by the Settings panel's Rebind dialog
// to capture a single key combination *without* relying on Win32 focus —
// our window has WS_EX_NOACTIVATE so it never gets keyboard input through
// the normal message route. WH_KEYBOARD_LL bypasses the focused-window
// requirement and lets us see all keystrokes system-wide while the hook
// is installed.
//
// Lifecycle: start() once when the modal opens, stop() when it closes.
// Polled from the UI thread each frame via committed() / canceled() /
// previewModifiers() to render the live preview and detect completion.
//
// Single-instance: a process can only have one capture active at a time
// (the underlying hook proc lives in a static slot). This is fine for our
// usage — the modal is exclusive.
namespace poebot::gui {

class HotkeyCapture {
public:
    HotkeyCapture() = default;
    ~HotkeyCapture();
    HotkeyCapture(const HotkeyCapture&)            = delete;
    HotkeyCapture& operator=(const HotkeyCapture&) = delete;

    // Install the low-level keyboard hook. Resets all state, so calling
    // start() while already capturing is equivalent to "start over".
    void start();

    // Uninstall the hook. Idempotent; safe to call from the destructor.
    void stop();

    bool active() const noexcept { return active_.load(); }

    // True once a non-modifier keydown has been captured. Read result() to
    // get the binding. Stays true until the next start() resets state.
    bool committed() const noexcept { return capturedVk_.load() != 0; }

    // True if the user hit Escape during capture. Stays true until reset.
    bool canceled() const noexcept { return canceled_.load(); }

    // Live modifier state — repaints every frame so the modal can show a
    // ghost preview ("Ctrl+Shift+...") while the user is still composing.
    UINT previewModifiers() const noexcept { return previewMods_.load(); }

    // Final captured binding. Only meaningful after committed() returns true.
    poebot::hotkey::HotkeyBinding result() const noexcept;

private:
    // Hook procedure runs on the system input thread, not ours. Made a
    // static member so it can poke at private atomics through the
    // process-wide owner pointer that start() installs.
    static LRESULT CALLBACK HookProc(int code, WPARAM wParam, LPARAM lParam);

    // All shared state goes through std::atomic so the UI thread can read
    // it each frame without a lock.
    std::atomic<bool> active_      {false};
    std::atomic<bool> canceled_    {false};
    std::atomic<UINT> previewMods_ {0};
    std::atomic<UINT> capturedVk_  {0};
    std::atomic<UINT> capturedMods_{0};
};

}  // namespace poebot::gui
