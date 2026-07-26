#include "dwm_preview.h"

#include "trace.h"

#include <dwmapi.h>

#include <cstdint>
#include <cstring>
#include <vector>

namespace
{
SRWLOCK g_frameLock = SRWLOCK_INIT;
std::vector<std::uint32_t> g_framePixels;
DWORD g_frameWidth = 0;
DWORD g_frameHeight = 0;
volatile LONG g_frameUpdates = 0;
HWND g_previewWindow = nullptr;
WNDPROC g_originalWindowProcedure = nullptr;

struct FrameSnapshot
{
    std::vector<std::uint32_t> pixels;
    DWORD width = 0;
    DWORD height = 0;
};

FrameSnapshot CopyFrameSnapshot() noexcept
{
    FrameSnapshot snapshot;
    AcquireSRWLockShared(&g_frameLock);
    try
    {
        snapshot.width = g_frameWidth;
        snapshot.height = g_frameHeight;
        snapshot.pixels = g_framePixels;
    }
    catch (...)
    {
        snapshot = {};
    }
    ReleaseSRWLockShared(&g_frameLock);
    return snapshot;
}

HBITMAP CreateScaledPreviewBitmap(const int targetWidth, const int targetHeight)
{
    if (targetWidth <= 0 || targetHeight <= 0)
    {
        return nullptr;
    }

    const FrameSnapshot frame = CopyFrameSnapshot();
    if (frame.pixels.empty() || frame.width == 0 || frame.height == 0)
    {
        return nullptr;
    }

    BITMAPINFO targetInfo = {};
    targetInfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    targetInfo.bmiHeader.biWidth = targetWidth;
    targetInfo.bmiHeader.biHeight = -targetHeight;
    targetInfo.bmiHeader.biPlanes = 1;
    targetInfo.bmiHeader.biBitCount = 32;
    targetInfo.bmiHeader.biCompression = BI_RGB;

    void* targetPixels = nullptr;
    const HBITMAP bitmap = CreateDIBSection(
        nullptr, &targetInfo, DIB_RGB_COLORS, &targetPixels, nullptr, 0);
    if (!bitmap || !targetPixels)
    {
        return nullptr;
    }
    std::memset(targetPixels, 0, static_cast<std::size_t>(targetWidth) *
        static_cast<std::size_t>(targetHeight) * sizeof(std::uint32_t));

    const HDC memoryDc = CreateCompatibleDC(nullptr);
    if (!memoryDc)
    {
        DeleteObject(bitmap);
        return nullptr;
    }
    const HGDIOBJ previousBitmap = SelectObject(memoryDc, bitmap);
    SetStretchBltMode(memoryDc, COLORONCOLOR);

    int drawWidth = targetWidth;
    int drawHeight = drawWidth * 3 / 4;
    if (drawHeight > targetHeight)
    {
        drawHeight = targetHeight;
        drawWidth = drawHeight * 4 / 3;
    }
    const int drawX = (targetWidth - drawWidth) / 2;
    const int drawY = (targetHeight - drawHeight) / 2;

    BITMAPINFO sourceInfo = {};
    sourceInfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    sourceInfo.bmiHeader.biWidth = static_cast<LONG>(frame.width);
    sourceInfo.bmiHeader.biHeight = -static_cast<LONG>(frame.height);
    sourceInfo.bmiHeader.biPlanes = 1;
    sourceInfo.bmiHeader.biBitCount = 32;
    sourceInfo.bmiHeader.biCompression = BI_RGB;

    StretchDIBits(
        memoryDc,
        drawX, drawY, drawWidth, drawHeight,
        0, 0, static_cast<int>(frame.width), static_cast<int>(frame.height),
        frame.pixels.data(), &sourceInfo, DIB_RGB_COLORS, SRCCOPY);

    SelectObject(memoryDc, previousBitmap);
    DeleteDC(memoryDc);

    auto* output = static_cast<std::uint32_t*>(targetPixels);
    const std::size_t pixelCount = static_cast<std::size_t>(targetWidth) *
        static_cast<std::size_t>(targetHeight);
    for (std::size_t index = 0; index < pixelCount; ++index)
    {
        output[index] |= 0xFF000000u;
    }
    return bitmap;
}

LRESULT CALLBACK PreviewWindowProcedure(
    const HWND window, const UINT message, const WPARAM wParam, const LPARAM lParam)
{
    if (message == WM_SETCURSOR && IsWindowEnabled(window))
    {
        // The original game leaves Windows' busy cursor visible over the
        // rendered surface. Hide it only while the main window is enabled;
        // modal Properties and exit dialogs therefore keep a usable cursor.
        SetCursor(nullptr);
        return TRUE;
    }

    // Keep this diagnostic deliberately narrow: the original title has both
    // an internal keyboard-driven menu and a native Win32 menu.  Logging the
    // messages at the already-installed subclass lets us prove whether a key
    // reached the game without changing or consuming it.
    if (message == WM_KEYDOWN || message == WM_KEYUP ||
        message == WM_SYSKEYDOWN || message == WM_SYSKEYUP)
    {
        gamevaultdraw::Trace(
            "WINDOW key message=0x%04X vk=0x%02llX repeat=%u previous=%u transition=%u focus=%p foreground=%p",
            message,
            static_cast<unsigned long long>(wParam),
            static_cast<unsigned int>(LOWORD(lParam)),
            static_cast<unsigned int>((lParam >> 30) & 1),
            static_cast<unsigned int>((lParam >> 31) & 1),
            GetFocus(),
            GetForegroundWindow());
    }
    else if (message == WM_COMMAND)
    {
        gamevaultdraw::Trace(
            "WINDOW command id=%u notification=%u source=%p",
            static_cast<unsigned int>(LOWORD(wParam)),
            static_cast<unsigned int>(HIWORD(wParam)),
            reinterpret_cast<void*>(lParam));
    }
    else if (message == WM_SETFOCUS || message == WM_KILLFOCUS ||
        message == WM_ACTIVATE || message == WM_ACTIVATEAPP)
    {
        gamevaultdraw::Trace(
            "WINDOW focus message=0x%04X wParam=0x%llX lParam=0x%llX focus=%p foreground=%p",
            message,
            static_cast<unsigned long long>(wParam),
            static_cast<unsigned long long>(lParam),
            GetFocus(),
            GetForegroundWindow());
    }

    const bool initialKeyPress = (static_cast<unsigned long long>(lParam) &
        (1ull << 30)) == 0;
    if (message == WM_KEYDOWN && initialKeyPress && wParam == VK_F2)
    {
        // The borderless presentation intentionally hides the original menu
        // bar.  Command 201 is the executable's own Options > Properties
        // command, so this opens its original difficulty, keyboard, sound and
        // display property sheet without recreating those settings ourselves.
        gamevaultdraw::Trace("WINDOW accessibility: F2 -> native Properties command 201");
        PostMessageW(window, WM_COMMAND, MAKEWPARAM(201, 0), 0);
        return 0;
    }
    if (message == WM_KEYDOWN && initialKeyPress && wParam == VK_ESCAPE)
    {
        // This Windows conversion receives Escape but does not act on it on a
        // modern system.  Restore a safe, modal exit path while keeping No as
        // the default to prevent an accidental loss of the current run.
        gamevaultdraw::Trace("WINDOW accessibility: Escape -> exit confirmation");
        wchar_t gameTitle[128] = {};
        if (GetWindowTextW(window, gameTitle, ARRAYSIZE(gameTitle)) == 0)
        {
            lstrcpynW(gameTitle, L"Game", ARRAYSIZE(gameTitle));
        }
        const int answer = MessageBoxW(
            window,
            L"Are you sure you want to exit the game?",
            gameTitle,
            MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2 | MB_APPLMODAL);
        gamevaultdraw::Trace("WINDOW accessibility: exit confirmation result=%d", answer);
        if (answer == IDYES)
        {
            PostMessageW(window, WM_CLOSE, 0, 0);
        }
        else
        {
            SetForegroundWindow(window);
            SetFocus(window);
        }
        return 0;
    }

    if (message == WM_DWMSENDICONICTHUMBNAIL)
    {
        const int maximumWidth = static_cast<int>(HIWORD(lParam));
        const int maximumHeight = static_cast<int>(LOWORD(lParam));
        const HBITMAP bitmap = CreateScaledPreviewBitmap(maximumWidth, maximumHeight);
        if (bitmap)
        {
            const HRESULT result = DwmSetIconicThumbnail(window, bitmap, 0);
            gamevaultdraw::Trace(
                "DWM iconic thumbnail: %dx%d -> HRESULT=0x%08lX",
                maximumWidth, maximumHeight, static_cast<unsigned long>(result));
            DeleteObject(bitmap);
            return 0;
        }
    }
    else if (message == WM_DWMSENDICONICLIVEPREVIEWBITMAP)
    {
        RECT client = {};
        if (GetClientRect(window, &client))
        {
            const int width = client.right - client.left;
            const int height = client.bottom - client.top;
            const HBITMAP bitmap = CreateScaledPreviewBitmap(width, height);
            if (bitmap)
            {
                const HRESULT result = DwmSetIconicLivePreviewBitmap(
                    window, bitmap, nullptr, DWM_SIT_DISPLAYFRAME);
                gamevaultdraw::Trace(
                    "DWM live preview: %dx%d -> HRESULT=0x%08lX",
                    width, height, static_cast<unsigned long>(result));
                DeleteObject(bitmap);
                return 0;
            }
        }
    }
    else if (message == WM_NCDESTROY && window == g_previewWindow)
    {
        const WNDPROC original = g_originalWindowProcedure;
        if (original)
        {
            SetWindowLongPtrW(window, GWLP_WNDPROC,
                reinterpret_cast<LONG_PTR>(original));
        }
        g_previewWindow = nullptr;
        g_originalWindowProcedure = nullptr;
        return original
            ? CallWindowProcW(original, window, message, wParam, lParam)
            : DefWindowProcW(window, message, wParam, lParam);
    }

    return g_originalWindowProcedure
        ? CallWindowProcW(g_originalWindowProcedure, window, message, wParam, lParam)
        : DefWindowProcW(window, message, wParam, lParam);
}
}

namespace gamevaultdraw
{
void AttachDwmPreview(const HWND window) noexcept
{
    if (!window || !IsWindow(window) || window == g_previewWindow)
    {
        return;
    }

    BOOL compositionEnabled = FALSE;
    if (FAILED(DwmIsCompositionEnabled(&compositionEnabled)) || !compositionEnabled)
    {
        Trace("DWM custom preview unavailable: composition disabled");
        return;
    }

    SetLastError(ERROR_SUCCESS);
    const LONG_PTR previous = SetWindowLongPtrW(
        window, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(PreviewWindowProcedure));
    if (previous == 0 && GetLastError() != ERROR_SUCCESS)
    {
        Trace("DWM custom preview subclass failed: error=%lu", GetLastError());
        return;
    }

    g_previewWindow = window;
    g_originalWindowProcedure = reinterpret_cast<WNDPROC>(previous);

    const BOOL enabled = TRUE;
    const HRESULT forceResult = DwmSetWindowAttribute(
        window, DWMWA_FORCE_ICONIC_REPRESENTATION, &enabled, sizeof(enabled));
    const HRESULT bitmapResult = DwmSetWindowAttribute(
        window, DWMWA_HAS_ICONIC_BITMAP, &enabled, sizeof(enabled));
    const HRESULT invalidateResult = DwmInvalidateIconicBitmaps(window);
    Trace(
        "DWM custom preview attached: hwnd=%p force=0x%08lX bitmap=0x%08lX invalidate=0x%08lX",
        window,
        static_cast<unsigned long>(forceResult),
        static_cast<unsigned long>(bitmapResult),
        static_cast<unsigned long>(invalidateResult));
}

void UpdateDwmPreview(
    const void* pixels, const LONG pitch, const DWORD width, const DWORD height,
    const DWORD bitsPerPixel) noexcept
{
    if (!pixels || pitch == 0 || width == 0 || height == 0 || bitsPerPixel != 32)
    {
        return;
    }

    try
    {
        std::vector<std::uint32_t> copy(
            static_cast<std::size_t>(width) * static_cast<std::size_t>(height));
        const auto* source = static_cast<const BYTE*>(pixels);
        const LONG absolutePitch = pitch < 0 ? -pitch : pitch;
        if (absolutePitch < static_cast<LONG>(width * sizeof(std::uint32_t)))
        {
            return;
        }

        for (DWORD y = 0; y < height; ++y)
        {
            const DWORD sourceY = pitch < 0 ? height - 1 - y : y;
            std::memcpy(
                copy.data() + static_cast<std::size_t>(y) * width,
                source + static_cast<std::size_t>(sourceY) * absolutePitch,
                static_cast<std::size_t>(width) * sizeof(std::uint32_t));
        }

        AcquireSRWLockExclusive(&g_frameLock);
        g_framePixels.swap(copy);
        g_frameWidth = width;
        g_frameHeight = height;
        ReleaseSRWLockExclusive(&g_frameLock);

        const LONG update = InterlockedIncrement(&g_frameUpdates);
        if (g_previewWindow && (update == 1 || update % 60 == 0))
        {
            DwmInvalidateIconicBitmaps(g_previewWindow);
        }
    }
    catch (...)
    {
        // A preview is cosmetic; allocation failure must never affect gameplay.
    }
}
}
