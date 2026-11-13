#pragma once
#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <span>
#include <string_view>

// Cyborg V2 vendor HID protocol. Commands here are volatile mode/report controls
// and read-only queries; no profile, calibration or firmware writes are used.
namespace AzeronNative {
constexpr uint16_t vendor = 0x16d0, product = 0x12f7;
constexpr uint64_t supported = ((1ULL << 26) - 1) | (15ULL << 32);
inline std::array<unsigned char, 65> command(uint8_t type, uint8_t sequence,
                                            std::span<const uint8_t> payload = {}) {
    std::array<unsigned char, 65> result{};
    if (payload.size() > 57) return result;
    result[2] = uint8_t(payload.size()); result[3] = type; result[4] = sequence;
    result[5] = result[6] = 1; result[7] = uint8_t(payload.size());
    std::copy(payload.begin(), payload.end(), result.begin() + 8);
    return result;
}
inline int physicalBit(int id) {
    // Stable positions used by Studio's numbered drawing, independent of OBM.
    constexpr int numbers[] = {0,1,6,10,14,2,7,11,15,3,8,12,16,18,4,9,13,17,19,5,22,0,20,21};
    if (id > 0 && id < int(std::size(numbers))) return numbers[id] ? numbers[id]-1 : -1;
    switch (id) {
        case 28: return 32; case 31: return 33; case 30: return 34; case 29: return 35;
        case 36: return 22; case 37: return 23; case 38: return 25; case 41: return 24;
        default: return -1;
    }
}
inline int s16(std::span<const uint8_t> p, size_t i) { return int16_t(p[i] | (p[i+1] << 8)); }
struct Calibration {
    int xmin=0, xmax=0, ymin=0, ymax=0, xoffset=0, yoffset=0;
    bool valid=false;
    bool read(std::span<const uint8_t> p) {
        if (p.size()<28 || p[6]!=8 || (p[7]!=2 && p[7]!=3)) return false;
        Calibration next;
        next.xoffset=s16(p,8); next.yoffset=s16(p,10);
        next.xmin=s16(p,19); next.xmax=s16(p,21); next.ymin=s16(p,23); next.ymax=s16(p,25);
        const auto axisValid=[](int lo,int hi,int offset) {
            const float center=(lo+hi)*.5f+offset;
            return lo>=0 && hi<=1023 && hi-lo>=40 && center>lo && center<hi;
        };
        next.valid=axisValid(next.xmin,next.xmax,next.xoffset)&&axisValid(next.ymin,next.ymax,next.yoffset);
        if (next.valid) *this=next;
        return next.valid;
    }
    static float axis(int raw,int lo,int hi,int offset) {
        const float center=(lo+hi)*.5f+offset;
        return std::clamp((raw-center)/(raw<center ? center-lo : hi-center),-1.f,1.f);
    }
};
struct Input {
    Calibration calibration;
    uint64_t buttons=0;
    float x=0,y=0;
    bool software=false, rawSeen=false;
    int firmware=0;
    void clear() { buttons=0; x=y=0; rawSeen=false; }
    // Input reports may contain several CR/LF-delimited messages, padded by NUL.
    bool line(std::string_view text) {
        while (!text.empty() && (text.back()=='\r'||text.back()=='\n')) text.remove_suffix(1);
        const auto number=[](std::string_view v,int& out) {
            auto result=std::from_chars(v.data(),v.data()+v.size(),out);
            return result.ec==std::errc{} && result.ptr==v.data()+v.size();
        };
        if (text.starts_with("BP_") || text.starts_with("BR_")) {
            int id=0;
            if (!number(text.substr(3),id)) return false;
            int bit=physicalBit(id); if (bit<0) return false;
            if (text[1]=='P') buttons|=1ULL<<bit; else buttons&=~(1ULL<<bit);
            return true;
        }
        if (!text.starts_with("PJOY_24_") || !calibration.valid) return false;
        auto rest=text.substr(8); auto split=rest.find('_'); int rx=0,ry=0;
        if (split==rest.npos || !number(rest.substr(0,split),rx) || !number(rest.substr(split+1),ry)
            || rx<0 || rx>1023 || ry<0 || ry>1023) return false;
        x=Calibration::axis(rx,calibration.xmin,calibration.xmax,calibration.xoffset);
        y=-Calibration::axis(ry,calibration.ymin,calibration.ymax,calibration.yoffset);
        rawSeen=true; return true;
    }
    template<class Observer>
    bool report(std::span<const uint8_t> bytes, Observer&& observe) {
        if (bytes.size()>=7 && bytes[1]==0 && bytes[4]==1 && bytes[5]==1 && bytes[6]<=57
            && bytes[0]==bytes[6] && size_t(7+bytes[6])<=bytes.size()) {
            auto p=bytes.subspan(7,bytes[6]);
            if (bytes[2]==5 && p.size()==2 && p[1]==8) firmware=p[0];
            else if (bytes[2]==2) calibration.read(p);
            else if (bytes[2]==6 && p.size()==1) {
                software=p[0]==0; if (!software) clear();
                observe(*this, std::string_view{});
            }
            return false;
        }
        bool changed=false;
        std::string_view text(reinterpret_cast<const char*>(bytes.data()),bytes.size());
        text=text.substr(0,text.find('\0'));
        while (!text.empty()) {
            auto end=text.find('\n');
            const auto message=text.substr(0,end);
            if (line(message)) { changed=true; observe(*this, message); }
            if (end==text.npos) break;
            text.remove_prefix(end+1);
        }
        return changed;
    }
    bool report(std::span<const uint8_t> bytes) {
        return report(bytes, [](const Input&, std::string_view) {});
    }
};
}
