#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <ddraw.h>

namespace gamevaultdraw
{
class SurfaceProxy final : public IDirectDrawSurface
{
public:
    SurfaceProxy(IDirectDrawSurface* real, bool isPrimary) noexcept;
    IDirectDrawSurface* Real() const noexcept { return real_; }

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** object) override;
    ULONG STDMETHODCALLTYPE AddRef() override;
    ULONG STDMETHODCALLTYPE Release() override;
    HRESULT STDMETHODCALLTYPE AddAttachedSurface(IDirectDrawSurface* surface) override;
    HRESULT STDMETHODCALLTYPE AddOverlayDirtyRect(RECT* rect) override;
    HRESULT STDMETHODCALLTYPE Blt(
        RECT* destinationRect, IDirectDrawSurface* source, RECT* sourceRect,
        DWORD flags, DDBLTFX* effects) override;
    HRESULT STDMETHODCALLTYPE BltBatch(DDBLTBATCH* batch, DWORD count, DWORD flags) override;
    HRESULT STDMETHODCALLTYPE BltFast(
        DWORD x, DWORD y, IDirectDrawSurface* source, RECT* sourceRect, DWORD flags) override;
    HRESULT STDMETHODCALLTYPE DeleteAttachedSurface(DWORD flags, IDirectDrawSurface* surface) override;
    HRESULT STDMETHODCALLTYPE EnumAttachedSurfaces(void* context, LPDDENUMSURFACESCALLBACK callback) override;
    HRESULT STDMETHODCALLTYPE EnumOverlayZOrders(DWORD flags, void* context, LPDDENUMSURFACESCALLBACK callback) override;
    HRESULT STDMETHODCALLTYPE Flip(IDirectDrawSurface* targetOverride, DWORD flags) override;
    HRESULT STDMETHODCALLTYPE GetAttachedSurface(DDSCAPS* caps, IDirectDrawSurface** surface) override;
    HRESULT STDMETHODCALLTYPE GetBltStatus(DWORD flags) override;
    HRESULT STDMETHODCALLTYPE GetCaps(DDSCAPS* caps) override;
    HRESULT STDMETHODCALLTYPE GetClipper(IDirectDrawClipper** clipper) override;
    HRESULT STDMETHODCALLTYPE GetColorKey(DWORD flags, DDCOLORKEY* key) override;
    HRESULT STDMETHODCALLTYPE GetDC(HDC* dc) override;
    HRESULT STDMETHODCALLTYPE GetFlipStatus(DWORD flags) override;
    HRESULT STDMETHODCALLTYPE GetOverlayPosition(LONG* x, LONG* y) override;
    HRESULT STDMETHODCALLTYPE GetPalette(IDirectDrawPalette** palette) override;
    HRESULT STDMETHODCALLTYPE GetPixelFormat(DDPIXELFORMAT* format) override;
    HRESULT STDMETHODCALLTYPE GetSurfaceDesc(DDSURFACEDESC* description) override;
    HRESULT STDMETHODCALLTYPE Initialize(IDirectDraw* directDraw, DDSURFACEDESC* description) override;
    HRESULT STDMETHODCALLTYPE IsLost() override;
    HRESULT STDMETHODCALLTYPE Lock(RECT* rect, DDSURFACEDESC* description, DWORD flags, HANDLE eventHandle) override;
    HRESULT STDMETHODCALLTYPE ReleaseDC(HDC dc) override;
    HRESULT STDMETHODCALLTYPE Restore() override;
    HRESULT STDMETHODCALLTYPE SetClipper(IDirectDrawClipper* clipper) override;
    HRESULT STDMETHODCALLTYPE SetColorKey(DWORD flags, DDCOLORKEY* key) override;
    HRESULT STDMETHODCALLTYPE SetOverlayPosition(LONG x, LONG y) override;
    HRESULT STDMETHODCALLTYPE SetPalette(IDirectDrawPalette* palette) override;
    HRESULT STDMETHODCALLTYPE Unlock(void* surfaceData) override;
    HRESULT STDMETHODCALLTYPE UpdateOverlay(
        RECT* sourceRect, IDirectDrawSurface* destination, RECT* destinationRect,
        DWORD flags, DDOVERLAYFX* effects) override;
    HRESULT STDMETHODCALLTYPE UpdateOverlayDisplay(DWORD flags) override;
    HRESULT STDMETHODCALLTYPE UpdateOverlayZOrder(DWORD flags, IDirectDrawSurface* reference) override;

private:
    ~SurfaceProxy();

    volatile LONG references_ = 1;
    volatile LONG bltCalls_ = 0;
    volatile LONG lockCalls_ = 0;
    volatile LONG unlockCalls_ = 0;
    IDirectDrawSurface* real_ = nullptr;
    IDirectDrawPalette* virtualPalette_ = nullptr;
    bool isPrimary_ = false;
    const void* lockedPixels_ = nullptr;
    LONG lockedPitch_ = 0;
    DWORD lockedWidth_ = 0;
    DWORD lockedHeight_ = 0;
    DWORD lockedBitsPerPixel_ = 0;
};

HRESULT WrapSurface(IDirectDrawSurface** surface, bool isPrimary = false);
IDirectDrawSurface* UnwrapSurface(IDirectDrawSurface* surface) noexcept;
void SetPresentationWindow(HWND window) noexcept;
}
