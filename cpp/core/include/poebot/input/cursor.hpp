#pragma once
#include <poebot/coords.hpp>

namespace poebot::input {

// Current mouse cursor position in absolute screen coordinates.
// Thread-safe (Win32 GetCursorPos is).
ScreenPoint cursorPosition() noexcept;

}  // namespace poebot::input
