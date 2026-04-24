#pragma once
#include <poebot/coords.hpp>

#include <atomic>

namespace poebot::input::mouse {

enum class Button { Left, Right };

// Move cursor to absolute screen position.
void moveTo(ScreenPoint p);

// Click (down+up) at the current cursor position.
void click(Button btn = Button::Left);

// Move then click.
void moveAndClick(ScreenPoint p, Button btn = Button::Left);

// Ctrl + click — POE's "move item between inventory and stash" shortcut,
// used by the deposit task. (Shift+click is the stack-split shortcut and
// isn't what we want here.)
void ctrlClick(Button btn = Button::Left);

// Right-click at current position (convenience).
void rightClick();

// --- Humanized variants -----------------------------------------------------
// These replace the instant SetCursorPos + SendInput chain with a bezier
// move, Gaussian position jitter, and a small Gaussian settle delay after
// the click. They check `stop` at every bezier step so tasks can still be
// cancelled promptly. Returns false if `stop` was set mid-move.

bool humanClick(std::atomic<bool>& stop, ScreenPoint p, Button btn = Button::Left);
bool humanCtrlClick(std::atomic<bool>& stop, ScreenPoint p);

}  // namespace poebot::input::mouse
