#include "ddraw_proxy.h"

#include "input_compat.h"
#include "trace.h"

#include <new>
#include <string>

namespace
{
using DirectDrawCreateFunction = HRESULT(WINAPI*)(GUID*, IDirectDraw**, IUnknown*);

INIT_ONCE g_initialization = INIT_ONCE_STATIC_INIT;
INIT_ONCE g_exceptionObserverInitialization = INIT_ONCE_STATIC_INIT;
HMODULE g_systemDdraw = nullptr;
DirectDrawCreateFunction g_realDirectDrawCreate = nullptr;
PVOID g_exceptionObserver = nullptr;
volatile LONG g_observedAccessViolations = 0;

LONG CALLBACK ObserveException(PEXCEPTION_POINTERS exceptionPointers)
{
    if (!exceptionPointers || !exceptionPointers->ExceptionRecord ||
        !exceptionPointers->ContextRecord ||
        exceptionPointers->ExceptionRecord->ExceptionCode != EXCEPTION_ACCESS_VIOLATION)
    {
        return EXCEPTION_CONTINUE_SEARCH;
    }

    const LONG sequence = InterlockedIncrement(&g_observedAccessViolations);
    if (sequence > 8)
    {
        return EXCEPTION_CONTINUE_SEARCH;
    }

    const EXCEPTION_RECORD* record = exceptionPointers->ExceptionRecord;
    const CONTEXT* context = exceptionPointers->ContextRecord;
    const ULONG_PTR operation = record->NumberParameters >= 1
        ? record->ExceptionInformation[0] : static_cast<ULONG_PTR>(~0u);
    const ULONG_PTR target = record->NumberParameters >= 2
        ? record->ExceptionInformation[1] : 0;

    gamevaultdraw::Trace(
        "EXCEPTION #%ld code=0x%08lX instruction=%p operation=%llu target=%p",
        sequence,
        static_cast<unsigned long>(record->ExceptionCode),
        record->ExceptionAddress,
        static_cast<unsigned long long>(operation),
        reinterpret_cast<void*>(target));

#if defined(_M_IX86)
    gamevaultdraw::Trace(
        "EXCEPTION x86 eip=%08lX esp=%08lX ebp=%08lX eax=%08lX ebx=%08lX "
        "ecx=%08lX edx=%08lX esi=%08lX edi=%08lX eflags=%08lX",
        context->Eip, context->Esp, context->Ebp, context->Eax,
        context->Ebx, context->Ecx, context->Edx, context->Esi,
        context->Edi, context->EFlags);

    DWORD stackWords[32] = {};
    SIZE_T bytesRead = 0;
    if (ReadProcessMemory(
            GetCurrentProcess(), reinterpret_cast<const void*>(context->Esp),
            stackWords, sizeof(stackWords), &bytesRead) && bytesRead >= sizeof(DWORD))
    {
        const SIZE_T wordCount = bytesRead / sizeof(DWORD);
        for (SIZE_T index = 0; index + 7 < wordCount; index += 8)
        {
            gamevaultdraw::Trace(
                "EXCEPTION stack esp+%02lX: %08lX %08lX %08lX %08lX %08lX %08lX %08lX %08lX",
                static_cast<unsigned long>(index * sizeof(DWORD)),
                stackWords[index + 0], stackWords[index + 1],
                stackWords[index + 2], stackWords[index + 3],
                stackWords[index + 4], stackWords[index + 5],
                stackWords[index + 6], stackWords[index + 7]);
        }
    }
#endif

    return EXCEPTION_CONTINUE_SEARCH;
}

BOOL CALLBACK InstallExceptionObserver(PINIT_ONCE, PVOID, PVOID*)
{
    g_exceptionObserver = AddVectoredExceptionHandler(1, ObserveException);
    gamevaultdraw::Trace("Exception observer installation handle=%p", g_exceptionObserver);
    return g_exceptionObserver != nullptr;
}

BOOL CALLBACK LoadSystemDirectDraw(PINIT_ONCE, PVOID, PVOID*)
{
    wchar_t systemDirectory[MAX_PATH] = {};
    const UINT length = GetSystemDirectoryW(systemDirectory, MAX_PATH);
    if (length == 0 || length >= MAX_PATH)
    {
        gamevaultdraw::Trace("GetSystemDirectoryW failed: error=%lu", GetLastError());
        return FALSE;
    }

    std::wstring path(systemDirectory, length);
    path += L"\\ddraw.dll";
    g_systemDdraw = LoadLibraryW(path.c_str());
    if (!g_systemDdraw)
    {
        gamevaultdraw::Trace("LoadLibraryW(system ddraw.dll) failed: error=%lu", GetLastError());
        return FALSE;
    }

    g_realDirectDrawCreate = reinterpret_cast<DirectDrawCreateFunction>(
        GetProcAddress(g_systemDdraw, "DirectDrawCreate"));
    if (!g_realDirectDrawCreate)
    {
        gamevaultdraw::Trace("GetProcAddress(DirectDrawCreate) failed: error=%lu", GetLastError());
        return FALSE;
    }

    gamevaultdraw::Trace("Loaded system DirectDraw from %ls", path.c_str());
    return TRUE;
}
}

extern "C" HRESULT WINAPI DirectDrawCreate(
    GUID* guid, IDirectDraw** directDraw, IUnknown* outer)
{
    if (!directDraw)
    {
        return E_POINTER;
    }
    *directDraw = nullptr;

    InitOnceExecuteOnce(
        &g_exceptionObserverInitialization, InstallExceptionObserver, nullptr, nullptr);
    gamevaultdraw::InstallInputCompatibility();

    if (!InitOnceExecuteOnce(&g_initialization, LoadSystemDirectDraw, nullptr, nullptr) ||
        !g_realDirectDrawCreate)
    {
        return DDERR_GENERIC;
    }

    gamevaultdraw::Trace("DirectDrawCreate guid=%p outer=%p", guid, outer);
    IDirectDraw* real = nullptr;
    const HRESULT result = g_realDirectDrawCreate(guid, &real, outer);
    gamevaultdraw::Trace("DirectDrawCreate(system) -> HRESULT=0x%08lX object=%p",
        static_cast<unsigned long>(result), real);
    if (FAILED(result) || !real)
    {
        return result;
    }

    auto* proxy = new (std::nothrow) gamevaultdraw::DirectDrawProxy(real);
    if (!proxy)
    {
        real->Release();
        return E_OUTOFMEMORY;
    }

    *directDraw = proxy;
    return result;
}

BOOL WINAPI DllMain(HINSTANCE instance, const DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH)
    {
        DisableThreadLibraryCalls(instance);
    }
    else if (reason == DLL_PROCESS_DETACH && g_exceptionObserver)
    {
        RemoveVectoredExceptionHandler(g_exceptionObserver);
        g_exceptionObserver = nullptr;
    }
    return TRUE;
}
