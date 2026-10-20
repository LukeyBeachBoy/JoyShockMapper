#include "VirtualHidInput.h"
#include <hidsdi.h>
#include <setupapi.h>
#include <algorithm>
#include <vector>
#include <cstring>

namespace {
HANDLE findEndpoint(USHORT usage) {
    GUID hid{}; HidD_GetHidGuid(&hid);
    HDEVINFO info = SetupDiGetClassDevsW(&hid, nullptr, nullptr, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
    if (info == INVALID_HANDLE_VALUE) return INVALID_HANDLE_VALUE;
    HANDLE found = INVALID_HANDLE_VALUE;
    SP_DEVICE_INTERFACE_DATA entry{}; entry.cbSize = sizeof(entry);
    for (DWORD i = 0; SetupDiEnumDeviceInterfaces(info, nullptr, &hid, i, &entry); ++i) {
        DWORD size = 0;
        SetupDiGetDeviceInterfaceDetailW(info, &entry, nullptr, 0, &size, nullptr);
        if (!size) continue;
        std::vector<unsigned char> storage(size);
        auto detail = reinterpret_cast<SP_DEVICE_INTERFACE_DETAIL_DATA_W *>(storage.data());
        detail->cbSize = sizeof(*detail);
        if (!SetupDiGetDeviceInterfaceDetailW(info, &entry, detail, size, nullptr, nullptr)) continue;
        HANDLE file = CreateFileW(detail->DevicePath, GENERIC_READ | GENERIC_WRITE,
            FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
        if (file == INVALID_HANDLE_VALUE) continue;
        HIDD_ATTRIBUTES attributes{}; attributes.Size = sizeof(attributes);
        PHIDP_PREPARSED_DATA data = nullptr; HIDP_CAPS caps{};
        const bool match = HidD_GetAttributes(file, &attributes) && attributes.VendorID == 0xfe0f &&
            attributes.ProductID == 0x00ff && HidD_GetPreparsedData(file, &data) &&
            HidP_GetCaps(data, &caps) == HIDP_STATUS_SUCCESS && caps.UsagePage == 0xff00 && caps.Usage == usage &&
            caps.OutputReportByteLength == 65;
        if (data) HidD_FreePreparsedData(data);
        if (match) { found = file; break; }
        CloseHandle(file);
    }
    SetupDiDestroyDeviceInfoList(info);
    return found;
}

unsigned char keyUsage(WORD vk) {
    if (vk >= 'A' && vk <= 'Z') return static_cast<unsigned char>(4 + vk - 'A');
    if (vk >= '1' && vk <= '9') return static_cast<unsigned char>(0x1e + vk - '1');
    if (vk == '0') return 0x27;
    if (vk >= VK_F1 && vk <= VK_F12) return static_cast<unsigned char>(0x3a + vk - VK_F1);
    // This driver's keyboard descriptor ends at usage 0x65; retain SendInput
    // for F13-F24 rather than submitting usages it cannot report.
    if (vk >= VK_F13 && vk <= VK_F24) return 0;
    if (vk >= VK_NUMPAD1 && vk <= VK_NUMPAD9) return static_cast<unsigned char>(0x59 + vk - VK_NUMPAD1);
    switch (vk) {
    case VK_RETURN:return 0x28;case VK_ESCAPE:return 0x29;case VK_BACK:return 0x2a;
    case VK_TAB:return 0x2b;case VK_SPACE:return 0x2c;case VK_CAPITAL:return 0x39;
    case VK_SNAPSHOT:return 0x46;case VK_SCROLL:return 0x47;case VK_PAUSE:return 0x48;
    case VK_INSERT:return 0x49;case VK_HOME:return 0x4a;case VK_PRIOR:return 0x4b;
    case VK_DELETE:return 0x4c;case VK_END:return 0x4d;case VK_NEXT:return 0x4e;
    case VK_RIGHT:return 0x4f;case VK_LEFT:return 0x50;case VK_DOWN:return 0x51;case VK_UP:return 0x52;
    case VK_NUMLOCK:return 0x53;case VK_DIVIDE:return 0x54;case VK_MULTIPLY:return 0x55;
    case VK_SUBTRACT:return 0x56;case VK_ADD:return 0x57;case VK_NUMPAD0:return 0x62;
    case VK_DECIMAL:return 0x63;case VK_APPS:return 0x65;
    }
    // OEM virtual keys vary by keyboard layout. Map their physical scan code.
    switch (MapVirtualKeyW(vk, MAPVK_VK_TO_VSC) & 0xff) {
    case 0x0c:return 0x2d;case 0x0d:return 0x2e;case 0x1a:return 0x2f;case 0x1b:return 0x30;
    case 0x2b:return 0x31;case 0x27:return 0x33;case 0x28:return 0x34;case 0x29:return 0x35;
    case 0x33:return 0x36;case 0x34:return 0x37;case 0x35:return 0x38;case 0x56:return 0x64;
    default:return 0;
    }
}

unsigned int mediaBit(WORD vk) {
    switch (vk) {
    case VK_LAUNCH_APP1:return 1u<<0;case VK_LAUNCH_APP2:return 1u<<1;
    case VK_BROWSER_SEARCH:return 1u<<2;case VK_BROWSER_HOME:return 1u<<3;
    case VK_BROWSER_BACK:return 1u<<4;case VK_BROWSER_FORWARD:return 1u<<5;
    case VK_LAUNCH_MEDIA_SELECT:return 1u<<6;case VK_LAUNCH_MAIL:return 1u<<7;
    case VK_MEDIA_NEXT_TRACK:return 1u<<8;case VK_MEDIA_PREV_TRACK:return 1u<<9;
    case VK_MEDIA_STOP:return 1u<<10;case VK_MEDIA_PLAY_PAUSE:return 1u<<11;
    case VK_VOLUME_MUTE:return 1u<<12;case VK_VOLUME_DOWN:return 1u<<13;case VK_VOLUME_UP:return 1u<<14;
    case VK_BROWSER_STOP:return 1u<<16;case VK_BROWSER_REFRESH:return 1u<<17;case VK_BROWSER_FAVORITES:return 1u<<18;
    default:return 0;
    }
}
unsigned char modifierBit(WORD vk) {
    switch(vk) {
    case VK_CONTROL:case VK_LCONTROL:return 1;case VK_SHIFT:case VK_LSHIFT:return 2;
    case VK_MENU:case VK_LMENU:return 4;case VK_LWIN:return 8;
    case VK_RCONTROL:return 16;case VK_RSHIFT:return 32;case VK_RMENU:return 64;case VK_RWIN:return 128;
    default:return 0;
    }
}
}

VirtualHidInput &VirtualHidInput::instance() { static VirtualHidInput value; return value; }
void VirtualHidInput::close() {
    if (control != INVALID_HANDLE_VALUE) CloseHandle(control);
    if (methods != INVALID_HANDLE_VALUE) CloseHandle(methods);
    control = methods = INVALID_HANDLE_VALUE;
    retryAfter = GetTickCount64() + 5000;
}
bool VirtualHidInput::connect() {
    if (control != INVALID_HANDLE_VALUE) return true;
    if (GetTickCount64() < retryAfter) return false;
    control = findEndpoint(1); methods = findEndpoint(2);
    if (control == INVALID_HANDLE_VALUE || methods == INVALID_HANDLE_VALUE) { close(); return false; }
    std::array<unsigned char,65> api{}; api[0] = 0x41; api[4] = 1;
    DWORD written = 0;
    if (!WriteFile(methods, api.data(), DWORD(api.size()), &written, nullptr) || written != api.size()) { close(); return false; }
    keys.clear(); modifiers = buttons = 0; media = 0;
    // A prior mapper crash must not leave modifiers or mouse buttons held.
    const unsigned char emptyKeys[9] = {1}, emptyMedia[4] = {2}, emptyMouse[8] = {3};
    return write(emptyKeys,sizeof(emptyKeys)) && write(emptyMedia,sizeof(emptyMedia)) && write(emptyMouse,sizeof(emptyMouse));
}
bool VirtualHidInput::write(const unsigned char *payload, size_t size) {
    if (control == INVALID_HANDLE_VALUE || size > 63) return false;
    std::array<unsigned char,65> report{}; report[0] = 0x40; report[1] = static_cast<unsigned char>(size);
    std::memcpy(report.data()+2, payload, size);
    DWORD written = 0;
    if (!WriteFile(control, report.data(), DWORD(report.size()), &written, nullptr) || written != report.size()) { close(); return false; }
    return true;
}
bool VirtualHidInput::keyboard() {
    unsigned char report[9] = {1, modifiers, 0};
    if (keys.size() > 6) std::fill(report+3,report+9,1); // HID rollover; never silently drop held keys
    else std::copy(keys.begin(),keys.end(),report+3);
    return write(report,sizeof(report));
}
bool VirtualHidInput::available() { std::lock_guard<std::mutex> lock(mutex); return connect(); }
bool VirtualHidInput::character(wchar_t value) {
    const auto stroke=VkKeyScanExW(value,GetKeyboardLayout(0));
    if(stroke==-1 || !available())return false;
    const BYTE flags=HIBYTE(stroke);const WORD vk=LOBYTE(stroke);
    if(flags&1)key(VK_LSHIFT,true);
    if(flags&2)key(VK_LCONTROL,true);
    if(flags&4)key(VK_LMENU,true);
    const bool sent=key(vk,true) && key(vk,false);
    if(flags&4)key(VK_LMENU,false);
    if(flags&2)key(VK_LCONTROL,false);
    if(flags&1)key(VK_LSHIFT,false);
    return sent;
}
bool VirtualHidInput::key(WORD vk, bool down) {
    const auto modifier = modifierBit(vk), usage = keyUsage(vk); const auto enhanced = mediaBit(vk);
    if (!modifier && !usage && !enhanced) return false;
    std::lock_guard<std::mutex> lock(mutex);
    if (!connect()) return false;
    if (enhanced) {
        media = down ? media | enhanced : media & ~enhanced;
        unsigned char report[4] = {2, static_cast<unsigned char>(media >> 8), static_cast<unsigned char>(media), static_cast<unsigned char>(media >> 16)};
        return write(report,sizeof(report));
    }
    if (modifier) modifiers = down ? modifiers | modifier : modifiers & ~modifier;
    else if (down) keys.insert(usage); else keys.erase(usage);
    return keyboard();
}
bool VirtualHidInput::relative(int x, int y, int wheel, int horizontalWheel) {
    std::lock_guard<std::mutex> lock(mutex);
    if (!connect()) return false;
    do {
        const auto dx = static_cast<short>(std::clamp(x,-32767,32767));
        const auto dy = static_cast<short>(std::clamp(y,-32767,32767));
        const auto dw = static_cast<signed char>(std::clamp(wheel,-127,127));
        const auto dh = static_cast<signed char>(std::clamp(horizontalWheel,-127,127));
        unsigned char report[8] = {3,buttons};
        std::memcpy(report+2,&dx,2);std::memcpy(report+4,&dy,2);report[6]=static_cast<unsigned char>(dw);report[7]=static_cast<unsigned char>(dh);
        if (!write(report,sizeof(report))) return false;
        x-=dx;y-=dy;wheel-=dw;horizontalWheel-=dh;
    } while(x || y || wheel || horizontalWheel);
    return true;
}
bool VirtualHidInput::mouseButton(WORD vk, bool down) {
    const unsigned char bit = vk==VK_LBUTTON?1:vk==VK_RBUTTON?2:vk==VK_MBUTTON?4:vk==VK_XBUTTON1?8:vk==VK_XBUTTON2?16:0;
    if (!bit) return false;
    std::lock_guard<std::mutex> lock(mutex);
    if (!connect()) return false;
    buttons = down ? buttons | bit : buttons & ~bit;
    unsigned char report[8] = {3,buttons}; return write(report,sizeof(report));
}
bool VirtualHidInput::absolute(unsigned short x, unsigned short y) {
    std::lock_guard<std::mutex> lock(mutex);
    if (!connect()) return false;
    unsigned char report[8] = {4,buttons};
    x = static_cast<unsigned short>(x/2); y = static_cast<unsigned short>(y/2);
    std::memcpy(report+2,&x,2);std::memcpy(report+4,&y,2);return write(report,sizeof(report));
}
void VirtualHidInput::releaseAll() {
    std::lock_guard<std::mutex> lock(mutex);
    keys.clear();modifiers=buttons=0;media=0;
    if(control != INVALID_HANDLE_VALUE) {
        keyboard();const unsigned char m[4]={2},r[8]={3};write(m,sizeof(m));write(r,sizeof(r));
    }
}
VirtualHidInput::~VirtualHidInput() { releaseAll(); close(); }
