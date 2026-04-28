#pragma once
#include <windows.h>

#include <array>
#include <string>
#include <string_view>
#include <unordered_map>

// Persisted hotkey configuration. The `HotkeyManager` (also in this namespace)
// stays the thin Win32 wrapper; this header sits a level above it and adds:
//
//   - HotkeyBinding: a {modifiers, virtual-key} pair that round-trips to a
//                    human-readable string ("Alt+1", "Ctrl+Shift+End").
//   - HotkeyConfig:  the action -> binding map persisted in app.json.
//   - HotkeyAction:  the static registry of every action the app exposes,
//                    pinned in `allHotkeyActions()`. The Settings panel
//                    iterates this list to render the rebinding UI.
//
// Default bindings here mirror the values that used to be hard-coded in
// App::registerHotkeys, so rebooting an existing install picks up the same
// behaviour even before the user touches a thing.
namespace poebot::hotkey {

// One bound combination. `modifiers` is a Win32 MOD_* bitfield; MOD_NOREPEAT
// is added by the registration layer, never persisted. `vk` is a virtual-key
// code (VK_F9, VK_END, '1', 'A'…). vk == 0 means "unbound" — registration
// silently skips entries with vk == 0 instead of erroring, so the user can
// disable an action by clearing it.
struct HotkeyBinding {
    UINT modifiers = 0;
    UINT vk        = 0;

    bool valid() const noexcept { return vk != 0; }

    bool operator==(const HotkeyBinding& o) const noexcept {
        return modifiers == o.modifiers && vk == o.vk;
    }
    bool operator!=(const HotkeyBinding& o) const noexcept { return !(*this == o); }

    // "Ctrl+Shift+Alt+Win+<key>" with the modifier order fixed so two equal
    // bindings always serialize to the same string. Returns "(unbound)" for
    // an empty binding.
    std::string format() const;

    // Best-effort parse of the format() output. Returns {0,0} on any failure
    // (empty input, unknown token, missing key) — callers treat that as
    // "unbound" and may fall back to the default for that action.
    static HotkeyBinding parse(std::string_view s);
};

// One row in the action registry: stable id used as the JSON key + i18n key
// fragment, default binding restored on Reset, plus the i18n key that maps
// to the user-facing label ("Capture orb1" / "捕获 orb1").
struct HotkeyAction {
    const char*    id;
    const char*    labelKey;
    HotkeyBinding  defaultBinding;
};

// All actions the user can rebind. Stable order — the Settings panel renders
// them top-to-bottom in this order. Adding a new action here is enough to
// surface it everywhere (UI, defaults, registration, persistence).
const std::array<HotkeyAction, 14>& allHotkeyActions() noexcept;

using HotkeyConfig = std::unordered_map<std::string, HotkeyBinding>;

// Defaults built from the action registry. Used both as a fallback when
// app.json has no `hotkeys` field and as the source for the per-row Reset
// affordance in the UI.
HotkeyConfig defaultHotkeys();

// Lookup helper: returns the action's default binding, or {0,0} if `id` is
// not a known action.
HotkeyBinding defaultBindingFor(std::string_view id) noexcept;

}  // namespace poebot::hotkey
