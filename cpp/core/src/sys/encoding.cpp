#include <poebot/sys/encoding.hpp>

#include <windows.h>

namespace poebot::sys {

std::wstring utf8ToWide(std::string_view utf8) {
    if (utf8.empty()) return {};
    int n = ::MultiByteToWideChar(CP_UTF8, 0, utf8.data(),
                                  static_cast<int>(utf8.size()),
                                  nullptr, 0);
    if (n <= 0) return {};
    std::wstring out(static_cast<size_t>(n), L'\0');
    ::MultiByteToWideChar(CP_UTF8, 0, utf8.data(),
                          static_cast<int>(utf8.size()),
                          out.data(), n);
    return out;
}

std::string wideToUtf8(std::wstring_view wide) {
    if (wide.empty()) return {};
    int n = ::WideCharToMultiByte(CP_UTF8, 0, wide.data(),
                                  static_cast<int>(wide.size()),
                                  nullptr, 0, nullptr, nullptr);
    if (n <= 0) return {};
    std::string out(static_cast<size_t>(n), '\0');
    ::WideCharToMultiByte(CP_UTF8, 0, wide.data(),
                          static_cast<int>(wide.size()),
                          out.data(), n, nullptr, nullptr);
    return out;
}

}  // namespace poebot::sys
