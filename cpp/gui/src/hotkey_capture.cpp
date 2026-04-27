#include <poebot/gui/hotkey_capture.hpp>

#include <spdlog/spdlog.h>

#include <windows.h>

namespace poebot::gui {

namespace {

// Process-wide hook handle + the instance owning it. Only one HotkeyCapture
// can be active at a time — the hook proc has to be a free function with no
// closure, so it dispatches via this static slot. The Settings panel's
// rebind modal is exclusive, which makes the single-slot constraint a non-
// issue in practice.
HHOOK          g_hook = nullptr;
HotkeyCapture* g_owner = nullptr;

bool isModifierVk(UINT vk) noexcept {
    switch (vk) {
        case VK_SHIFT:   case VK_LSHIFT:   case VK_RSHIFT:
        case VK_CONTROL: case VK_LCONTROL: case VK_RCONTROL:
        case VK_MENU:    case VK_LMENU:    case VK_RMENU:    // Alt
        case VK_LWIN:    case VK_RWIN:
        case VK_CAPITAL: case VK_NUMLOCK:  case VK_SCROLL:   // toggles — not bindable
            return true;
        default:
            return false;
    }
}

UINT readCurrentModifiers() noexcept {
    UINT m = 0;
    // The high-bit-set check is idiomatic for GetAsyncKeyState — bit 0x8000
    // is "currently down". MOD_* are the bitfield values used by RegisterHotKey.
    if (::GetAsyncKeyState(VK_CONTROL) & 0x8000) m |= MOD_CONTROL;
    if (::GetAsyncKeyState(VK_SHIFT)   & 0x8000) m |= MOD_SHIFT;
    if (::GetAsyncKeyState(VK_MENU)    & 0x8000) m |= MOD_ALT;
    if ((::GetAsyncKeyState(VK_LWIN) | ::GetAsyncKeyState(VK_RWIN)) & 0x8000) m |= MOD_WIN;
    return m;
}

}  // namespace

LRESULT CALLBACK HotkeyCapture::HookProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode != HC_ACTION || !g_owner) {
        return ::CallNextHookEx(nullptr, nCode, wParam, lParam);
    }
    auto* kb = reinterpret_cast<KBDLLHOOKSTRUCT*>(lParam);

    // We watch both KEYDOWN and SYSKEYDOWN — the latter fires when Alt is
    // held, and the bot's defaults are heavy on Alt+number combos.
    const bool isDown = (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN);
    const bool isUp   = (wParam == WM_KEYUP   || wParam == WM_SYSKEYUP);

    if (isDown) {
        const UINT vk   = kb->vkCode;
        const UINT mods = readCurrentModifiers();

        // Live preview always reflects current modifier state.
        g_owner->previewMods_.store(mods);

        if (vk == VK_ESCAPE) {
            // Cancel — swallow so the game doesn't see ESC either.
            g_owner->canceled_.store(true);
            return 1;
        }
        if (isModifierVk(vk)) {
            // Keep modifier-only presses out of the foreground app while the
            // capture is active so the user can compose freely (e.g. holding
            // Alt won't pop the game's menu). Modifiers don't commit.
            return 1;
        }

        // Non-modifier key: capture and swallow. The owning UI sees this on
        // the next frame via committed() and tears the hook down.
        // Order matters — write modifiers BEFORE vk, because UI reads vk
        // first as the "is this committed?" flag.
        g_owner->capturedMods_.store(mods);
        g_owner->capturedVk_.store(vk);
        return 1;
    }

    if (isUp) {
        // Release events update the preview but never commit.
        g_owner->previewMods_.store(readCurrentModifiers());
        // Don't swallow — releases need to reach the OS or the next app
        // ends up with a "stuck" modifier (e.g. caps-lock styled).
    }

    return ::CallNextHookEx(nullptr, nCode, wParam, lParam);
}

HotkeyCapture::~HotkeyCapture() {
    stop();
}

void HotkeyCapture::start() {
    stop();  // ensure any prior session is fully torn down

    // Reset all atomic state so the UI sees a clean slate.
    active_.store(true);
    canceled_.store(false);
    previewMods_.store(0);
    capturedVk_.store(0);
    capturedMods_.store(0);

    g_owner = this;
    g_hook  = ::SetWindowsHookExW(WH_KEYBOARD_LL, &HotkeyCapture::HookProc,
                                  ::GetModuleHandleW(nullptr), 0);
    if (!g_hook) {
        spdlog::error("HotkeyCapture: SetWindowsHookExW failed: {}", ::GetLastError());
        active_.store(false);
        g_owner = nullptr;
    }
}

void HotkeyCapture::stop() {
    if (g_hook) {
        ::UnhookWindowsHookEx(g_hook);
        g_hook = nullptr;
    }
    if (g_owner == this) g_owner = nullptr;
    active_.store(false);
}

poebot::hotkey::HotkeyBinding HotkeyCapture::result() const noexcept {
    poebot::hotkey::HotkeyBinding b;
    b.modifiers = capturedMods_.load();
    b.vk        = capturedVk_.load();
    return b;
}

}  // namespace poebot::gui
