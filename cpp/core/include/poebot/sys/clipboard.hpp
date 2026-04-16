#pragma once
#include <optional>
#include <string>

namespace poebot::sys::clipboard {

// Read the current clipboard text content (CF_UNICODETEXT) and return it as
// UTF-8. Returns std::nullopt if the clipboard is empty, locked, or does not
// contain text.
std::optional<std::string> readText();

// Empty the clipboard. Used before Ctrl+C so we can detect when fresh
// content arrives.
void clear();

}  // namespace poebot::sys::clipboard
