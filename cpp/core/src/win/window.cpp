#include <poebot/win/window.hpp>

#include <poebot/sys/encoding.hpp>

#include <spdlog/spdlog.h>

namespace poebot::win {

// ---------- EnumWindows helper -----------------------------------------------

namespace {

struct FindCtx {
    std::wstring pattern;
    HWND         result = nullptr;
};

BOOL CALLBACK findProc(HWND hwnd, LPARAM lp) {
    auto* ctx = reinterpret_cast<FindCtx*>(lp);
    if (!::IsWindowVisible(hwnd)) return TRUE;

    wchar_t buf[512]{};
    int n = ::GetWindowTextW(hwnd, buf, 512);
    if (n <= 0) return TRUE;

    std::wstring_view tv(buf, static_cast<size_t>(n));
    if (tv.find(ctx->pattern) != std::wstring_view::npos) {
        ctx->result = hwnd;
        return FALSE;  // stop enumeration
    }
    return TRUE;
}

}  // namespace

// ---------- GameWindow -------------------------------------------------------

bool GameWindow::tryFind(std::string_view titleSubstr) {
    if (titleSubstr.empty()) return false;
    FindCtx ctx;
    ctx.pattern = poebot::sys::utf8ToWide(titleSubstr);
    ::EnumWindows(findProc, reinterpret_cast<LPARAM>(&ctx));
    if (ctx.result) {
        hwnd_ = ctx.result;
        return true;
    }
    return false;
}

std::string GameWindow::title() const {
    if (!valid()) return {};
    wchar_t buf[512]{};
    int n = ::GetWindowTextW(hwnd_, buf, 512);
    if (n <= 0) return {};
    return poebot::sys::wideToUtf8({buf, static_cast<size_t>(n)});
}

GameWindow::Size GameWindow::clientSize() const {
    if (!valid()) return {};
    RECT rc{};
    ::GetClientRect(hwnd_, &rc);
    return {rc.right - rc.left, rc.bottom - rc.top};
}

ScreenPoint GameWindow::clientToScreen(ClientPoint p) const {
    POINT pt{p.x, p.y};
    if (valid()) ::ClientToScreen(hwnd_, &pt);
    return ScreenPoint{pt.x, pt.y};
}

ClientPoint GameWindow::screenToClient(ScreenPoint p) const {
    POINT pt{p.x, p.y};
    if (valid()) ::ScreenToClient(hwnd_, &pt);
    return ClientPoint{pt.x, pt.y};
}

void GameWindow::activate() const {
    if (!valid()) return;
    if (::IsIconic(hwnd_)) ::ShowWindow(hwnd_, SW_RESTORE);

    // AttachThreadInput trick: the foreground thread must cooperate for
    // SetForegroundWindow to succeed. Attach our thread to the target
    // window's thread briefly, call SetForegroundWindow, then detach.
    DWORD targetTid = ::GetWindowThreadProcessId(hwnd_, nullptr);
    DWORD selfTid   = ::GetCurrentThreadId();
    bool  attached  = false;
    if (targetTid != selfTid) {
        attached = ::AttachThreadInput(selfTid, targetTid, TRUE) != 0;
    }
    ::SetForegroundWindow(hwnd_);
    ::BringWindowToTop(hwnd_);
    if (attached) {
        ::AttachThreadInput(selfTid, targetTid, FALSE);
    }
}

bool GameWindow::isForeground() const {
    return valid() && ::GetForegroundWindow() == hwnd_;
}

}  // namespace poebot::win
