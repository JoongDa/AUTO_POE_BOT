#include <poebot/config/settings.hpp>

namespace poebot::config {

GameProfile* Settings::active() noexcept {
    auto it = profiles.find(activeProfile);
    return it == profiles.end() ? nullptr : &it->second;
}

const GameProfile* Settings::active() const noexcept {
    auto it = profiles.find(activeProfile);
    return it == profiles.end() ? nullptr : &it->second;
}

Settings Settings::makeDefault() {
    Settings s;
    s.version = 1;
    s.language = "zh";
    s.activeProfile = "poe1";
    s.profiles.emplace("poe1", defaultProfileFor("poe1"));
    s.profiles.emplace("poe2", defaultProfileFor("poe2"));
    return s;
}

}  // namespace poebot::config
