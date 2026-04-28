#include <poebot/gui/hotkey_capture.hpp>

#include <spdlog/spdlog.h>

#include <atomic>
#include <windows.h>

namespace poebot::gui {

namespace {

// Process-wide hook handle + the instance owning it. Only one HotkeyCapture
// can be active at a time — the hook proc has to be a free function with no
// closure, so it dispatches via this static slot. The Settings panel's
// rebind modal is exclusive, which makes the single-slot constraint a non-
// issue in practice.
HHOOK          g_hook  = nullptr;
HotkeyCapture* g_owner = nullptr;

// Self-tracked modifier state, walked from the keydown/up events the hook
// sees. We can't trust GetAsyncKeyState inside a WH_KEYBOARD_LL hook for
// two reasons:
//
//   1. The async key state for the *current* event hasn't been committed
//      yet when the hook fires — querying it for the just-pressed key
//      returns "released".
//   2. Returning a non-zero value from the hook (which we do, to keep
//      modifier presses from leaking to the foreground game) prevents
//      Windows from updating its tracked state at all, so subsequent
//      GetAsyncKeyState lookups for that modifier come back zero too.
//
// Walking modifier transitions ourselves makes captured combos like
// "Alt+5" reliable — without it we'd see only "5".
std::atomic<UINT> g_modState{0};

// Map a virtual-key code to the MOD_* bit it represents, or 0 if the key
// isn't a modifier we care about for binding.
UINT modBitForVk(UINT vk) noexcept {
    switch (vk) {
        case VK_CONTROL: case VK_LCONTROL: case VK_RCONTROL: return MOD_CONTROL;
        case VK_SHIFT:   case VK_LSHIFT:   case VK_RSHIFT:   return MOD_SHIFT;
        case VK_MENU:    case VK_LMENU:    case VK_RMENU:    return MOD_ALT;
        case VK_LWIN:    case VK_RWIN:                       return MOD_WIN;
        default:                                              return 0;
    }
}

// Toggle keys (Caps / Num / Scroll Lock) — bindable as keys themselves
// would be silly but we still want to filter them out from the
// "non-modifier ⇒ commit" check so they don't latch a binding.
bool isToggleVk(UINT vk) noexcept {
    return vk == VK_CAPITAL || vk == VK_NUMLOCK || vk == VK_SCROLL;
}

// Seed the tracker from the OS's view at install time. This is the only
// place GetAsyncKeyState is reliable for us — before the hook starts
// swallowing events, async state mirrors reality. From there on, the
// hook's keydown/up walk keeps it accurate.
UINT readCurrentModifiersFromOS() noexcept {
    UINT m = 0;
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

    // Both KEYDOWN and SYSKEYDOWN — SYSKEYDOWN fires for any key while Alt
    // is held, and our defaults are heavy on Alt+number combos so we'd
    // miss them otherwise.
    const bool isDown = (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN);
    const bool isUp   = (wParam == WM_KEYUP   || wParam == WM_SYSKEYUP);
    const UINT vk     = kb->vkCode;

    if (isDown) {
        // Update self-tracked modifier state BEFORE checking type so the
        // preview reflects the press immediately.
        if (UINT bit = modBitForVk(vk); bit != 0) {
            g_modState.fetch_or(bit);
            g_owner->previewMods_.store(g_modState.load());
            // Modifier-only press: don't commit, just track. Swallow so
            // the foreground app doesn't see "Alt down" while the user
            // is composing a binding (would pop game menus, item
            // highlights, the Win Start menu, etc.).
            return 1;
        }

        if (vk == VK_ESCAPE) {
            // Cancel — swallow so the game doesn't see ESC either.
            g_owner->canceled_.store(true);
            return 1;
        }

        if (isToggleVk(vk)) {
            // Caps / Num / Scroll lock aren't useful as bindings. Let
            // them through (don't swallow) so the lock state still
            // works elsewhere.
            return ::CallNextHookEx(nullptr, nCode, wParam, lParam);
        }

        // Non-modifier key: commit with current modifier state. Order
        // matters — capturedVk_ is the "committed?" sentinel the UI
        // polls, so write mods first.
        g_owner->capturedMods_.store(g_modState.load());
        g_owner->capturedVk_.store(vk);
        // Swallow so the foreground app doesn't receive the keystroke
        // (otherwise rebinding "Alt+5" while POE is up would also send
        // Alt+5 to the game).
        return 1;
    }

    if (isUp) {
        if (UINT bit = modBitForVk(vk); bit != 0) {
            g_modState.fetch_and(~bit);
            g_owner->previewMods_.store(g_modState.load());
            // Releases of swallowed-down modifiers must also be swallowed
            // — otherwise the OS sees an unmatched key-up and gets a
            // "stuck modifier" feeling on whatever's foreground next.
            return 1;
        }
        // Non-modifier key-up: pass through. Important — if we swallowed
        // the down event for a non-modifier and then ALSO swallowed the
        // up, downstream apps that buffer "this key is held" would never
        // see the release.
    }

    return ::CallNextHookEx(nullptr, nCode, wParam, lParam);
}

HotkeyCapture::~HotkeyCapture() {
    stop();
}

void HotkeyCapture::start() {
    stop();  // ensure any prior session is fully torn down

    // Seed modifier tracker from the OS state at this exact moment. This
    // is the one window where GetAsyncKeyState gives us a clean reading
    // — before any swallowing has happened. From here on we maintain
    // g_modState ourselves via hook events.
    g_modState.store(readCurrentModifiersFromOS());

    // Reset capture-specific atomics so the UI sees a clean slate.
    active_.store(true);
    canceled_.store(false);
    previewMods_.store(g_modState.load());
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
    // Don't reset g_modState here — the next start() reseeds from the OS.
}

poebot::hotkey::HotkeyBinding HotkeyCapture::result() const noexcept {
    poebot::hotkey::HotkeyBinding b;
    b.modifiers = capturedMods_.load();
    b.vk        = capturedVk_.load();
    return b;
}

}  // namespace poebot::gui
