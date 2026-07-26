#include "palette_proxy.h"

#include "ddraw_proxy.h"
#include "trace.h"

#include <new>

namespace gamevaultdraw
{
PaletteProxy::PaletteProxy(IDirectDrawPalette* real) noexcept : real_(real)
{
    Trace("IDirectDrawPalette wrapper created: real=%p wrapper=%p", real_, this);
}

HRESULT STDMETHODCALLTYPE PaletteProxy::QueryInterface(REFIID riid, void** object)
{
    if (!object)
    {
        return E_POINTER;
    }
    if (riid == IID_IUnknown || riid == IID_IDirectDrawPalette)
    {
        *object = static_cast<IDirectDrawPalette*>(this);
        AddRef();
        TraceResult("IDirectDrawPalette::QueryInterface(wrapper)", S_OK);
        return S_OK;
    }
    const HRESULT result = real_->QueryInterface(riid, object);
    TraceResult("IDirectDrawPalette::QueryInterface(forwarded)", result);
    return result;
}

ULONG STDMETHODCALLTYPE PaletteProxy::AddRef()
{
    real_->AddRef();
    const LONG count = InterlockedIncrement(&references_);
    Trace("IDirectDrawPalette::AddRef -> %ld", count);
    return static_cast<ULONG>(count);
}

ULONG STDMETHODCALLTYPE PaletteProxy::Release()
{
    const ULONG realCount = real_->Release();
    const LONG count = InterlockedDecrement(&references_);
    Trace("IDirectDrawPalette::Release -> wrapper=%ld real=%lu", count, realCount);
    if (count == 0)
    {
        delete this;
        return 0;
    }
    return static_cast<ULONG>(count);
}

HRESULT STDMETHODCALLTYPE PaletteProxy::GetCaps(DWORD* caps)
{
    const HRESULT result = real_->GetCaps(caps);
    Trace("IDirectDrawPalette::GetCaps -> HRESULT=0x%08lX caps=0x%08lX",
        static_cast<unsigned long>(result), caps ? *caps : 0UL);
    return result;
}

HRESULT STDMETHODCALLTYPE PaletteProxy::GetEntries(
    const DWORD flags, const DWORD base, const DWORD count, PALETTEENTRY* entries)
{
    Trace("IDirectDrawPalette::GetEntries flags=0x%08lX base=%lu count=%lu entries=%p",
        flags, base, count, entries);
    const HRESULT result = real_->GetEntries(flags, base, count, entries);
    TraceResult("IDirectDrawPalette::GetEntries", result);
    return result;
}

HRESULT STDMETHODCALLTYPE PaletteProxy::Initialize(
    IDirectDraw* directDraw, const DWORD flags, PALETTEENTRY* entries)
{
    if (auto* proxy = dynamic_cast<DirectDrawProxy*>(directDraw))
    {
        directDraw = proxy->Real();
    }
    Trace("IDirectDrawPalette::Initialize ddraw=%p flags=0x%08lX entries=%p",
        directDraw, flags, entries);
    const HRESULT result = real_->Initialize(directDraw, flags, entries);
    TraceResult("IDirectDrawPalette::Initialize", result);
    return result;
}

HRESULT STDMETHODCALLTYPE PaletteProxy::SetEntries(
    const DWORD flags, const DWORD base, const DWORD count, PALETTEENTRY* entries)
{
    Trace("IDirectDrawPalette::SetEntries flags=0x%08lX base=%lu count=%lu entries=%p",
        flags, base, count, entries);
    const HRESULT result = real_->SetEntries(flags, base, count, entries);
    TraceResult("IDirectDrawPalette::SetEntries", result);
    return result;
}

HRESULT WrapPalette(IDirectDrawPalette** palette)
{
    if (!palette || !*palette)
    {
        return E_POINTER;
    }
    auto* proxy = new (std::nothrow) PaletteProxy(*palette);
    if (!proxy)
    {
        (*palette)->Release();
        *palette = nullptr;
        return E_OUTOFMEMORY;
    }
    *palette = proxy;
    return S_OK;
}

IDirectDrawPalette* UnwrapPalette(IDirectDrawPalette* palette) noexcept
{
    if (auto* proxy = dynamic_cast<PaletteProxy*>(palette))
    {
        return proxy->Real();
    }
    return palette;
}
}

