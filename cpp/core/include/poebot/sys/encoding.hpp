#pragma once
#include <string>
#include <string_view>

namespace poebot::sys {

// UTF-8 <-> UTF-16 conversions using the Win32 code-page APIs. All other
// strings in the project are UTF-8; these helpers exist only at the Win32
// API boundary (window titles, clipboard, file paths in wchar_t form).

std::wstring utf8ToWide(std::string_view utf8);
std::string  wideToUtf8(std::wstring_view wide);

}  // namespace poebot::sys
