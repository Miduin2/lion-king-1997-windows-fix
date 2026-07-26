#include "surface_proxy.h"

#include "ddraw_proxy.h"
#include "dwm_preview.h"
#include "palette_proxy.h"
#include "trace.h"

#include <algorithm>
#include <intrin.h>
#include <new>
#include <string>

extern "C" IMAGE_DOS_HEADER __ImageBase;

namespace
{
INIT_ONCE g_presentationInitialization = INIT_ONCE_STATIC_INIT;
HWND g_presentationWindow = nullptr;
bool g_fullscreen = true;
bool g_windowPrepared = false;
int g_aspectWidth = 4;
int g_aspectHeight = 3;
int g_frameLimit = 60;
RECT g_monitorRect = {};
LARGE_INTEGER g_performanceFrequency = {};
LONGLONG g_nextFrame = 0;

std::wstring GetConfigurationPath()
{
    wchar_t modulePath[MAX_PATH] = {};
    const DWORD length = GetModuleFileNameW(
        reinterpret_cast<HMODULE>(&__ImageBase), modulePath, MAX_PATH);
    if (length == 0 || length >= MAX_PATH)
    {
        return L"GameVaultDraw.ini";
    }

    std::wstring path(modulePath, length);
    const auto separator = path.find_last_of(L"\\/");
    if (separator != std::wstring::npos)
    {
        path.resize(separator + 1);
    }
    else
    {
        path.clear();
    }
    path += L"GameVaultDraw.ini";
    return path;
}

BOOL CALLBACK LoadPresentationConfiguration(PINIT_ONCE, PVOID, PVOID*)
{
    const std::wstring path = GetConfigurationPath();
    g_fullscreen = GetPrivateProfileIntW(L"Display", L"Fullscreen", 1, path.c_str()) != 0;
    g_aspectWidth = (std::max<int>)(1, static_cast<int>(GetPrivateProfileIntW(
        L"Display", L"AspectWidth", 4, path.c_str())));
    g_aspectHeight = (std::max<int>)(1, static_cast<int>(GetPrivateProfileIntW(
        L"Display", L"AspectHeight", 3, path.c_str())));
    g_frameLimit = (std::max<int>)(0, static_cast<int>(GetPrivateProfileIntW(
        L"Timing", L"FrameLimit", 60, path.c_str())));
    QueryPerformanceFrequency(&g_performanceFrequency);
    gamevaultdraw::Trace(
        "Presentation configuration: fullscreen=%d aspect=%d:%d frameLimit=%d ini=%ls",
        g_fullscreen ? 1 : 0, g_aspectWidth, g_aspectHeight, g_frameLimit, path.c_str());
    return TRUE;
}

void EnsurePresentationConfiguration()
{
    InitOnceExecuteOnce(
        &g_presentationInitialization, LoadPresentationConfiguration, nullptr, nullptr);
}

bool EnsureFullscreenWindow()
{
    EnsurePresentationConfiguration();
    if (!g_fullscreen || !g_presentationWindow || !IsWindow(g_presentationWindow))
    {
        return false;
    }
    if (g_windowPrepared)
    {
        return true;
    }

    MONITORINFO monitorInfo = {};
    monitorInfo.cbSize = sizeof(monitorInfo);
    const HMONITOR monitor = MonitorFromWindow(
        g_presentationWindow, MONITOR_DEFAULTTONEAREST);
    if (!monitor || !GetMonitorInfoW(monitor, &monitorInfo))
    {
        gamevaultdraw::Trace("Fullscreen preparation failed: monitor error=%lu", GetLastError());
        return false;
    }
    g_monitorRect = monitorInfo.rcMonitor;

    // Force the Win9x window to be a modern top-level application window.
    // Borderless WS_POPUP windows are otherwise easy for the shell to omit
    // from both the taskbar and Alt+Tab, especially when they retain an owner.
    ShowWindow(g_presentationWindow, SW_HIDE);
    const HMENU nativeMenu = GetMenu(g_presentationWindow);
    gamevaultdraw::Trace(
        "Native application menu before borderless conversion: menu=%p items=%d",
        nativeMenu, nativeMenu ? GetMenuItemCount(nativeMenu) : 0);
    SetMenu(g_presentationWindow, nullptr);
    SetWindowLongPtrW(g_presentationWindow, GWLP_HWNDPARENT, 0);
    LONG_PTR style = GetWindowLongPtrW(g_presentationWindow, GWL_STYLE);
    style &= ~(WS_CAPTION | WS_THICKFRAME | WS_MINIMIZEBOX |
        WS_MAXIMIZEBOX | WS_SYSMENU | WS_CHILD);
    style |= WS_POPUP | WS_VISIBLE;
    SetWindowLongPtrW(g_presentationWindow, GWL_STYLE, style);

    LONG_PTR extendedStyle = GetWindowLongPtrW(g_presentationWindow, GWL_EXSTYLE);
    extendedStyle &= ~(WS_EX_DLGMODALFRAME | WS_EX_CLIENTEDGE |
        WS_EX_STATICEDGE | WS_EX_WINDOWEDGE | WS_EX_TOOLWINDOW |
        WS_EX_NOACTIVATE);
    extendedStyle |= WS_EX_APPWINDOW;
    SetWindowLongPtrW(g_presentationWindow, GWL_EXSTYLE, extendedStyle);

    const int width = g_monitorRect.right - g_monitorRect.left;
    const int height = g_monitorRect.bottom - g_monitorRect.top;
    if (!SetWindowPos(
            g_presentationWindow, HWND_TOP,
            g_monitorRect.left, g_monitorRect.top, width, height,
            SWP_FRAMECHANGED | SWP_NOOWNERZORDER | SWP_SHOWWINDOW))
    {
        gamevaultdraw::Trace("Fullscreen preparation failed: SetWindowPos error=%lu", GetLastError());
        return false;
    }

    if (const HDC dc = GetDC(g_presentationWindow))
    {
        RECT client = {};
        GetClientRect(g_presentationWindow, &client);
        FillRect(dc, &client, static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)));
        ReleaseDC(g_presentationWindow, dc);
    }

    g_windowPrepared = true;
    gamevaultdraw::Trace(
        "Borderless fullscreen prepared: monitor=[%ld,%ld,%ld,%ld] style=0x%p exStyle=0x%p owner=%p",
        g_monitorRect.left, g_monitorRect.top, g_monitorRect.right, g_monitorRect.bottom,
        reinterpret_cast<void*>(style), reinterpret_cast<void*>(extendedStyle),
        GetWindow(g_presentationWindow, GW_OWNER));
    return true;
}

RECT CalculatePresentationRectangle()
{
    const LONG monitorWidth = g_monitorRect.right - g_monitorRect.left;
    const LONG monitorHeight = g_monitorRect.bottom - g_monitorRect.top;
    LONG width = monitorWidth;
    LONG height = static_cast<LONG>(
        static_cast<long long>(width) * g_aspectHeight / g_aspectWidth);
    if (height > monitorHeight)
    {
        height = monitorHeight;
        width = static_cast<LONG>(
            static_cast<long long>(height) * g_aspectWidth / g_aspectHeight);
    }

    RECT result = {};
    result.left = g_monitorRect.left + (monitorWidth - width) / 2;
    result.top = g_monitorRect.top + (monitorHeight - height) / 2;
    result.right = result.left + width;
    result.bottom = result.top + height;
    return result;
}

void LimitPresentationRate()
{
    EnsurePresentationConfiguration();
    if (g_frameLimit <= 0 || g_performanceFrequency.QuadPart <= 0)
    {
        return;
    }

    LARGE_INTEGER now = {};
    QueryPerformanceCounter(&now);
    const LONGLONG interval = (std::max<LONGLONG>)(
        1, g_performanceFrequency.QuadPart / g_frameLimit);
    if (g_nextFrame == 0 || now.QuadPart > g_nextFrame + interval * 4)
    {
        g_nextFrame = now.QuadPart;
    }

    while (now.QuadPart < g_nextFrame)
    {
        const LONGLONG remaining = g_nextFrame - now.QuadPart;
        const DWORD milliseconds = static_cast<DWORD>(
            remaining * 1000 / g_performanceFrequency.QuadPart);
        if (milliseconds > 1)
        {
            Sleep(milliseconds - 1);
        }
        else
        {
            YieldProcessor();
        }
        QueryPerformanceCounter(&now);
    }
    g_nextFrame += interval;
}

void TraceRect(const char* label, const RECT* rect)
{
    if (rect)
    {
        gamevaultdraw::Trace("%s=[%ld,%ld,%ld,%ld]", label,
            rect->left, rect->top, rect->right, rect->bottom);
    }
}

void TraceSurfaceDescription(const char* label, const DDSURFACEDESC* description)
{
    if (!description)
    {
        gamevaultdraw::Trace("%s: null", label);
        return;
    }
    gamevaultdraw::Trace(
        "%s: flags=0x%08lX width=%lu height=%lu pitch=%ld surface=%p "
        "caps=0x%08lX pfFlags=0x%08lX rgbBits=%lu",
        label,
        description->dwFlags,
        description->dwWidth,
        description->dwHeight,
        description->lPitch,
        description->lpSurface,
        description->ddsCaps.dwCaps,
        description->ddpfPixelFormat.dwFlags,
        description->ddpfPixelFormat.dwRGBBitCount);
}
}

namespace gamevaultdraw
{
SurfaceProxy::SurfaceProxy(IDirectDrawSurface* real, const bool isPrimary) noexcept
    : real_(real), isPrimary_(isPrimary)
{
    Trace("IDirectDrawSurface wrapper created: real=%p wrapper=%p primary=%d",
        real_, this, isPrimary_ ? 1 : 0);
}

SurfaceProxy::~SurfaceProxy()
{
    if (virtualPalette_)
    {
        virtualPalette_->Release();
        virtualPalette_ = nullptr;
    }
}

HRESULT STDMETHODCALLTYPE SurfaceProxy::QueryInterface(REFIID riid, void** object)
{
    if (!object)
    {
        return E_POINTER;
    }
    if (riid == IID_IUnknown || riid == IID_IDirectDrawSurface)
    {
        *object = static_cast<IDirectDrawSurface*>(this);
        AddRef();
        TraceResult("IDirectDrawSurface::QueryInterface(wrapper)", S_OK);
        return S_OK;
    }
    const HRESULT result = real_->QueryInterface(riid, object);
    TraceResult("IDirectDrawSurface::QueryInterface(forwarded)", result);
    return result;
}

ULONG STDMETHODCALLTYPE SurfaceProxy::AddRef()
{
    real_->AddRef();
    const LONG count = InterlockedIncrement(&references_);
    Trace("IDirectDrawSurface::AddRef -> %ld", count);
    return static_cast<ULONG>(count);
}

ULONG STDMETHODCALLTYPE SurfaceProxy::Release()
{
    const ULONG realCount = real_->Release();
    const LONG count = InterlockedDecrement(&references_);
    Trace("IDirectDrawSurface::Release -> wrapper=%ld real=%lu", count, realCount);
    if (count == 0)
    {
        delete this;
        return 0;
    }
    return static_cast<ULONG>(count);
}

HRESULT STDMETHODCALLTYPE SurfaceProxy::AddAttachedSurface(IDirectDrawSurface* surface)
{
    Trace("IDirectDrawSurface::AddAttachedSurface surface=%p", surface);
    const HRESULT result = real_->AddAttachedSurface(UnwrapSurface(surface));
    TraceResult("IDirectDrawSurface::AddAttachedSurface", result);
    return result;
}

HRESULT STDMETHODCALLTYPE SurfaceProxy::AddOverlayDirtyRect(RECT* rect)
{
    TraceRect("IDirectDrawSurface::AddOverlayDirtyRect", rect);
    const HRESULT result = real_->AddOverlayDirtyRect(rect);
    TraceResult("IDirectDrawSurface::AddOverlayDirtyRect", result);
    return result;
}

HRESULT STDMETHODCALLTYPE SurfaceProxy::Blt(
    RECT* destinationRect, IDirectDrawSurface* source, RECT* sourceRect,
    const DWORD flags, DDBLTFX* effects)
{
    const LONG callNumber = InterlockedIncrement(&bltCalls_);
    RECT repairedDestination{};
    RECT* forwardedDestination = destinationRect;
    bool repaired = false;

    if (isPrimary_)
    {
        LimitPresentationRate();
        if (EnsureFullscreenWindow())
        {
            repairedDestination = CalculatePresentationRectangle();
            forwardedDestination = &repairedDestination;
            repaired = true;
        }
    }

    // Some Win9x-era titles leave RECT::bottom uninitialised when blitting a
    // client-sized frame to the primary surface. Modern DirectDraw rejects
    // that with DDERR_INVALIDRECT. The valid source rectangle tells us the
    // intended height without assuming a game or desktop resolution.
    if (!repaired && destinationRect && sourceRect &&
        destinationRect->right > destinationRect->left &&
        sourceRect->right > sourceRect->left &&
        sourceRect->bottom > sourceRect->top &&
        destinationRect->bottom <= destinationRect->top)
    {
        repairedDestination = *destinationRect;
        repairedDestination.bottom = repairedDestination.top +
            (sourceRect->bottom - sourceRect->top);
        forwardedDestination = &repairedDestination;
        repaired = true;
    }

    const bool traceCall = callNumber <= 8;
    if (traceCall)
    {
        Trace("IDirectDrawSurface::Blt #%ld this=%p source=%p flags=0x%08lX effects=%p",
            callNumber, this, source, flags, effects);
        TraceRect("  destination(input)", destinationRect);
        TraceRect("  source", sourceRect);
        if (repaired)
        {
            TraceRect("  destination(repaired)", forwardedDestination);
        }
    }
    const HRESULT result = real_->Blt(
        forwardedDestination, UnwrapSurface(source), sourceRect, flags, effects);
    if (traceCall)
    {
        TraceResult("IDirectDrawSurface::Blt", result);
    }
    return result;
}

HRESULT STDMETHODCALLTYPE SurfaceProxy::BltBatch(
    DDBLTBATCH* batch, const DWORD count, const DWORD flags)
{
    Trace("IDirectDrawSurface::BltBatch batch=%p count=%lu flags=0x%08lX",
        batch, count, flags);
    const HRESULT result = real_->BltBatch(batch, count, flags);
    TraceResult("IDirectDrawSurface::BltBatch", result);
    return result;
}

HRESULT STDMETHODCALLTYPE SurfaceProxy::BltFast(
    const DWORD x, const DWORD y, IDirectDrawSurface* source, RECT* sourceRect,
    const DWORD flags)
{
    Trace("IDirectDrawSurface::BltFast x=%lu y=%lu source=%p flags=0x%08lX",
        x, y, source, flags);
    TraceRect("  source", sourceRect);
    const HRESULT result = real_->BltFast(x, y, UnwrapSurface(source), sourceRect, flags);
    TraceResult("IDirectDrawSurface::BltFast", result);
    return result;
}

HRESULT STDMETHODCALLTYPE SurfaceProxy::DeleteAttachedSurface(
    const DWORD flags, IDirectDrawSurface* surface)
{
    Trace("IDirectDrawSurface::DeleteAttachedSurface flags=0x%08lX surface=%p", flags, surface);
    const HRESULT result = real_->DeleteAttachedSurface(flags, UnwrapSurface(surface));
    TraceResult("IDirectDrawSurface::DeleteAttachedSurface", result);
    return result;
}

HRESULT STDMETHODCALLTYPE SurfaceProxy::EnumAttachedSurfaces(
    void* context, const LPDDENUMSURFACESCALLBACK callback)
{
    Trace("IDirectDrawSurface::EnumAttachedSurfaces callback=%p", callback);
    const HRESULT result = real_->EnumAttachedSurfaces(context, callback);
    TraceResult("IDirectDrawSurface::EnumAttachedSurfaces", result);
    return result;
}

HRESULT STDMETHODCALLTYPE SurfaceProxy::EnumOverlayZOrders(
    const DWORD flags, void* context, const LPDDENUMSURFACESCALLBACK callback)
{
    Trace("IDirectDrawSurface::EnumOverlayZOrders flags=0x%08lX callback=%p", flags, callback);
    const HRESULT result = real_->EnumOverlayZOrders(flags, context, callback);
    TraceResult("IDirectDrawSurface::EnumOverlayZOrders", result);
    return result;
}

HRESULT STDMETHODCALLTYPE SurfaceProxy::Flip(IDirectDrawSurface* targetOverride, const DWORD flags)
{
    Trace("IDirectDrawSurface::Flip target=%p flags=0x%08lX", targetOverride, flags);
    const HRESULT result = real_->Flip(UnwrapSurface(targetOverride), flags);
    TraceResult("IDirectDrawSurface::Flip", result);
    return result;
}

HRESULT STDMETHODCALLTYPE SurfaceProxy::GetAttachedSurface(
    DDSCAPS* caps, IDirectDrawSurface** surface)
{
    Trace("IDirectDrawSurface::GetAttachedSurface caps=0x%08lX",
        caps ? caps->dwCaps : 0UL);
    const HRESULT result = real_->GetAttachedSurface(caps, surface);
    TraceResult("IDirectDrawSurface::GetAttachedSurface", result);
    if (SUCCEEDED(result) && surface && *surface)
    {
        return WrapSurface(surface);
    }
    return result;
}

#define GV_SURFACE_FORWARD_FLAGS(method) \
    Trace("IDirectDrawSurface::" #method " flags=0x%08lX", flags); \
    const HRESULT result = real_->method(flags); \
    TraceResult("IDirectDrawSurface::" #method, result); \
    return result

HRESULT STDMETHODCALLTYPE SurfaceProxy::GetBltStatus(const DWORD flags)
{
    GV_SURFACE_FORWARD_FLAGS(GetBltStatus);
}

HRESULT STDMETHODCALLTYPE SurfaceProxy::GetCaps(DDSCAPS* caps)
{
    const HRESULT result = real_->GetCaps(caps);
    Trace("IDirectDrawSurface::GetCaps -> HRESULT=0x%08lX caps=0x%08lX",
        static_cast<unsigned long>(result), caps ? caps->dwCaps : 0UL);
    return result;
}

HRESULT STDMETHODCALLTYPE SurfaceProxy::GetClipper(IDirectDrawClipper** clipper)
{
    const HRESULT result = real_->GetClipper(clipper);
    Trace("IDirectDrawSurface::GetClipper -> HRESULT=0x%08lX object=%p",
        static_cast<unsigned long>(result), clipper ? *clipper : nullptr);
    return result;
}

HRESULT STDMETHODCALLTYPE SurfaceProxy::GetColorKey(const DWORD flags, DDCOLORKEY* key)
{
    Trace("IDirectDrawSurface::GetColorKey flags=0x%08lX", flags);
    const HRESULT result = real_->GetColorKey(flags, key);
    TraceResult("IDirectDrawSurface::GetColorKey", result);
    return result;
}

HRESULT STDMETHODCALLTYPE SurfaceProxy::GetDC(HDC* dc)
{
    const HRESULT result = real_->GetDC(dc);
    Trace("IDirectDrawSurface::GetDC -> HRESULT=0x%08lX dc=%p",
        static_cast<unsigned long>(result), dc ? *dc : nullptr);
    return result;
}

HRESULT STDMETHODCALLTYPE SurfaceProxy::GetFlipStatus(const DWORD flags)
{
    GV_SURFACE_FORWARD_FLAGS(GetFlipStatus);
}

HRESULT STDMETHODCALLTYPE SurfaceProxy::GetOverlayPosition(LONG* x, LONG* y)
{
    const HRESULT result = real_->GetOverlayPosition(x, y);
    Trace("IDirectDrawSurface::GetOverlayPosition -> HRESULT=0x%08lX x=%ld y=%ld",
        static_cast<unsigned long>(result), x ? *x : 0L, y ? *y : 0L);
    return result;
}

HRESULT STDMETHODCALLTYPE SurfaceProxy::GetPalette(IDirectDrawPalette** palette)
{
    if (!palette)
    {
        return DDERR_INVALIDPARAMS;
    }
    if (virtualPalette_)
    {
        *palette = virtualPalette_;
        virtualPalette_->AddRef();
        Trace("IDirectDrawSurface::GetPalette(virtual) this=%p object=%p", this, *palette);
        return DD_OK;
    }

    const HRESULT result = real_->GetPalette(palette);
    Trace("IDirectDrawSurface::GetPalette -> HRESULT=0x%08lX object=%p",
        static_cast<unsigned long>(result), palette ? *palette : nullptr);
    if (SUCCEEDED(result) && palette && *palette)
    {
        return WrapPalette(palette);
    }
    return result;
}

HRESULT STDMETHODCALLTYPE SurfaceProxy::GetPixelFormat(DDPIXELFORMAT* format)
{
    const HRESULT result = real_->GetPixelFormat(format);
    Trace("IDirectDrawSurface::GetPixelFormat -> HRESULT=0x%08lX flags=0x%08lX bits=%lu",
        static_cast<unsigned long>(result), format ? format->dwFlags : 0UL,
        format ? format->dwRGBBitCount : 0UL);
    return result;
}

HRESULT STDMETHODCALLTYPE SurfaceProxy::GetSurfaceDesc(DDSURFACEDESC* description)
{
    const HRESULT result = real_->GetSurfaceDesc(description);
    TraceResult("IDirectDrawSurface::GetSurfaceDesc", result);
    TraceSurfaceDescription("IDirectDrawSurface::GetSurfaceDesc output", description);
    return result;
}

HRESULT STDMETHODCALLTYPE SurfaceProxy::Initialize(
    IDirectDraw* directDraw, DDSURFACEDESC* description)
{
    if (auto* proxy = dynamic_cast<DirectDrawProxy*>(directDraw))
    {
        directDraw = proxy->Real();
    }
    TraceSurfaceDescription("IDirectDrawSurface::Initialize", description);
    const HRESULT result = real_->Initialize(directDraw, description);
    TraceResult("IDirectDrawSurface::Initialize", result);
    return result;
}

HRESULT STDMETHODCALLTYPE SurfaceProxy::IsLost()
{
    const HRESULT result = real_->IsLost();
    TraceResult("IDirectDrawSurface::IsLost", result);
    return result;
}

HRESULT STDMETHODCALLTYPE SurfaceProxy::Lock(
    RECT* rect, DDSURFACEDESC* description, const DWORD flags, const HANDLE eventHandle)
{
    const LONG callNumber = InterlockedIncrement(&lockCalls_);
    const bool traceCall = callNumber <= 8;
    if (traceCall)
    {
        Trace("IDirectDrawSurface::Lock #%ld this=%p flags=0x%08lX event=%p",
            callNumber, this, flags, eventHandle);
        TraceRect("  rect", rect);
    }
    const HRESULT result = real_->Lock(rect, description, flags, eventHandle);
    if (traceCall)
    {
        TraceResult("IDirectDrawSurface::Lock", result);
        TraceSurfaceDescription("IDirectDrawSurface::Lock output", description);
    }
    if (SUCCEEDED(result) && !isPrimary_ && description && description->lpSurface &&
        description->ddpfPixelFormat.dwRGBBitCount == 32)
    {
        lockedPixels_ = description->lpSurface;
        lockedPitch_ = description->lPitch;
        lockedWidth_ = description->dwWidth;
        lockedHeight_ = description->dwHeight > 200 ? 200 : description->dwHeight;
        lockedBitsPerPixel_ = description->ddpfPixelFormat.dwRGBBitCount;
    }
    return result;
}

HRESULT STDMETHODCALLTYPE SurfaceProxy::ReleaseDC(const HDC dc)
{
    Trace("IDirectDrawSurface::ReleaseDC dc=%p", dc);
    const HRESULT result = real_->ReleaseDC(dc);
    TraceResult("IDirectDrawSurface::ReleaseDC", result);
    return result;
}

HRESULT STDMETHODCALLTYPE SurfaceProxy::Restore()
{
    const HRESULT result = real_->Restore();
    TraceResult("IDirectDrawSurface::Restore", result);
    return result;
}

HRESULT STDMETHODCALLTYPE SurfaceProxy::SetClipper(IDirectDrawClipper* clipper)
{
    Trace("IDirectDrawSurface::SetClipper clipper=%p", clipper);
    const HRESULT result = real_->SetClipper(clipper);
    TraceResult("IDirectDrawSurface::SetClipper", result);
    return result;
}

HRESULT STDMETHODCALLTYPE SurfaceProxy::SetColorKey(const DWORD flags, DDCOLORKEY* key)
{
    Trace("IDirectDrawSurface::SetColorKey flags=0x%08lX", flags);
    const HRESULT result = real_->SetColorKey(flags, key);
    TraceResult("IDirectDrawSurface::SetColorKey", result);
    return result;
}

HRESULT STDMETHODCALLTYPE SurfaceProxy::SetOverlayPosition(const LONG x, const LONG y)
{
    Trace("IDirectDrawSurface::SetOverlayPosition x=%ld y=%ld", x, y);
    const HRESULT result = real_->SetOverlayPosition(x, y);
    TraceResult("IDirectDrawSurface::SetOverlayPosition", result);
    return result;
}

HRESULT STDMETHODCALLTYPE SurfaceProxy::SetPalette(IDirectDrawPalette* palette)
{
    Trace("IDirectDrawSurface::SetPalette this=%p palette=%p", this, palette);
    const HRESULT result = real_->SetPalette(UnwrapPalette(palette));
    TraceResult("IDirectDrawSurface::SetPalette", result);

    // A true-colour primary surface cannot accept a DirectDraw palette on
    // current Windows, while some Win9x games still require the logical
    // association to succeed. Preserve that COM association virtually; the
    // game's already-converted 32-bit pixels continue through native DDraw.
    if (result == DDERR_INVALIDPIXELFORMAT)
    {
        if (palette)
        {
            palette->AddRef();
        }
        if (virtualPalette_)
        {
            virtualPalette_->Release();
        }
        virtualPalette_ = palette;
        TraceResult("IDirectDrawSurface::SetPalette(virtual)", DD_OK);
        return DD_OK;
    }

    if (SUCCEEDED(result) && virtualPalette_)
    {
        virtualPalette_->Release();
        virtualPalette_ = nullptr;
    }
    return result;
}

HRESULT STDMETHODCALLTYPE SurfaceProxy::Unlock(void* surfaceData)
{
    const LONG callNumber = InterlockedIncrement(&unlockCalls_);
    const bool traceCall = callNumber <= 8;
    if (traceCall)
    {
        Trace("IDirectDrawSurface::Unlock #%ld this=%p surface=%p",
            callNumber, this, surfaceData);
    }
    if (lockedPixels_)
    {
        UpdateDwmPreview(
            lockedPixels_, lockedPitch_, lockedWidth_, lockedHeight_,
            lockedBitsPerPixel_);
    }
    lockedPixels_ = nullptr;
    lockedPitch_ = 0;
    lockedWidth_ = 0;
    lockedHeight_ = 0;
    lockedBitsPerPixel_ = 0;

    const HRESULT result = real_->Unlock(surfaceData);
    if (traceCall)
    {
        TraceResult("IDirectDrawSurface::Unlock", result);
    }
    return result;
}

HRESULT STDMETHODCALLTYPE SurfaceProxy::UpdateOverlay(
    RECT* sourceRect, IDirectDrawSurface* destination, RECT* destinationRect,
    const DWORD flags, DDOVERLAYFX* effects)
{
    Trace("IDirectDrawSurface::UpdateOverlay destination=%p flags=0x%08lX effects=%p",
        destination, flags, effects);
    const HRESULT result = real_->UpdateOverlay(
        sourceRect, UnwrapSurface(destination), destinationRect, flags, effects);
    TraceResult("IDirectDrawSurface::UpdateOverlay", result);
    return result;
}

HRESULT STDMETHODCALLTYPE SurfaceProxy::UpdateOverlayDisplay(const DWORD flags)
{
    GV_SURFACE_FORWARD_FLAGS(UpdateOverlayDisplay);
}

HRESULT STDMETHODCALLTYPE SurfaceProxy::UpdateOverlayZOrder(
    const DWORD flags, IDirectDrawSurface* reference)
{
    Trace("IDirectDrawSurface::UpdateOverlayZOrder flags=0x%08lX reference=%p",
        flags, reference);
    const HRESULT result = real_->UpdateOverlayZOrder(flags, UnwrapSurface(reference));
    TraceResult("IDirectDrawSurface::UpdateOverlayZOrder", result);
    return result;
}

#undef GV_SURFACE_FORWARD_FLAGS

HRESULT WrapSurface(IDirectDrawSurface** surface, const bool isPrimary)
{
    if (!surface || !*surface)
    {
        return E_POINTER;
    }
    auto* proxy = new (std::nothrow) SurfaceProxy(*surface, isPrimary);
    if (!proxy)
    {
        (*surface)->Release();
        *surface = nullptr;
        return E_OUTOFMEMORY;
    }
    *surface = proxy;
    return S_OK;
}

IDirectDrawSurface* UnwrapSurface(IDirectDrawSurface* surface) noexcept
{
    if (auto* proxy = dynamic_cast<SurfaceProxy*>(surface))
    {
        return proxy->Real();
    }
    return surface;
}

void SetPresentationWindow(const HWND window) noexcept
{
    g_presentationWindow = window;
    g_windowPrepared = false;
    g_nextFrame = 0;
    AttachDwmPreview(window);
    Trace("Presentation window registered: hwnd=%p", window);
}
}
