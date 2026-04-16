#pragma once
#include <windows.h>

#include <functional>
#include <unordered_map>

namespace poebot::hotkey {

// Modifier flags matching Win32 MOD_* constants. Safe to OR together.
enum class Mod : UINT {
    None     = 0,
    Alt      = MOD_ALT,
    Ctrl     = MOD_CONTROL,
    Shift    = MOD_SHIFT,
    Win      = MOD_WIN,
    NoRepeat = MOD_NOREPEAT,  // ignore key-repeat while held
};

constexpr Mod  operator|(Mod a, Mod b) noexcept { return static_cast<Mod>(static_cast<UINT>(a) | static_cast<UINT>(b)); }
constexpr Mod  operator&(Mod a, Mod b) noexcept { return static_cast<Mod>(static_cast<UINT>(a) & static_cast<UINT>(b)); }
constexpr bool any(Mod m) noexcept { return static_cast<UINT>(m) != 0; }

// Thin wrapper around RegisterHotKey / UnregisterHotKey. Routes WM_HOTKEY
// events to user-supplied callbacks via dispatch().
class HotkeyManager {
public:
    using Callback = std::function<void()>;

    HotkeyManager() = default;
    ~HotkeyManager() { clear(); }

    HotkeyManager(const HotkeyManager&) = delete;
    HotkeyManager& operator=(const HotkeyManager&) = delete;

    // Set the HWND that will receive WM_HOTKEY notifications.
    void attach(HWND owner) noexcept { hwnd_ = owner; }

    // Register a system-wide hotkey. Returns the internal id (>0) or 0 on
    // failure (e.g. another app owns that combination).
    int registerHotkey(Mod modifiers, UINT vk, Callback cb);

    void unregisterHotkey(int id);
    void clear();

    // Dispatches a WM_HOTKEY (wparam = id). Returns true if the id was ours.
    bool dispatch(WPARAM id);

private:
    HWND                                   hwnd_    = nullptr;
    int                                    nextId_  = 1;
    std::unordered_map<int, Callback>      callbacks_;
};

}  // namespace poebot::hotkey
