#include <poebot/input/mouse.hpp>

#include <windows.h>

namespace poebot::input::mouse {

void moveTo(ScreenPoint p) {
    ::SetCursorPos(p.x, p.y);
}

void click(Button btn) {
    const DWORD down = (btn == Button::Left) ? MOUSEEVENTF_LEFTDOWN : MOUSEEVENTF_RIGHTDOWN;
    const DWORD up   = (btn == Button::Left) ? MOUSEEVENTF_LEFTUP   : MOUSEEVENTF_RIGHTUP;
    INPUT inputs[2]{};
    inputs[0].type = INPUT_MOUSE;
    inputs[0].mi.dwFlags = down;
    inputs[1].type = INPUT_MOUSE;
    inputs[1].mi.dwFlags = up;
    ::SendInput(2, inputs, sizeof(INPUT));
}

void moveAndClick(ScreenPoint p, Button btn) {
    moveTo(p);
    click(btn);
}

void shiftClick(Button btn) {
    const DWORD down = (btn == Button::Left) ? MOUSEEVENTF_LEFTDOWN : MOUSEEVENTF_RIGHTDOWN;
    const DWORD up   = (btn == Button::Left) ? MOUSEEVENTF_LEFTUP   : MOUSEEVENTF_RIGHTUP;
    INPUT inputs[4]{};
    // Shift down
    inputs[0].type = INPUT_KEYBOARD;
    inputs[0].ki.wVk = VK_SHIFT;
    // Mouse down
    inputs[1].type = INPUT_MOUSE;
    inputs[1].mi.dwFlags = down;
    // Mouse up
    inputs[2].type = INPUT_MOUSE;
    inputs[2].mi.dwFlags = up;
    // Shift up
    inputs[3].type = INPUT_KEYBOARD;
    inputs[3].ki.wVk = VK_SHIFT;
    inputs[3].ki.dwFlags = KEYEVENTF_KEYUP;
    ::SendInput(4, inputs, sizeof(INPUT));
}

void rightClick() {
    click(Button::Right);
}

}  // namespace poebot::input::mouse
