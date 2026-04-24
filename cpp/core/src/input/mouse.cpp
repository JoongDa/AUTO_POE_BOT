#include <poebot/input/mouse.hpp>

#include <poebot/input/motion.hpp>

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

void ctrlClick(Button btn) {
    const DWORD down = (btn == Button::Left) ? MOUSEEVENTF_LEFTDOWN : MOUSEEVENTF_RIGHTDOWN;
    const DWORD up   = (btn == Button::Left) ? MOUSEEVENTF_LEFTUP   : MOUSEEVENTF_RIGHTUP;
    INPUT inputs[4]{};
    // Ctrl down
    inputs[0].type = INPUT_KEYBOARD;
    inputs[0].ki.wVk = VK_CONTROL;
    // Mouse down
    inputs[1].type = INPUT_MOUSE;
    inputs[1].mi.dwFlags = down;
    // Mouse up
    inputs[2].type = INPUT_MOUSE;
    inputs[2].mi.dwFlags = up;
    // Ctrl up
    inputs[3].type = INPUT_KEYBOARD;
    inputs[3].ki.wVk = VK_CONTROL;
    inputs[3].ki.dwFlags = KEYEVENTF_KEYUP;
    ::SendInput(4, inputs, sizeof(INPUT));
}

void rightClick() {
    click(Button::Right);
}

// --- Humanized variants ------------------------------------------------------

namespace {

// Small Gaussian jitter around the target so repeated clicks don't land on
// exactly the same pixel. Stddev ~1.5px matches the AHK original.
ScreenPoint jitter(ScreenPoint p) {
    const int dx = static_cast<int>(std::lround(motion::gaussRand(1.5)));
    const int dy = static_cast<int>(std::lround(motion::gaussRand(1.5)));
    return {p.x + dx, p.y + dy};
}

}  // namespace

bool humanClick(std::atomic<bool>& stop, ScreenPoint p, Button btn) {
    const ScreenPoint t = jitter(p);
    if (!motion::bezierMoveTo(stop, t)) return false;
    click(btn);
    // Short Gaussian settle so the click isn't immediately followed by the
    // next move — mirrors the AHK HumanClick trailer.
    return motion::gaussSleep(stop,
                              std::chrono::milliseconds(25),
                              std::chrono::milliseconds(6),
                              std::chrono::milliseconds(12));
}

bool humanCtrlClick(std::atomic<bool>& stop, ScreenPoint p) {
    const ScreenPoint t = jitter(p);
    if (!motion::bezierMoveTo(stop, t)) return false;
    ctrlClick(Button::Left);
    return motion::gaussSleep(stop,
                              std::chrono::milliseconds(25),
                              std::chrono::milliseconds(6),
                              std::chrono::milliseconds(12));
}

}  // namespace poebot::input::mouse
