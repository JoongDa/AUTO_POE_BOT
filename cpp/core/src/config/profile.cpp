#include <poebot/config/profile.hpp>

namespace poebot::config {

GameProfile defaultProfileFor(std::string_view version) {
    GameProfile p;
    // NOTE: POE1's window title ("Path of Exile") is a substring of POE2's
    // ("Path of Exile 2"). The window-detection module must probe POE2 first
    // to avoid false positives. Keep patterns simple here; detection order
    // belongs in the detector, not the profile.
    if (version == "poe2") {
        p.name = "poe2";
        p.displayName = "Path of Exile 2";
        p.windowTitlePattern = "Path of Exile 2";
    } else {
        p.name = "poe1";
        p.displayName = "Path of Exile 1";
        p.windowTitlePattern = "Path of Exile";
    }

    p.craft.cols = 10;
    p.craft.rows = 2;
    p.map.cols = 4;
    p.map.rows = 3;
    p.deposit.cols = 12;
    p.deposit.rows = 5;
    return p;
}

}  // namespace poebot::config
