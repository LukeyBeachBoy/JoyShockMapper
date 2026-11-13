#pragma once
#ifdef _WIN32
#include <windows.h>
#include <tlhelp32.h>
namespace AzeronNative {
inline bool vendorAppRunning() {
    HANDLE list = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (list == INVALID_HANDLE_VALUE) return true;
    PROCESSENTRY32W entry{};
    entry.dwSize = sizeof(entry);
    bool found = false;
    if (Process32FirstW(list, &entry)) do {
        if (_wcsicmp(entry.szExeFile, L"Azeron Software.exe") == 0) { found = true; break; }
    } while (Process32NextW(list, &entry));
    CloseHandle(list);
    return found;
}
}
#endif
