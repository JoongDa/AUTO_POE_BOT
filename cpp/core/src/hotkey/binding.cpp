#include <poebot/hotkey/binding.hpp>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <string>

namespace poebot::hotkey {

namespace {

// Single source of truth: virtual-key code <-> printable token. Used by both
// format() (vk -> token) and parse() (token -> vk). The order favors keys
// the bot actually uses; the long tail of OEM/numpad keys could be added if
// we ever need them.
struct VkName { UINT vk; const char* name; };
constexpr VkName kNamedKeys[] = {
    // Function keys.
    {VK_F1,  "F1"},  {VK_F2,  "F2"},  {VK_F3,  "F3"},  {VK_F4,  "F4"},
    {VK_F5,  "F5"},  {VK_F6,  "F6"},  {VK_F7,  "F7"},  {VK_F8,  "F8"},
    {VK_F9,  "F9"},  {VK_F10, "F10"}, {VK_F11, "F11"}, {VK_F12, "F12"},
    {VK_F13, "F13"}, {VK_F14, "F14"}, {VK_F15, "F15"}, {VK_F16, "F16"},
    {VK_F17, "F17"}, {VK_F18, "F18"}, {VK_F19, "F19"}, {VK_F20, "F20"},

    // Navigation cluster.
    {VK_INSERT, "Insert"}, {VK_DELETE, "Delete"},
    {VK_HOME,   "Home"},   {VK_END,    "End"},
    {VK_PRIOR,  "PageUp"}, {VK_NEXT,   "PageDown"},
    {VK_LEFT,   "Left"},   {VK_RIGHT,  "Right"},
    {VK_UP,     "Up"},     {VK_DOWN,   "Down"},

    // Editing.
    {VK_TAB,    "Tab"},    {VK_RETURN,    "Enter"},
    {VK_SPACE,  "Space"},  {VK_BACK,      "Backspace"},
    {VK_ESCAPE, "Escape"}, {VK_PAUSE,     "Pause"},
    {VK_SCROLL, "ScrollLock"},
    {VK_CAPITAL,"CapsLock"},

    // Numpad. Distinct from the top-row digits so users binding "Num0" don't
    // accidentally also fire on "0". We keep both vk codes.
    {VK_NUMPAD0, "Num0"}, {VK_NUMPAD1, "Num1"}, {VK_NUMPAD2, "Num2"},
    {VK_NUMPAD3, "Num3"}, {VK_NUMPAD4, "Num4"}, {VK_NUMPAD5, "Num5"},
    {VK_NUMPAD6, "Num6"}, {VK_NUMPAD7, "Num7"}, {VK_NUMPAD8, "Num8"},
    {VK_NUMPAD9, "Num9"},
    {VK_MULTIPLY, "Num*"}, {VK_ADD, "Num+"}, {VK_SUBTRACT, "Num-"},
    {VK_DIVIDE,   "Num/"}, {VK_DECIMAL, "Num."},

    // OEM punctuation — the values match a US layout.
    {VK_OEM_MINUS,  "-"},   {VK_OEM_PLUS,   "="},
    {VK_OEM_COMMA,  ","},   {VK_OEM_PERIOD, "."},
    {VK_OEM_1,      ";"},   {VK_OEM_2,      "/"},
    {VK_OEM_3,      "`"},   {VK_OEM_4,      "["},
    {VK_OEM_5,      "\\"},  {VK_OEM_6,      "]"},
    {VK_OEM_7,      "'"},
};

const char* lookupVkName(UINT vk) noexcept {
    for (const auto& nk : kNamedKeys) if (nk.vk == vk) return nk.name;
    return nullptr;
}

UINT lookupNamedKey(std::string_view token) noexcept {
    // Case-insensitive compare on tokens like "End", "F9", "PageUp".
    for (const auto& nk : kNamedKeys) {
        const std::size_t n = std::strlen(nk.name);
        if (token.size() != n) continue;
        bool match = true;
        for (std::size_t i = 0; i < n; ++i) {
            if (std::tolower(static_cast<unsigned char>(nk.name[i])) !=
                std::tolower(static_cast<unsigned char>(token[i]))) {
                match = false;
                break;
            }
        }
        if (match) return nk.vk;
    }
    return 0;
}

// Trim ASCII whitespace from both ends — robust against extra spaces around
// the "+" separators in user-edited app.json.
std::string_view trim(std::string_view s) noexcept {
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front()))) s.remove_prefix(1);
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back())))  s.remove_suffix(1);
    return s;
}

bool ieq(std::string_view a, std::string_view b) noexcept {
    if (a.size() != b.size()) return false;
    for (std::size_t i = 0; i < a.size(); ++i) {
        if (std::tolower(static_cast<unsigned char>(a[i])) !=
            std::tolower(static_cast<unsigned char>(b[i]))) {
            return false;
        }
    }
    return true;
}

}  // namespace

std::string HotkeyBinding::format() const {
    if (vk == 0) return "(unbound)";

    std::string out;
    // Fixed modifier order — Ctrl, Shift, Alt, Win — so two HotkeyBindings
    // with the same modifier set always produce the same string. This is
    // what UI labels and JSON keys round-trip on.
    if (modifiers & MOD_CONTROL) out += "Ctrl+";
    if (modifiers & MOD_SHIFT)   out += "Shift+";
    if (modifiers & MOD_ALT)     out += "Alt+";
    if (modifiers & MOD_WIN)     out += "Win+";

    if (const char* named = lookupVkName(vk)) {
        out += named;
    } else if ((vk >= '0' && vk <= '9') || (vk >= 'A' && vk <= 'Z')) {
        // Digits and uppercase letters share their printable ASCII char.
        out += static_cast<char>(vk);
    } else {
        // Fallback for anything we don't have a friendly name for. The user
        // sees something like "VK_0xC0" — ugly but unambiguous, and it
        // round-trips through parse() so settings still work.
        char buf[12];
        std::snprintf(buf, sizeof(buf), "VK_0x%02X", vk);
        out += buf;
    }
    return out;
}

HotkeyBinding HotkeyBinding::parse(std::string_view s) {
    HotkeyBinding b{};
    s = trim(s);
    if (s.empty() || ieq(s, "(unbound)")) return b;

    // Split on '+'. Last token is the key; everything before is modifiers.
    while (!s.empty()) {
        auto plus = s.find('+');
        std::string_view token = (plus == std::string_view::npos) ? s : s.substr(0, plus);
        token = trim(token);

        const bool isLast = (plus == std::string_view::npos);

        if (!isLast) {
            // Modifier token.
            if      (ieq(token, "Ctrl") || ieq(token, "Control")) b.modifiers |= MOD_CONTROL;
            else if (ieq(token, "Shift"))                          b.modifiers |= MOD_SHIFT;
            else if (ieq(token, "Alt"))                            b.modifiers |= MOD_ALT;
            else if (ieq(token, "Win") || ieq(token, "Meta") || ieq(token, "Super"))
                                                                   b.modifiers |= MOD_WIN;
            else {
                // Unknown modifier — bail. Returning {0,0} signals "couldn't
                // parse"; caller falls back to the default binding.
                return HotkeyBinding{};
            }
            s.remove_prefix(plus + 1);
            continue;
        }

        // Last token is the key.
        if (token.empty()) return HotkeyBinding{};

        if (UINT named = lookupNamedKey(token); named != 0) {
            b.vk = named;
        } else if (token.size() == 1) {
            const unsigned char c = static_cast<unsigned char>(token[0]);
            if (c >= '0' && c <= '9') {
                b.vk = c;
            } else if (c >= 'a' && c <= 'z') {
                b.vk = static_cast<UINT>(c - 'a' + 'A');
            } else if (c >= 'A' && c <= 'Z') {
                b.vk = c;
            } else {
                return HotkeyBinding{};
            }
        } else if (token.size() > 5 && ieq(token.substr(0, 5), "VK_0X")) {
            // Round-trip for the format() fallback path. Manual hex parse
            // avoids the MSVC sscanf deprecation warning while staying
            // simpler than pulling in <charconv> for one fallback path.
            unsigned int v = 0;
            for (std::size_t i = 5; i < token.size(); ++i) {
                const unsigned char c = static_cast<unsigned char>(token[i]);
                unsigned int d;
                if      (c >= '0' && c <= '9') d = c - '0';
                else if (c >= 'a' && c <= 'f') d = 10 + (c - 'a');
                else if (c >= 'A' && c <= 'F') d = 10 + (c - 'A');
                else return HotkeyBinding{};
                v = (v << 4) | d;
            }
            if (v == 0) return HotkeyBinding{};
            b.vk = static_cast<UINT>(v);
        } else {
            return HotkeyBinding{};
        }
        break;  // last token consumed
    }

    return b;
}

const std::array<HotkeyAction, 14>& allHotkeyActions() noexcept {
    // Ordered the way the Settings panel renders. Top group is the
    // task-control hotkeys the user touches most (F1/F2/F3 to start, End
    // to stop); F9 sits with them as another global window-control key;
    // capture rows fill the bottom in their grouped order (orbs, anchors,
    // inventory).
    //
    // Adding a row here is enough to surface it everywhere — App's
    // makeCallbackFor() picks the action up by id and the Settings panel
    // renders it as soon as a matching i18n label exists.
    static const std::array<HotkeyAction, 14> k = {{
        {"task.start.craft",   "hotkey.action.task.start.craft",   {0,           VK_F1}},
        {"task.start.map",     "hotkey.action.task.start.map",     {0,           VK_F2}},
        {"task.start.deposit", "hotkey.action.task.start.deposit", {0,           VK_F3}},
        {"task.stop",          "hotkey.action.task.stop",          {0,           VK_END}},
        {"overlay.toggle",     "hotkey.action.overlay.toggle",     {0,           VK_F9}},
        {"capture.orb1",       "hotkey.action.capture.orb1",       {MOD_ALT,     '1'}},
        {"capture.orb2",       "hotkey.action.capture.orb2",       {MOD_ALT,     '2'}},
        {"capture.orb3",       "hotkey.action.capture.orb3",       {MOD_ALT,     '3'}},
        {"capture.baseItem",   "hotkey.action.capture.baseItem",   {MOD_ALT,     '0'}},
        {"capture.p01Item",    "hotkey.action.capture.p01Item",    {MOD_ALT,     '8'}},
        {"capture.p10Item",    "hotkey.action.capture.p10Item",    {MOD_ALT,     '9'}},
        {"capture.invBase",    "hotkey.action.capture.invBase",    {MOD_ALT,     '4'}},
        {"capture.invP01",     "hotkey.action.capture.invP01",     {MOD_ALT,     '5'}},
        {"capture.invP10",     "hotkey.action.capture.invP10",     {MOD_ALT,     '6'}},
    }};
    return k;
}

HotkeyConfig defaultHotkeys() {
    HotkeyConfig cfg;
    for (const auto& a : allHotkeyActions()) cfg[a.id] = a.defaultBinding;
    return cfg;
}

HotkeyBinding defaultBindingFor(std::string_view id) noexcept {
    for (const auto& a : allHotkeyActions()) {
        if (id == a.id) return a.defaultBinding;
    }
    return HotkeyBinding{};
}

}  // namespace poebot::hotkey
