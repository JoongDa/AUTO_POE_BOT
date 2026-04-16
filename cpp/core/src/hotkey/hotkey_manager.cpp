#include <poebot/hotkey/hotkey_manager.hpp>

#include <spdlog/spdlog.h>

namespace poebot::hotkey {

int HotkeyManager::registerHotkey(Mod modifiers, UINT vk, Callback cb) {
    if (!hwnd_) {
        spdlog::error("HotkeyManager::registerHotkey called before attach()");
        return 0;
    }
    const int id = nextId_++;
    if (!::RegisterHotKey(hwnd_, id, static_cast<UINT>(modifiers), vk)) {
        const DWORD err = ::GetLastError();
        spdlog::warn("RegisterHotKey(vk=0x{:02X}, mod=0x{:X}) failed: GetLastError={}",
                     vk, static_cast<UINT>(modifiers), err);
        return 0;
    }
    callbacks_.emplace(id, std::move(cb));
    return id;
}

void HotkeyManager::unregisterHotkey(int id) {
    if (!hwnd_ || id == 0) return;
    auto it = callbacks_.find(id);
    if (it == callbacks_.end()) return;
    ::UnregisterHotKey(hwnd_, id);
    callbacks_.erase(it);
}

void HotkeyManager::clear() {
    if (hwnd_) {
        for (auto& [id, _] : callbacks_) {
            ::UnregisterHotKey(hwnd_, id);
        }
    }
    callbacks_.clear();
}

bool HotkeyManager::dispatch(WPARAM id) {
    auto it = callbacks_.find(static_cast<int>(id));
    if (it == callbacks_.end()) return false;
    // Copy before invoking in case the callback unregisters this id.
    Callback cb = it->second;
    cb();
    return true;
}

}  // namespace poebot::hotkey
