#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace gamevaultdraw
{
void AttachDwmPreview(HWND window) noexcept;
void UpdateDwmPreview(
    const void* pixels, LONG pitch, DWORD width, DWORD height,
    DWORD bitsPerPixel) noexcept;
}
