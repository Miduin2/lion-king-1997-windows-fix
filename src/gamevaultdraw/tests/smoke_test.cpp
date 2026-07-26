#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <ddraw.h>

#include <cstdio>

namespace
{
using DirectDrawCreateFunction = HRESULT(WINAPI*)(GUID*, IDirectDraw**, IUnknown*);
}

int main()
{
    const HMODULE module = LoadLibraryW(L"ddraw.dll");
    if (!module)
    {
        std::fprintf(stderr, "LoadLibraryW failed: %lu\n", GetLastError());
        return 1;
    }

    const auto create = reinterpret_cast<DirectDrawCreateFunction>(
        GetProcAddress(module, "DirectDrawCreate"));
    if (!create)
    {
        std::fprintf(stderr, "GetProcAddress failed: %lu\n", GetLastError());
        FreeLibrary(module);
        return 2;
    }

    IDirectDraw* directDraw = nullptr;
    const HRESULT createResult = create(nullptr, &directDraw, nullptr);
    if (FAILED(createResult) || !directDraw)
    {
        std::fprintf(stderr, "DirectDrawCreate failed: 0x%08lX\n",
            static_cast<unsigned long>(createResult));
        FreeLibrary(module);
        return 3;
    }

    DDSURFACEDESC description = {};
    description.dwSize = sizeof(description);
    const HRESULT modeResult = directDraw->GetDisplayMode(&description);
    if (FAILED(modeResult))
    {
        std::fprintf(stderr, "GetDisplayMode failed: 0x%08lX\n",
            static_cast<unsigned long>(modeResult));
        directDraw->Release();
        FreeLibrary(module);
        return 4;
    }

    std::printf(
        "GameVaultDraw smoke test OK: %lux%lu, %lu bpp\n",
        description.dwWidth,
        description.dwHeight,
        description.ddpfPixelFormat.dwRGBBitCount);

    const HRESULT cooperativeResult = directDraw->SetCooperativeLevel(
        GetDesktopWindow(), DDSCL_NORMAL);
    if (FAILED(cooperativeResult))
    {
        std::fprintf(stderr, "SetCooperativeLevel failed: 0x%08lX\n",
            static_cast<unsigned long>(cooperativeResult));
        directDraw->Release();
        FreeLibrary(module);
        return 5;
    }

    DDSURFACEDESC surfaceDescription = {};
    surfaceDescription.dwSize = sizeof(surfaceDescription);
    surfaceDescription.dwFlags = DDSD_CAPS | DDSD_WIDTH | DDSD_HEIGHT;
    surfaceDescription.dwWidth = 320;
    surfaceDescription.dwHeight = 216;
    surfaceDescription.ddsCaps.dwCaps = DDSCAPS_OFFSCREENPLAIN | DDSCAPS_SYSTEMMEMORY;

    IDirectDrawSurface* surface = nullptr;
    const HRESULT surfaceResult = directDraw->CreateSurface(
        &surfaceDescription, &surface, nullptr);
    if (FAILED(surfaceResult) || !surface)
    {
        std::fprintf(stderr, "CreateSurface failed: 0x%08lX\n",
            static_cast<unsigned long>(surfaceResult));
        directDraw->Release();
        FreeLibrary(module);
        return 6;
    }

    PALETTEENTRY entries[256] = {};
    IDirectDrawPalette* palette = nullptr;
    const HRESULT paletteResult = directDraw->CreatePalette(
        DDPCAPS_8BIT | DDPCAPS_ALLOW256, entries, &palette, nullptr);
    if (FAILED(paletteResult) || !palette)
    {
        std::fprintf(stderr, "CreatePalette failed: 0x%08lX\n",
            static_cast<unsigned long>(paletteResult));
        surface->Release();
        directDraw->Release();
        FreeLibrary(module);
        return 7;
    }

    const HRESULT setPaletteResult = surface->SetPalette(palette);
    std::printf("Offscreen SetPalette returned: 0x%08lX\n",
        static_cast<unsigned long>(setPaletteResult));
    if (FAILED(setPaletteResult))
    {
        std::fprintf(stderr, "Virtual palette association failed: 0x%08lX\n",
            static_cast<unsigned long>(setPaletteResult));
        palette->Release();
        surface->Release();
        directDraw->Release();
        FreeLibrary(module);
        return 9;
    }

    IDirectDrawPalette* associatedPalette = nullptr;
    const HRESULT getPaletteResult = surface->GetPalette(&associatedPalette);
    if (FAILED(getPaletteResult) || !associatedPalette)
    {
        std::fprintf(stderr, "Virtual GetPalette failed: 0x%08lX\n",
            static_cast<unsigned long>(getPaletteResult));
        palette->Release();
        surface->Release();
        directDraw->Release();
        FreeLibrary(module);
        return 10;
    }
    associatedPalette->Release();

    DDSURFACEDESC lockDescription = {};
    lockDescription.dwSize = sizeof(lockDescription);
    const HRESULT lockResult = surface->Lock(nullptr, &lockDescription, 0, nullptr);
    if (SUCCEEDED(lockResult))
    {
        surface->Unlock(lockDescription.lpSurface);
    }

    palette->Release();
    surface->Release();

    directDraw->Release();
    FreeLibrary(module);
    return 0;
}
