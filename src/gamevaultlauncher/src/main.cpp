#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <tlhelp32.h>

#include <cstdarg>
#include <cstdint>
#include <cwchar>
#include <string>
#include <vector>

namespace
{
constexpr wchar_t kLauncherMutex[] = L"Local\\GameVault.DisneyClassicLionKing.Launcher";
constexpr wchar_t kCompatibilityKey[] =
    L"Software\\Microsoft\\Windows NT\\CurrentVersion\\AppCompatFlags\\Layers";
constexpr wchar_t kCompatibilityEnvironment[] = L"__COMPAT_LAYER";

std::wstring g_logPath;

std::wstring JoinPath(const std::wstring& directory, const wchar_t* name)
{
    if (directory.empty() || directory.back() == L'\\')
    {
        return directory + name;
    }
    return directory + L"\\" + name;
}

std::wstring Utf16FromSystemMessage(const DWORD error)
{
    wchar_t* raw = nullptr;
    const DWORD length = FormatMessageW(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
            FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr, error, 0, reinterpret_cast<wchar_t*>(&raw), 0, nullptr);
    std::wstring message = length && raw ? std::wstring(raw, length) : L"Unknown error";
    if (raw)
    {
        LocalFree(raw);
    }
    while (!message.empty() &&
        (message.back() == L'\r' || message.back() == L'\n' || message.back() == L' '))
    {
        message.pop_back();
    }
    return message;
}

void Log(const wchar_t* format, ...)
{
    if (g_logPath.empty())
    {
        return;
    }

    wchar_t body[2048] = {};
    va_list arguments;
    va_start(arguments, format);
    _vsnwprintf_s(body, _countof(body), _TRUNCATE, format, arguments);
    va_end(arguments);

    SYSTEMTIME time = {};
    GetLocalTime(&time);
    wchar_t line[2304] = {};
    _snwprintf_s(
        line, _countof(line), _TRUNCATE,
        L"%04u-%02u-%02u %02u:%02u:%02u.%03u %ls\r\n",
        time.wYear, time.wMonth, time.wDay, time.wHour, time.wMinute,
        time.wSecond, time.wMilliseconds, body);

    const int byteCount = WideCharToMultiByte(
        CP_UTF8, 0, line, -1, nullptr, 0, nullptr, nullptr);
    if (byteCount <= 1)
    {
        return;
    }
    std::vector<char> bytes(static_cast<std::size_t>(byteCount));
    WideCharToMultiByte(
        CP_UTF8, 0, line, -1, bytes.data(), byteCount, nullptr, nullptr);

    const HANDLE file = CreateFileW(
        g_logPath.c_str(), FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE)
    {
        return;
    }
    DWORD written = 0;
    WriteFile(
        file, bytes.data(), static_cast<DWORD>(bytes.size() - 1), &written, nullptr);
    CloseHandle(file);
}

int ShowError(const std::wstring& message, const DWORD error = ERROR_SUCCESS)
{
    std::wstring full = message;
    if (error != ERROR_SUCCESS)
    {
        full += L"\n\nWindows details: ";
        full += Utf16FromSystemMessage(error);
        full += L"\nError code: ";
        full += std::to_wstring(error);
    }
    full += L"\n\nSee GameVaultLauncher.log for more information.";
    Log(L"ERROR code=%lu message=%ls", error, message.c_str());
    MessageBoxW(
        nullptr, full.c_str(), L"Disney's The Lion King could not be started",
        MB_OK | MB_ICONERROR | MB_SETFOREGROUND);
    return 1;
}

std::wstring ExecutableDirectory()
{
    std::vector<wchar_t> path(32768);
    const DWORD length = GetModuleFileNameW(
        nullptr, path.data(), static_cast<DWORD>(path.size()));
    if (!length || length >= path.size())
    {
        return {};
    }
    std::wstring result(path.data(), length);
    const std::size_t separator = result.find_last_of(L"\\/");
    return separator == std::wstring::npos ? std::wstring() : result.substr(0, separator);
}

bool FileExists(const std::wstring& path)
{
    const DWORD attributes = GetFileAttributesW(path.c_str());
    return attributes != INVALID_FILE_ATTRIBUTES &&
        (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

bool IsLionKingRunning()
{
    const HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE)
    {
        return false;
    }
    PROCESSENTRY32W entry = {};
    entry.dwSize = sizeof(entry);
    bool running = false;
    if (Process32FirstW(snapshot, &entry))
    {
        do
        {
            if (_wcsicmp(entry.szExeFile, L"LIONW.EXE") == 0)
            {
                running = true;
                break;
            }
        } while (Process32NextW(snapshot, &entry));
    }
    CloseHandle(snapshot);
    return running;
}

std::wstring Quote(const std::wstring& value)
{
    return L"\"" + value + L"\"";
}

bool RunHiddenProcess(const std::wstring& executable, const std::wstring& arguments)
{
    std::wstring command = Quote(executable) + L" " + arguments;
    STARTUPINFOW startup = {};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process = {};
    const BOOL created = CreateProcessW(
        executable.c_str(), command.data(), nullptr, nullptr, FALSE,
        CREATE_NO_WINDOW, nullptr, nullptr, &startup, &process);
    if (!created)
    {
        return false;
    }
    WaitForSingleObject(process.hProcess, INFINITE);
    DWORD exitCode = ERROR_GEN_FAILURE;
    GetExitCodeProcess(process.hProcess, &exitCode);
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    SetLastError(exitCode);
    return exitCode == ERROR_SUCCESS;
}

wchar_t PickVirtualDrive()
{
    const DWORD drives = GetLogicalDrives();
    constexpr wchar_t candidates[] = L"VWXYZRQ";
    for (const wchar_t letter : candidates)
    {
        if (!letter)
        {
            break;
        }
        const DWORD bit = 1u << static_cast<unsigned int>(letter - L'A');
        if ((drives & bit) == 0)
        {
            return letter;
        }
    }
    return L'\0';
}

std::wstring SubstExecutable()
{
    wchar_t systemDirectory[MAX_PATH] = {};
    const UINT length = GetSystemDirectoryW(systemDirectory, _countof(systemDirectory));
    if (!length || length >= _countof(systemDirectory))
    {
        return {};
    }
    return JoinPath(systemDirectory, L"subst.exe");
}

bool MapDrive(
    const std::wstring& subst, const wchar_t letter,
    const std::wstring& directory)
{
    const std::wstring drive = std::wstring(1, letter) + L":";
    return RunHiddenProcess(subst, drive + L" " + Quote(directory));
}

void UnmapDrive(const std::wstring& subst, const wchar_t letter)
{
    if (!subst.empty() && letter)
    {
        const std::wstring drive = std::wstring(1, letter) + L":";
        RunHiddenProcess(subst, drive + L" /D");
    }
}

void RotateGameLog(const std::wstring& directory)
{
    const std::wstring current = JoinPath(directory, L"GameVaultDraw.log");
    if (!FileExists(current))
    {
        return;
    }
    SYSTEMTIME time = {};
    GetLocalTime(&time);
    wchar_t name[96] = {};
    _snwprintf_s(
        name, _countof(name), _TRUNCATE,
        L"GameVaultDraw-%04u%02u%02u-%02u%02u%02u-%03u.log",
        time.wYear, time.wMonth, time.wDay, time.wHour, time.wMinute,
        time.wSecond, time.wMilliseconds);
    MoveFileExW(
        current.c_str(), JoinPath(directory, name).c_str(),
        MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH);
}

struct CompatibilityState
{
    HKEY key = nullptr;
    std::wstring valueName;
    DWORD type = REG_SZ;
    std::vector<BYTE> value;
    bool existed = false;

    void RemoveFor(const std::wstring& gamePath)
    {
        valueName = gamePath;
        if (RegOpenKeyExW(
                HKEY_CURRENT_USER, kCompatibilityKey, 0,
                KEY_QUERY_VALUE | KEY_SET_VALUE, &key) != ERROR_SUCCESS)
        {
            key = nullptr;
            return;
        }
        DWORD size = 0;
        const LSTATUS query = RegQueryValueExW(
            key, valueName.c_str(), nullptr, &type, nullptr, &size);
        if (query == ERROR_SUCCESS)
        {
            value.resize(size);
            if (RegQueryValueExW(
                    key, valueName.c_str(), nullptr, &type,
                    value.data(), &size) == ERROR_SUCCESS)
            {
                existed = true;
            }
            else
            {
                value.clear();
            }
        }
        RegDeleteValueW(key, valueName.c_str());
    }

    void Restore()
    {
        if (!key)
        {
            return;
        }
        if (existed)
        {
            RegSetValueExW(
                key, valueName.c_str(), 0, type, value.data(),
                static_cast<DWORD>(value.size()));
        }
        else
        {
            RegDeleteValueW(key, valueName.c_str());
        }
        RegCloseKey(key);
        key = nullptr;
    }
};

struct EnvironmentState
{
    bool existed = false;
    std::wstring value;

    void Clear()
    {
        SetLastError(ERROR_SUCCESS);
        const DWORD needed = GetEnvironmentVariableW(
            kCompatibilityEnvironment, nullptr, 0);
        if (needed)
        {
            std::vector<wchar_t> buffer(needed);
            GetEnvironmentVariableW(
                kCompatibilityEnvironment, buffer.data(), needed);
            value.assign(buffer.data());
            existed = true;
        }
        SetEnvironmentVariableW(kCompatibilityEnvironment, nullptr);
    }

    void Restore() const
    {
        SetEnvironmentVariableW(
            kCompatibilityEnvironment, existed ? value.c_str() : nullptr);
    }
};

struct LaunchCleanup
{
    std::wstring subst;
    wchar_t drive = L'\0';
    CompatibilityState compatibility;
    EnvironmentState environment;
    bool finished = false;

    void Finish()
    {
        if (finished)
        {
            return;
        }
        compatibility.Restore();
        environment.Restore();
        ChangeDisplaySettingsW(nullptr, 0);
        UnmapDrive(subst, drive);
        Log(L"Cleanup complete drive=%lc", drive ? drive : L'-');
        finished = true;
    }

    ~LaunchCleanup()
    {
        Finish();
    }
};
}

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int)
{
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOOPENFILEERRORBOX);

    const std::wstring directory = ExecutableDirectory();
    if (directory.empty())
    {
        return ShowError(L"The launcher directory could not be determined.", GetLastError());
    }
    g_logPath = JoinPath(directory, L"GameVaultLauncher.log");
    Log(L"Launcher 1.0.0 starting directory=%ls", directory.c_str());

    const HANDLE mutex = CreateMutexW(nullptr, TRUE, kLauncherMutex);
    if (!mutex)
    {
        return ShowError(L"The launcher instance control could not be created.", GetLastError());
    }
    if (GetLastError() == ERROR_ALREADY_EXISTS || IsLionKingRunning())
    {
        CloseHandle(mutex);
        return ShowError(L"Disney's The Lion King is already running. Close it before starting it again.");
    }

    const std::wstring game = JoinPath(directory, L"LIONW.EXE");
    const std::wstring ddraw = JoinPath(directory, L"ddraw.dll");
    if (!FileExists(game) || !FileExists(ddraw))
    {
        CloseHandle(mutex);
        return ShowError(
            L"LIONW.EXE or ddraw.dll is missing beside the launcher. "
            L"Do not move Play Lion King.exe outside the game directory.");
    }

    const std::wstring pendingDdraw = JoinPath(directory, L"ddraw.next.dll");
    if (FileExists(pendingDdraw))
    {
        if (!CopyFileW(pendingDdraw.c_str(), ddraw.c_str(), FALSE))
        {
            const DWORD error = GetLastError();
            CloseHandle(mutex);
            return ShowError(L"The graphics compatibility layer could not be updated.", error);
        }
        DeleteFileW(pendingDdraw.c_str());
        Log(L"Applied pending ddraw.next.dll");
    }

    RotateGameLog(directory);
    LaunchCleanup cleanup;
    cleanup.subst = SubstExecutable();
    cleanup.drive = PickVirtualDrive();
    if (cleanup.subst.empty() || !cleanup.drive)
    {
        CloseHandle(mutex);
        return ShowError(L"No temporary drive letter is available.");
    }
    if (!MapDrive(cleanup.subst, cleanup.drive, directory))
    {
        const DWORD error = GetLastError();
        cleanup.drive = L'\0';
        CloseHandle(mutex);
        return ShowError(L"The temporary short path for the game could not be created.", error);
    }

    const std::wstring virtualRoot = std::wstring(1, cleanup.drive) + L":\\";
    const std::wstring virtualGame = virtualRoot + L"LIONW.EXE";
    cleanup.compatibility.RemoveFor(virtualGame);
    cleanup.environment.Clear();
    Log(L"Launching game path=%ls", virtualGame.c_str());

    std::wstring command = Quote(virtualGame);
    STARTUPINFOW startup = {};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process = {};
    if (!CreateProcessW(
            virtualGame.c_str(), command.data(), nullptr, nullptr, FALSE,
            CREATE_DEFAULT_ERROR_MODE, nullptr, virtualRoot.c_str(),
            &startup, &process))
    {
        const DWORD error = GetLastError();
        CloseHandle(mutex);
        return ShowError(L"Windows could not open LIONW.EXE.", error);
    }

    CloseHandle(process.hThread);
    WaitForSingleObject(process.hProcess, INFINITE);
    DWORD exitCode = 0;
    GetExitCodeProcess(process.hProcess, &exitCode);
    CloseHandle(process.hProcess);
    Log(L"Game exited code=%lu", exitCode);

    cleanup.Finish();
    ReleaseMutex(mutex);
    CloseHandle(mutex);
    return 0;
}
