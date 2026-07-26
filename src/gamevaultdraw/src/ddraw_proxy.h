#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <ddraw.h>

namespace gamevaultdraw
{
class DirectDrawProxy final : public IDirectDraw
{
public:
    explicit DirectDrawProxy(IDirectDraw* real) noexcept;
    IDirectDraw* Real() const noexcept { return real_; }

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** object) override;
    ULONG STDMETHODCALLTYPE AddRef() override;
    ULONG STDMETHODCALLTYPE Release() override;

    HRESULT STDMETHODCALLTYPE Compact() override;
    HRESULT STDMETHODCALLTYPE CreateClipper(
        DWORD flags, IDirectDrawClipper** clipper, IUnknown* outer) override;
    HRESULT STDMETHODCALLTYPE CreatePalette(
        DWORD flags, PALETTEENTRY* entries, IDirectDrawPalette** palette, IUnknown* outer) override;
    HRESULT STDMETHODCALLTYPE CreateSurface(
        DDSURFACEDESC* description, IDirectDrawSurface** surface, IUnknown* outer) override;
    HRESULT STDMETHODCALLTYPE DuplicateSurface(
        IDirectDrawSurface* source, IDirectDrawSurface** duplicate) override;
    HRESULT STDMETHODCALLTYPE EnumDisplayModes(
        DWORD flags, DDSURFACEDESC* description, void* context, LPDDENUMMODESCALLBACK callback) override;
    HRESULT STDMETHODCALLTYPE EnumSurfaces(
        DWORD flags, DDSURFACEDESC* description, void* context, LPDDENUMSURFACESCALLBACK callback) override;
    HRESULT STDMETHODCALLTYPE FlipToGDISurface() override;
    HRESULT STDMETHODCALLTYPE GetCaps(DDCAPS* driverCaps, DDCAPS* helCaps) override;
    HRESULT STDMETHODCALLTYPE GetDisplayMode(DDSURFACEDESC* description) override;
    HRESULT STDMETHODCALLTYPE GetFourCCCodes(DWORD* codeCount, DWORD* codes) override;
    HRESULT STDMETHODCALLTYPE GetGDISurface(IDirectDrawSurface** surface) override;
    HRESULT STDMETHODCALLTYPE GetMonitorFrequency(DWORD* frequency) override;
    HRESULT STDMETHODCALLTYPE GetScanLine(DWORD* scanLine) override;
    HRESULT STDMETHODCALLTYPE GetVerticalBlankStatus(BOOL* status) override;
    HRESULT STDMETHODCALLTYPE Initialize(GUID* guid) override;
    HRESULT STDMETHODCALLTYPE RestoreDisplayMode() override;
    HRESULT STDMETHODCALLTYPE SetCooperativeLevel(HWND window, DWORD flags) override;
    HRESULT STDMETHODCALLTYPE SetDisplayMode(DWORD width, DWORD height, DWORD bitsPerPixel) override;
    HRESULT STDMETHODCALLTYPE WaitForVerticalBlank(DWORD flags, HANDLE eventHandle) override;

private:
    ~DirectDrawProxy() = default;

    volatile LONG references_ = 1;
    IDirectDraw* real_ = nullptr;
};
}
