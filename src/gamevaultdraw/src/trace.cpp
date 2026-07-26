#include "trace.h"

#include <cstdarg>
#include <cstdio>
#include <string>

extern "C" IMAGE_DOS_HEADER __ImageBase;

namespace
{
std::wstring GetLogPath()
{
    wchar_t modulePath[MAX_PATH] = {};
    const auto length = GetModuleFileNameW(
        reinterpret_cast<HMODULE>(&__ImageBase), modulePath, MAX_PATH);
    if (length == 0 || length >= MAX_PATH)
    {
        return L"GameVaultDraw.log";
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
    path += L"GameVaultDraw.log";
    return path;
}
}

namespace gamevaultdraw
{
void Trace(const char* format, ...)
{
    char message[2048] = {};
    va_list arguments;
    va_start(arguments, format);
    const int count = vsnprintf_s(message, sizeof(message), _TRUNCATE, format, arguments);
    va_end(arguments);
    if (count < 0)
    {
        return;
    }

    SYSTEMTIME now = {};
    GetLocalTime(&now);

    char line[2304] = {};
    const int lineLength = sprintf_s(
        line,
        sizeof(line),
        "%02u:%02u:%02u.%03u [tid=%lu] %s\r\n",
        now.wHour,
        now.wMinute,
        now.wSecond,
        now.wMilliseconds,
        GetCurrentThreadId(),
        message);
    if (lineLength < 0)
    {
        return;
    }

    const auto path = GetLogPath();
    const HANDLE file = CreateFileW(
        path.c_str(),
        FILE_APPEND_DATA,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr,
        OPEN_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);
    if (file == INVALID_HANDLE_VALUE)
    {
        return;
    }

    DWORD written = 0;
    WriteFile(file, line, static_cast<DWORD>(lineLength), &written, nullptr);
    CloseHandle(file);
}

void TraceResult(const char* operation, const HRESULT result)
{
    Trace("%s -> HRESULT=0x%08lX", operation, static_cast<unsigned long>(result));
}
}
