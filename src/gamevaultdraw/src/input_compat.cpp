#include "input_compat.h"

#include "trace.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <cstdint>
#include <cstring>

namespace
{
using SetWindowLongAFunction = LONG(WINAPI*)(HWND, int, LONG);
using CallWindowProcAFunction = LRESULT(WINAPI*)(WNDPROC, HWND, UINT, WPARAM, LPARAM);

INIT_ONCE g_installation = INIT_ONCE_STATIC_INIT;
SetWindowLongAFunction g_realSetWindowLongA = nullptr;
CallWindowProcAFunction g_realCallWindowProcA = nullptr;
SRWLOCK g_subclassLock = SRWLOCK_INIT;

struct SubclassRecord
{
    HWND window = nullptr;
    WNDPROC previousProcedure = nullptr;
    WNDPROC gameProcedure = nullptr;
};

SubclassRecord g_subclassRecords[16] = {};

bool IsKnownLegacyInputProcedure(const WNDPROC procedure) noexcept
{
    const auto module = reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
    const auto address = reinterpret_cast<std::uintptr_t>(procedure);
    if (!module || address < module)
    {
        return false;
    }

    // These are the edit-control procedures installed by the original
    // ALADDINW.EXE and LIONW.EXE input property pages. The executables have no
    // relocation for these absolute callbacks, but compare as RVAs for clarity
    // and safety.
    const std::uintptr_t rva = address - module;
    switch (rva)
    {
    case 0x6A1Cu:
    case 0x6B13u:
    case 0x6C00u:
    case 0x6CEDu:
    case 0x6DDAu:
    case 0x6EC7u:
    case 0x6FB4u:
    case 0x7878u:
    case 0x789Eu:
    case 0x78C4u:
    case 0x292FBu:
    case 0x293E8u:
    case 0x294D5u:
    case 0x295C2u:
    case 0x296AFu:
    case 0x2979Cu:
    case 0x29889u:
    case 0x29976u:
    case 0x2A29Cu:
    case 0x2A574u:
    case 0x2A84Cu:
    case 0x2AB24u:
        return true;
    default:
        return false;
    }
}

void RememberSubclass(
    const HWND window, const WNDPROC previousProcedure,
    const WNDPROC gameProcedure) noexcept
{
    AcquireSRWLockExclusive(&g_subclassLock);
    SubclassRecord* available = nullptr;
    for (auto& record : g_subclassRecords)
    {
        if (record.window == window)
        {
            record.previousProcedure = previousProcedure;
            record.gameProcedure = gameProcedure;
            ReleaseSRWLockExclusive(&g_subclassLock);
            return;
        }
        if (!available && !record.window)
        {
            available = &record;
        }
    }
    if (available)
    {
        available->window = window;
        available->previousProcedure = previousProcedure;
        available->gameProcedure = gameProcedure;
    }
    ReleaseSRWLockExclusive(&g_subclassLock);
}

bool IsTrackedSubclass(const HWND window) noexcept
{
    bool result = false;
    AcquireSRWLockShared(&g_subclassLock);
    for (const auto& record : g_subclassRecords)
    {
        if (record.window == window)
        {
            result = true;
            break;
        }
    }
    ReleaseSRWLockShared(&g_subclassLock);
    return result;
}

void ForgetSubclass(const HWND window) noexcept
{
    AcquireSRWLockExclusive(&g_subclassLock);
    for (auto& record : g_subclassRecords)
    {
        if (record.window == window)
        {
            record = {};
            break;
        }
    }
    ReleaseSRWLockExclusive(&g_subclassLock);
}

LONG WINAPI CompatibleSetWindowLongA(
    const HWND window, const int index, const LONG newValue)
{
    const LONG previous = g_realSetWindowLongA(window, index, newValue);
    if (index == GWL_WNDPROC)
    {
        const auto gameProcedure = reinterpret_cast<WNDPROC>(
            static_cast<std::uintptr_t>(static_cast<DWORD>(newValue)));
        if (IsKnownLegacyInputProcedure(gameProcedure))
        {
            const auto previousProcedure = reinterpret_cast<WNDPROC>(
                static_cast<std::uintptr_t>(static_cast<DWORD>(previous)));
            RememberSubclass(window, previousProcedure, gameProcedure);
            gamevaultdraw::Trace(
                "INPUT compat: subclass hwnd=%p gameProc=%p previousProc=%p",
                window, gameProcedure, previousProcedure);
        }
    }
    return previous;
}

LRESULT WINAPI CompatibleCallWindowProcA(
    WNDPROC previousProcedure, const HWND window, const UINT message,
    const WPARAM wParam, const LPARAM lParam)
{
    WPARAM compatibleWParam = wParam;
    const bool tracked = IsTrackedSubclass(window);
    if (tracked && message == EM_GETSEL && HIWORD(wParam) == 0 &&
        HIWORD(static_cast<DWORD>(lParam)) != 0)
    {
        // The 1996 callback stores WPARAM in a 16-bit register. EM_GETSEL
        // receives two adjacent DWORD pointers; LPARAM survives intact while
        // WPARAM loses its high word (for example 000DEA80 -> 0000EA80).
        // Reconstruct only this documented pointer-bearing edit message and
        // require the resulting addresses to remain adjacent.
        const DWORD reference = static_cast<DWORD>(lParam);
        const DWORD candidate =
            (reference & 0xFFFF0000u) | static_cast<DWORD>(LOWORD(wParam));
        const DWORD distance = candidate > reference
            ? candidate - reference : reference - candidate;
        if (distance <= 16u)
        {
            compatibleWParam = static_cast<WPARAM>(candidate);
            gamevaultdraw::Trace(
                "INPUT compat: restored EM_GETSEL pointer hwnd=%p supplied=%p actual=%p companion=%p",
                window,
                reinterpret_cast<void*>(static_cast<std::uintptr_t>(wParam)),
                reinterpret_cast<void*>(static_cast<std::uintptr_t>(candidate)),
                reinterpret_cast<void*>(static_cast<std::uintptr_t>(reference)));
        }
    }

    const LRESULT result = g_realCallWindowProcA(
        previousProcedure, window, message, compatibleWParam, lParam);
    if (message == WM_NCDESTROY && tracked)
    {
        ForgetSubclass(window);
    }
    return result;
}

bool PatchImport(
    const HMODULE module, const char* importedModule, const char* importedName,
    void* replacement) noexcept
{
    if (!module || !importedModule || !importedName || !replacement)
    {
        return false;
    }

    const auto base = reinterpret_cast<BYTE*>(module);
    const auto dos = reinterpret_cast<IMAGE_DOS_HEADER*>(base);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE)
    {
        return false;
    }
    const auto nt = reinterpret_cast<IMAGE_NT_HEADERS*>(base + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE)
    {
        return false;
    }

    const IMAGE_DATA_DIRECTORY directory =
        nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    if (!directory.VirtualAddress)
    {
        return false;
    }

    auto descriptor = reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR*>(
        base + directory.VirtualAddress);
    for (; descriptor->Name; ++descriptor)
    {
        const char* moduleName = reinterpret_cast<const char*>(base + descriptor->Name);
        if (_stricmp(moduleName, importedModule) != 0 ||
            !descriptor->OriginalFirstThunk || !descriptor->FirstThunk)
        {
            continue;
        }

        auto names = reinterpret_cast<IMAGE_THUNK_DATA*>(
            base + descriptor->OriginalFirstThunk);
        auto addresses = reinterpret_cast<IMAGE_THUNK_DATA*>(
            base + descriptor->FirstThunk);
        for (; names->u1.AddressOfData; ++names, ++addresses)
        {
            if (IMAGE_SNAP_BY_ORDINAL(names->u1.Ordinal))
            {
                continue;
            }
            const auto import = reinterpret_cast<IMAGE_IMPORT_BY_NAME*>(
                base + names->u1.AddressOfData);
            if (std::strcmp(reinterpret_cast<const char*>(import->Name), importedName) != 0)
            {
                continue;
            }

            DWORD oldProtection = 0;
            if (!VirtualProtect(
                    &addresses->u1.Function, sizeof(addresses->u1.Function),
                    PAGE_READWRITE, &oldProtection))
            {
                return false;
            }
            addresses->u1.Function = reinterpret_cast<ULONG_PTR>(replacement);
            FlushInstructionCache(
                GetCurrentProcess(), &addresses->u1.Function,
                sizeof(addresses->u1.Function));
            DWORD ignored = 0;
            VirtualProtect(
                &addresses->u1.Function, sizeof(addresses->u1.Function),
                oldProtection, &ignored);
            return true;
        }
    }
    return false;
}

BOOL CALLBACK Install(PINIT_ONCE, PVOID, PVOID*)
{
    const HMODULE user32 = GetModuleHandleW(L"user32.dll");
    const HMODULE executable = GetModuleHandleW(nullptr);
    if (!user32 || !executable)
    {
        return FALSE;
    }

    g_realSetWindowLongA = reinterpret_cast<SetWindowLongAFunction>(
        GetProcAddress(user32, "SetWindowLongA"));
    g_realCallWindowProcA = reinterpret_cast<CallWindowProcAFunction>(
        GetProcAddress(user32, "CallWindowProcA"));
    if (!g_realSetWindowLongA || !g_realCallWindowProcA)
    {
        return FALSE;
    }

    const bool setPatched = PatchImport(
        executable, "USER32.dll", "SetWindowLongA",
        reinterpret_cast<void*>(&CompatibleSetWindowLongA));
    const bool callPatched = PatchImport(
        executable, "USER32.dll", "CallWindowProcA",
        reinterpret_cast<void*>(&CompatibleCallWindowProcA));
    gamevaultdraw::Trace(
        "INPUT compatibility installation: SetWindowLongA=%u CallWindowProcA=%u",
        setPatched ? 1u : 0u, callPatched ? 1u : 0u);
    return setPatched && callPatched;
}
}

namespace gamevaultdraw
{
void InstallInputCompatibility() noexcept
{
    InitOnceExecuteOnce(&g_installation, Install, nullptr, nullptr);
}
}
