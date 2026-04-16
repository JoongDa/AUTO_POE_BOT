#pragma once
#include <poebot/coords.hpp>

namespace poebot::input::mouse {

enum class Button { Left, Right };

// Move cursor to absolute screen position.
void moveTo(ScreenPoint p);

// Click (down+up) at the current cursor position.
void click(Button btn = Button::Left);

// Move then click.
void moveAndClick(ScreenPoint p, Button btn = Button::Left);

// Shift + click (for POE deposit / stash operations).
void shiftClick(Button btn = Button::Left);

// Right-click at current position (convenience).
void rightClick();

}  // namespace poebot::input::mouse
