#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <ddraw.h>

namespace gamevaultdraw
{
class PaletteProxy final : public IDirectDrawPalette
{
public:
    explicit PaletteProxy(IDirectDrawPalette* real) noexcept;
    IDirectDrawPalette* Real() const noexcept { return real_; }

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** object) override;
    ULONG STDMETHODCALLTYPE AddRef() override;
    ULONG STDMETHODCALLTYPE Release() override;
    HRESULT STDMETHODCALLTYPE GetCaps(DWORD* caps) override;
    HRESULT STDMETHODCALLTYPE GetEntries(
        DWORD flags, DWORD base, DWORD count, PALETTEENTRY* entries) override;
    HRESULT STDMETHODCALLTYPE Initialize(
        IDirectDraw* directDraw, DWORD flags, PALETTEENTRY* entries) override;
    HRESULT STDMETHODCALLTYPE SetEntries(
        DWORD flags, DWORD base, DWORD count, PALETTEENTRY* entries) override;

private:
    ~PaletteProxy() = default;

    volatile LONG references_ = 1;
    IDirectDrawPalette* real_ = nullptr;
};

HRESULT WrapPalette(IDirectDrawPalette** palette);
IDirectDrawPalette* UnwrapPalette(IDirectDrawPalette* palette) noexcept;
}

