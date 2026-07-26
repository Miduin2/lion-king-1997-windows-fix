#include "ddraw_proxy.h"

#include "palette_proxy.h"
#include "surface_proxy.h"
#include "trace.h"

#include <new>

namespace
{
void TraceSurfaceDescription(const char* label, const DDSURFACEDESC* description)
{
    if (!description)
    {
        gamevaultdraw::Trace("%s: null", label);
        return;
    }

    gamevaultdraw::Trace(
        "%s: size=%lu flags=0x%08lX width=%lu height=%lu pitch=%ld "
        "caps=0x%08lX pfFlags=0x%08lX rgbBits=%lu",
        label,
        description->dwSize,
        description->dwFlags,
        description->dwWidth,
        description->dwHeight,
        description->lPitch,
        description->ddsCaps.dwCaps,
        description->ddpfPixelFormat.dwFlags,
        description->ddpfPixelFormat.dwRGBBitCount);
}

void TraceGuid(const char* label, const GUID& guid)
{
    gamevaultdraw::Trace(
        "%s={%08lX-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X}",
        label,
        guid.Data1,
        guid.Data2,
        guid.Data3,
        guid.Data4[0],
        guid.Data4[1],
        guid.Data4[2],
        guid.Data4[3],
        guid.Data4[4],
        guid.Data4[5],
        guid.Data4[6],
        guid.Data4[7]);
}
}

namespace gamevaultdraw
{
DirectDrawProxy::DirectDrawProxy(IDirectDraw* real) noexcept : real_(real)
{
    Trace("IDirectDraw wrapper created: real=%p wrapper=%p", real_, this);
}

HRESULT STDMETHODCALLTYPE DirectDrawProxy::QueryInterface(REFIID riid, void** object)
{
    if (!object)
    {
        return E_POINTER;
    }

    TraceGuid("IDirectDraw::QueryInterface", riid);
    if (riid == IID_IUnknown || riid == IID_IDirectDraw)
    {
        *object = static_cast<IDirectDraw*>(this);
        AddRef();
        TraceResult("IDirectDraw::QueryInterface(wrapper)", S_OK);
        return S_OK;
    }

    const HRESULT result = real_->QueryInterface(riid, object);
    TraceResult("IDirectDraw::QueryInterface(forwarded)", result);
    return result;
}

ULONG STDMETHODCALLTYPE DirectDrawProxy::AddRef()
{
    real_->AddRef();
    const LONG count = InterlockedIncrement(&references_);
    Trace("IDirectDraw::AddRef -> %ld", count);
    return static_cast<ULONG>(count);
}

ULONG STDMETHODCALLTYPE DirectDrawProxy::Release()
{
    const ULONG realCount = real_->Release();
    const LONG count = InterlockedDecrement(&references_);
    Trace("IDirectDraw::Release -> wrapper=%ld real=%lu", count, realCount);
    if (count == 0)
    {
        delete this;
        return 0;
    }
    return static_cast<ULONG>(count);
}

#define GV_FORWARD_NOARGS(method) \
    do \
    { \
        Trace("IDirectDraw::" #method); \
        const HRESULT result = real_->method(); \
        TraceResult("IDirectDraw::" #method, result); \
        return result; \
    } while (false)

HRESULT STDMETHODCALLTYPE DirectDrawProxy::Compact()
{
    GV_FORWARD_NOARGS(Compact);
}

HRESULT STDMETHODCALLTYPE DirectDrawProxy::CreateClipper(
    const DWORD flags, IDirectDrawClipper** clipper, IUnknown* outer)
{
    Trace("IDirectDraw::CreateClipper flags=0x%08lX outer=%p", flags, outer);
    const HRESULT result = real_->CreateClipper(flags, clipper, outer);
    Trace("IDirectDraw::CreateClipper -> HRESULT=0x%08lX object=%p",
        static_cast<unsigned long>(result), clipper ? *clipper : nullptr);
    return result;
}

HRESULT STDMETHODCALLTYPE DirectDrawProxy::CreatePalette(
    const DWORD flags, PALETTEENTRY* entries, IDirectDrawPalette** palette, IUnknown* outer)
{
    Trace("IDirectDraw::CreatePalette flags=0x%08lX entries=%p outer=%p", flags, entries, outer);
    const HRESULT result = real_->CreatePalette(flags, entries, palette, outer);
    Trace("IDirectDraw::CreatePalette -> HRESULT=0x%08lX object=%p",
        static_cast<unsigned long>(result), palette ? *palette : nullptr);
    if (SUCCEEDED(result) && palette && *palette)
    {
        const HRESULT wrapResult = WrapPalette(palette);
        TraceResult("IDirectDraw::CreatePalette wrap", wrapResult);
        return wrapResult;
    }
    return result;
}

HRESULT STDMETHODCALLTYPE DirectDrawProxy::CreateSurface(
    DDSURFACEDESC* description, IDirectDrawSurface** surface, IUnknown* outer)
{
    TraceSurfaceDescription("IDirectDraw::CreateSurface input", description);
    const HRESULT result = real_->CreateSurface(description, surface, outer);
    Trace("IDirectDraw::CreateSurface -> HRESULT=0x%08lX object=%p outer=%p",
        static_cast<unsigned long>(result), surface ? *surface : nullptr, outer);
    TraceSurfaceDescription("IDirectDraw::CreateSurface output", description);
    if (SUCCEEDED(result) && surface && *surface)
    {
        const bool isPrimary = description &&
            (description->ddsCaps.dwCaps & DDSCAPS_PRIMARYSURFACE) != 0;
        const HRESULT wrapResult = WrapSurface(surface, isPrimary);
        TraceResult("IDirectDraw::CreateSurface wrap", wrapResult);
        return wrapResult;
    }
    return result;
}

HRESULT STDMETHODCALLTYPE DirectDrawProxy::DuplicateSurface(
    IDirectDrawSurface* source, IDirectDrawSurface** duplicate)
{
    Trace("IDirectDraw::DuplicateSurface source=%p", source);
    const HRESULT result = real_->DuplicateSurface(source, duplicate);
    TraceResult("IDirectDraw::DuplicateSurface", result);
    return result;
}

HRESULT STDMETHODCALLTYPE DirectDrawProxy::EnumDisplayModes(
    const DWORD flags, DDSURFACEDESC* description, void* context, const LPDDENUMMODESCALLBACK callback)
{
    Trace("IDirectDraw::EnumDisplayModes flags=0x%08lX callback=%p", flags, callback);
    TraceSurfaceDescription("IDirectDraw::EnumDisplayModes filter", description);
    const HRESULT result = real_->EnumDisplayModes(flags, description, context, callback);
    TraceResult("IDirectDraw::EnumDisplayModes", result);
    return result;
}

HRESULT STDMETHODCALLTYPE DirectDrawProxy::EnumSurfaces(
    const DWORD flags, DDSURFACEDESC* description, void* context, const LPDDENUMSURFACESCALLBACK callback)
{
    Trace("IDirectDraw::EnumSurfaces flags=0x%08lX callback=%p", flags, callback);
    TraceSurfaceDescription("IDirectDraw::EnumSurfaces filter", description);
    const HRESULT result = real_->EnumSurfaces(flags, description, context, callback);
    TraceResult("IDirectDraw::EnumSurfaces", result);
    return result;
}

HRESULT STDMETHODCALLTYPE DirectDrawProxy::FlipToGDISurface()
{
    GV_FORWARD_NOARGS(FlipToGDISurface);
}

HRESULT STDMETHODCALLTYPE DirectDrawProxy::GetCaps(DDCAPS* driverCaps, DDCAPS* helCaps)
{
    Trace("IDirectDraw::GetCaps driver=%p hel=%p", driverCaps, helCaps);
    const HRESULT result = real_->GetCaps(driverCaps, helCaps);
    TraceResult("IDirectDraw::GetCaps", result);
    return result;
}

HRESULT STDMETHODCALLTYPE DirectDrawProxy::GetDisplayMode(DDSURFACEDESC* description)
{
    Trace("IDirectDraw::GetDisplayMode");
    const HRESULT result = real_->GetDisplayMode(description);
    TraceResult("IDirectDraw::GetDisplayMode", result);
    TraceSurfaceDescription("IDirectDraw::GetDisplayMode output", description);
    return result;
}

HRESULT STDMETHODCALLTYPE DirectDrawProxy::GetFourCCCodes(DWORD* codeCount, DWORD* codes)
{
    Trace("IDirectDraw::GetFourCCCodes count=%p codes=%p", codeCount, codes);
    const HRESULT result = real_->GetFourCCCodes(codeCount, codes);
    TraceResult("IDirectDraw::GetFourCCCodes", result);
    return result;
}

HRESULT STDMETHODCALLTYPE DirectDrawProxy::GetGDISurface(IDirectDrawSurface** surface)
{
    Trace("IDirectDraw::GetGDISurface");
    const HRESULT result = real_->GetGDISurface(surface);
    Trace("IDirectDraw::GetGDISurface -> HRESULT=0x%08lX object=%p",
        static_cast<unsigned long>(result), surface ? *surface : nullptr);
    return result;
}

HRESULT STDMETHODCALLTYPE DirectDrawProxy::GetMonitorFrequency(DWORD* frequency)
{
    const HRESULT result = real_->GetMonitorFrequency(frequency);
    Trace("IDirectDraw::GetMonitorFrequency -> HRESULT=0x%08lX value=%lu",
        static_cast<unsigned long>(result), frequency ? *frequency : 0UL);
    return result;
}

HRESULT STDMETHODCALLTYPE DirectDrawProxy::GetScanLine(DWORD* scanLine)
{
    const HRESULT result = real_->GetScanLine(scanLine);
    Trace("IDirectDraw::GetScanLine -> HRESULT=0x%08lX value=%lu",
        static_cast<unsigned long>(result), scanLine ? *scanLine : 0UL);
    return result;
}

HRESULT STDMETHODCALLTYPE DirectDrawProxy::GetVerticalBlankStatus(BOOL* status)
{
    const HRESULT result = real_->GetVerticalBlankStatus(status);
    Trace("IDirectDraw::GetVerticalBlankStatus -> HRESULT=0x%08lX value=%d",
        static_cast<unsigned long>(result), status ? *status : FALSE);
    return result;
}

HRESULT STDMETHODCALLTYPE DirectDrawProxy::Initialize(GUID* guid)
{
    if (guid)
    {
        TraceGuid("IDirectDraw::Initialize", *guid);
    }
    else
    {
        Trace("IDirectDraw::Initialize guid=null");
    }
    const HRESULT result = real_->Initialize(guid);
    TraceResult("IDirectDraw::Initialize", result);
    return result;
}

HRESULT STDMETHODCALLTYPE DirectDrawProxy::RestoreDisplayMode()
{
    GV_FORWARD_NOARGS(RestoreDisplayMode);
}

HRESULT STDMETHODCALLTYPE DirectDrawProxy::SetCooperativeLevel(const HWND window, const DWORD flags)
{
    Trace("IDirectDraw::SetCooperativeLevel hwnd=%p flags=0x%08lX", window, flags);
    const HRESULT result = real_->SetCooperativeLevel(window, flags);
    TraceResult("IDirectDraw::SetCooperativeLevel", result);
    if (SUCCEEDED(result))
    {
        SetPresentationWindow(window);
    }
    return result;
}

HRESULT STDMETHODCALLTYPE DirectDrawProxy::SetDisplayMode(
    const DWORD width, const DWORD height, const DWORD bitsPerPixel)
{
    Trace("IDirectDraw::SetDisplayMode width=%lu height=%lu bpp=%lu",
        width, height, bitsPerPixel);
    const HRESULT result = real_->SetDisplayMode(width, height, bitsPerPixel);
    TraceResult("IDirectDraw::SetDisplayMode", result);
    return result;
}

HRESULT STDMETHODCALLTYPE DirectDrawProxy::WaitForVerticalBlank(const DWORD flags, const HANDLE eventHandle)
{
    Trace("IDirectDraw::WaitForVerticalBlank flags=0x%08lX event=%p", flags, eventHandle);
    const HRESULT result = real_->WaitForVerticalBlank(flags, eventHandle);
    TraceResult("IDirectDraw::WaitForVerticalBlank", result);
    return result;
}

#undef GV_FORWARD_NOARGS
}
