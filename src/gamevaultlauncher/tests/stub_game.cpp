#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <string>

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int)
{
    wchar_t module[32768] = {};
    wchar_t directory[32768] = {};
    wchar_t compatibility[512] = {};
    GetModuleFileNameW(nullptr, module, _countof(module));
    GetCurrentDirectoryW(_countof(directory), directory);
    const DWORD compatibilityLength = GetEnvironmentVariableW(
        L"__COMPAT_LAYER", compatibility, _countof(compatibility));

    std::wstring result = L"module=";
    result += module;
    result += L"\r\ndirectory=";
    result += directory;
    result += L"\r\ncompatibility=";
    if (compatibilityLength)
    {
        result += compatibility;
    }
    result += L"\r\n";

    const HANDLE file = CreateFileW(
        L"launcher-stub-result.txt", GENERIC_WRITE, FILE_SHARE_READ,
        nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE)
    {
        return 2;
    }
    DWORD written = 0;
    const BOOL success = WriteFile(
        file, result.data(),
        static_cast<DWORD>(result.size() * sizeof(wchar_t)), &written, nullptr);
    CloseHandle(file);
    return success ? 0 : 3;
}
