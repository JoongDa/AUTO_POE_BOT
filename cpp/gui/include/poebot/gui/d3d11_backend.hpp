#pragma once
#include <windows.h>
#include <d3d11.h>
#include <dxgi.h>

namespace poebot::gui {

// Thin RAII wrapper around the D3D11 device + swap chain + RTV trio used as
// ImGui's rendering backend. Owns no textures other than the backbuffer RTV.
class D3D11Backend {
public:
    D3D11Backend() = default;
    ~D3D11Backend() { shutdown(); }

    D3D11Backend(const D3D11Backend&) = delete;
    D3D11Backend& operator=(const D3D11Backend&) = delete;

    // Creates device, swap chain, and backbuffer RTV. Falls back to WARP if
    // hardware is unavailable (VM / RDP).
    bool init(HWND hwnd);
    void shutdown();

    // Buffered resize; applied at frame boundary by applyPendingResize().
    // Safe to call from WndProc.
    void requestResize(UINT width, UINT height);
    void applyPendingResize();

    // Bind RTV and clear to the given premultiplied RGBA color.
    void beginFrame(const float clearColor[4]);
    // Present.
    void endFrame();

    ID3D11Device*        device()  const noexcept { return device_; }
    ID3D11DeviceContext* context() const noexcept { return context_; }

private:
    bool createRenderTarget();
    void releaseRenderTarget();

    ID3D11Device*           device_     = nullptr;
    ID3D11DeviceContext*    context_    = nullptr;
    IDXGISwapChain*         swapChain_  = nullptr;
    ID3D11RenderTargetView* rtv_        = nullptr;
    UINT                    pendingW_   = 0;
    UINT                    pendingH_   = 0;
};

}  // namespace poebot::gui
