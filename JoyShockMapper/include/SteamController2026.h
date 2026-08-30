#pragma once

#include <array>
#include <cstdint>
#include <string_view>

namespace steam_controller_2026
{

// Valve's Triton controller family as identified by SDL's HIDAPI driver.
constexpr uint16_t kValveVendorId = 0x28DE;
constexpr uint16_t kControllerUsbProductId = 0x1302;
constexpr uint16_t kControllerBleProductId = 0x1303;
constexpr uint16_t kProteusPuckProductId = 0x1304;
constexpr uint16_t kNereidReceiverProductId = 0x1305;

constexpr int kTouchpadCount = 2;
constexpr int kTouchpadFingerCount = 1;
// SDL reports normalized coordinates. JSM uses this logical resolution to
// convert movement into the units used by its existing touch-stick settings.
constexpr int kTouchpadSizeX = 1920;
constexpr int kTouchpadSizeY = 920;

enum class ConnectionKind
{
    Usb,
    Bluetooth,
    ProteusPuck,
    NereidReceiver,
    Unknown,
};

constexpr bool isSupportedDevice(uint16_t vendorId, uint16_t productId)
{
    return vendorId == kValveVendorId &&
           (productId == kControllerUsbProductId ||
            productId == kControllerBleProductId ||
            productId == kProteusPuckProductId ||
            productId == kNereidReceiverProductId);
}

constexpr ConnectionKind connectionKind(uint16_t productId)
{
    switch (productId)
    {
    case kControllerUsbProductId:
        return ConnectionKind::Usb;
    case kControllerBleProductId:
        return ConnectionKind::Bluetooth;
    case kProteusPuckProductId:
        return ConnectionKind::ProteusPuck;
    case kNereidReceiverProductId:
        return ConnectionKind::NereidReceiver;
    default:
        return ConnectionKind::Unknown;
    }
}

constexpr std::string_view connectionKindName(ConnectionKind kind)
{
    switch (kind)
    {
    case ConnectionKind::Usb:
        return "USB";
    case ConnectionKind::Bluetooth:
        return "Bluetooth";
    case ConnectionKind::ProteusPuck:
        return "Proteus Puck";
    case ConnectionKind::NereidReceiver:
        return "Nereid receiver";
    default:
        return "Unknown";
    }
}

// SDL's Triton HID driver sends these as raw joystick buttons. The values are
// the raw joystick indices used by SDL's generated Steam Controller mapping.
enum class RawButton : int
{
    Qam = 11,
    RightPaddle1 = 12,
    LeftPaddle1 = 13,
    RightPaddle2 = 14,
    LeftPaddle2 = 15,
    RightPadClick = 16,
    LeftPadClick = 17,
};

constexpr int rawButtonIndex(RawButton button)
{
    return static_cast<int>(button);
}

constexpr std::array<RawButton, 7> allRawButtons()
{
    return {
        RawButton::Qam,
        RawButton::RightPaddle1,
        RawButton::LeftPaddle1,
        RawButton::RightPaddle2,
        RawButton::LeftPaddle2,
        RawButton::RightPadClick,
        RawButton::LeftPadClick,
    };
}

enum class CapSense
{
    LeftStick,
    RightStick,
    LeftGrip,
    RightGrip,
};

constexpr std::array<CapSense, 4> allCapSenseInputs()
{
    return {
        CapSense::LeftStick,
        CapSense::RightStick,
        CapSense::LeftGrip,
        CapSense::RightGrip,
    };
}

} // namespace steam_controller_2026

static_assert(steam_controller_2026::isSupportedDevice(
    steam_controller_2026::kValveVendorId,
    steam_controller_2026::kControllerUsbProductId));
static_assert(steam_controller_2026::kTouchpadCount == 2);
static_assert(steam_controller_2026::rawButtonIndex(
    steam_controller_2026::RawButton::LeftPadClick) == 17);
