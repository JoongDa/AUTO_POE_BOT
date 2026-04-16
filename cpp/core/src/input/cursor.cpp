#include <poebot/input/cursor.hpp>

#include <windows.h>

namespace poebot::input {

ScreenPoint cursorPosition() noexcept {
    POINT p{};
    ::GetCursorPos(&p);
    return ScreenPoint{p.x, p.y};
}

}  // namespace poebot::input
