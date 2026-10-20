#include "VirtualHidInput.h"
#include <vector>

extern "C" __declspec(dllexport) int jsm_hid_key(unsigned short key, int down) {
    return VirtualHidInput::instance().key(key,down!=0) ? 1 : 0;
}
extern "C" __declspec(dllexport) int jsm_hid_text(const wchar_t *text,unsigned int length) {
    if(!text || length>1024)return 0;
    std::vector<SHORT> strokes;strokes.reserve(length);
    for(unsigned int i=0;i<length;++i) {
        SHORT stroke=VkKeyScanExW(text[i],GetKeyboardLayout(0));
        if(stroke==-1)return 0; // validate the whole text before sending anything
        strokes.push_back(stroke);
    }
    auto &hid=VirtualHidInput::instance();if(!hid.available())return 0;
    for(auto stroke:strokes) {
        const BYTE modifiers=HIBYTE(stroke);const WORD key=LOBYTE(stroke);
        if(modifiers&1)hid.key(VK_LSHIFT,true);
        if(modifiers&2)hid.key(VK_LCONTROL,true);
        if(modifiers&4)hid.key(VK_LMENU,true);
        const bool sent=hid.key(key,true) && hid.key(key,false);
        if(modifiers&4)hid.key(VK_LMENU,false);
        if(modifiers&2)hid.key(VK_LCONTROL,false);
        if(modifiers&1)hid.key(VK_LSHIFT,false);
        if(!sent){hid.releaseAll();return -1;}
    }
    return 1;
}
