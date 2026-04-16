#pragma once
#include <windows.h>

namespace poebot::input::keyboard {

// Press and release a single virtual-key code.
void pressAndRelease(UINT vk);

// Ctrl+C — reads the hovered item's text into the clipboard in POE.
void sendCtrlC();

}  // namespace poebot::input::keyboard
