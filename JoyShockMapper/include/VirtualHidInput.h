#pragma once
#ifdef _WIN32
#include <windows.h>
#include <array>
#include <mutex>
#include <set>

// Compatible with FakerInput's public v1 HID protocol (Ryochan7/FakerInput).
// No service, token impersonation or changes to Windows security policy.
class VirtualHidInput {
    friend struct VirtualHidInputTest;
    HANDLE control = INVALID_HANDLE_VALUE, methods = INVALID_HANDLE_VALUE;
    std::mutex mutex;
    ULONGLONG retryAfter = 0;
    std::set<unsigned char> keys;
    unsigned char modifiers = 0, buttons = 0;
    unsigned int media = 0;
    bool connect();
    bool write(const unsigned char *payload, size_t size);
    bool keyboard();
    void close();
public:
    static VirtualHidInput &instance();
    ~VirtualHidInput();
    bool available();
    bool key(WORD vk, bool down);
    bool character(wchar_t value);
    bool mouseButton(WORD vk, bool down);
    bool relative(int x, int y, int wheel = 0, int horizontalWheel = 0);
    bool absolute(unsigned short x, unsigned short y);
    void releaseAll();
};
#endif
