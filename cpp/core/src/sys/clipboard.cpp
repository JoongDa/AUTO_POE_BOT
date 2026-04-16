#include <poebot/sys/clipboard.hpp>

#include <poebot/sys/encoding.hpp>

#include <windows.h>

namespace poebot::sys::clipboard {

std::optional<std::string> readText() {
    if (!::OpenClipboard(nullptr)) return std::nullopt;

    HANDLE h = ::GetClipboardData(CF_UNICODETEXT);
    if (!h) {
        ::CloseClipboard();
        return std::nullopt;
    }

    auto* ptr = static_cast<const wchar_t*>(::GlobalLock(h));
    if (!ptr) {
        ::CloseClipboard();
        return std::nullopt;
    }

    std::string result = poebot::sys::wideToUtf8(ptr);

    ::GlobalUnlock(h);
    ::CloseClipboard();

    return result;
}

}  // namespace poebot::sys::clipboard
