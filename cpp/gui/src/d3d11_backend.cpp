#include <poebot/gui/d3d11_backend.hpp>

#include <spdlog/spdlog.h>

namespace poebot::gui {

bool D3D11Backend::init(HWND hwnd) {
    DXGI_SWAP_CHAIN_DESC sd{};
    sd.BufferCount = 2;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hwnd;
    sd.SampleDesc.Count = 1;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    UINT createFlags = 0;
#ifdef _DEBUG
    createFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif
    D3D_FEATURE_LEVEL levels[] = {D3D_FEATURE_LEVEL_11_0};
    D3D_FEATURE_LEVEL obtained;

    HRESULT hr = D3D11CreateDeviceAndSwapChain(
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, createFlags,
        levels, 1, D3D11_SDK_VERSION, &sd,
        &swapChain_, &device_, &obtained, &context_);

    if (hr == DXGI_ERROR_UNSUPPORTED) {
        spdlog::warn("D3D11 hardware device unavailable, falling back to WARP");
        hr = D3D11CreateDeviceAndSwapChain(
            nullptr, D3D_DRIVER_TYPE_WARP, nullptr, createFlags,
            levels, 1, D3D11_SDK_VERSION, &sd,
            &swapChain_, &device_, &obtained, &context_);
    }
    if (FAILED(hr)) {
        spdlog::error("D3D11CreateDeviceAndSwapChain failed: 0x{:08X}",
                      static_cast<unsigned>(hr));
        return false;
    }
    return createRenderTarget();
}

void D3D11Backend::shutdown() {
    releaseRenderTarget();
    if (swapChain_) { swapChain_->Release(); swapChain_ = nullptr; }
    if (context_)   { context_->Release();   context_   = nullptr; }
    if (device_)    { device_->Release();    device_    = nullptr; }
}

void D3D11Backend::requestResize(UINT w, UINT h) {
    pendingW_ = w;
    pendingH_ = h;
}

void D3D11Backend::applyPendingResize() {
    if (pendingW_ == 0 && pendingH_ == 0) return;
    if (!swapChain_) return;
    releaseRenderTarget();
    swapChain_->ResizeBuffers(0, pendingW_, pendingH_, DXGI_FORMAT_UNKNOWN, 0);
    pendingW_ = 0;
    pendingH_ = 0;
    createRenderTarget();
}

void D3D11Backend::beginFrame(const float clearColor[4]) {
    context_->OMSetRenderTargets(1, &rtv_, nullptr);
    context_->ClearRenderTargetView(rtv_, clearColor);
}

void D3D11Backend::endFrame() {
    swapChain_->Present(1, 0);
}

bool D3D11Backend::createRenderTarget() {
    ID3D11Texture2D* back = nullptr;
    if (FAILED(swapChain_->GetBuffer(0, IID_PPV_ARGS(&back))) || !back) {
        spdlog::error("IDXGISwapChain::GetBuffer failed");
        return false;
    }
    HRESULT hr = device_->CreateRenderTargetView(back, nullptr, &rtv_);
    back->Release();
    if (FAILED(hr)) {
        spdlog::error("CreateRenderTargetView failed: 0x{:08X}",
                      static_cast<unsigned>(hr));
        return false;
    }
    return true;
}

void D3D11Backend::releaseRenderTarget() {
    if (rtv_) { rtv_->Release(); rtv_ = nullptr; }
}

}  // namespace poebot::gui
