#include <cassert>
#include <cstdint>
#include <iostream>

#include "SteamController2026.h"

namespace
{
using namespace steam_controller_2026;

void test_device_ids()
{
    assert(isSupportedDevice(kValveVendorId, kControllerUsbProductId));
    assert(isSupportedDevice(kValveVendorId, kControllerBleProductId));
    assert(isSupportedDevice(kValveVendorId, kProteusPuckProductId));
    assert(isSupportedDevice(kValveVendorId, kNereidReceiverProductId));
    assert(!isSupportedDevice(kValveVendorId, 0x1142));
    assert(!isSupportedDevice(0x054C, kControllerUsbProductId));

    assert(connectionKind(kControllerUsbProductId) == ConnectionKind::Usb);
    assert(connectionKind(kControllerBleProductId) == ConnectionKind::Bluetooth);
    assert(connectionKind(kProteusPuckProductId) == ConnectionKind::ProteusPuck);
    assert(connectionKind(kNereidReceiverProductId) == ConnectionKind::NereidReceiver);
    assert(connectionKindName(ConnectionKind::ProteusPuck) == "Proteus Puck");
}

void test_two_touchpads()
{
    assert(kTouchpadCount == 2);
    assert(kTouchpadFingerCount == 1);
    assert(kTouchpadSizeX == 1920);
    assert(kTouchpadSizeY == 920);
}

void test_raw_button_contract()
{
    const auto buttons = allRawButtons();
    assert(buttons.size() == 7);
    assert(rawButtonIndex(RawButton::Qam) == 11);
    assert(rawButtonIndex(RawButton::RightPaddle1) == 12);
    assert(rawButtonIndex(RawButton::LeftPaddle1) == 13);
    assert(rawButtonIndex(RawButton::RightPaddle2) == 14);
    assert(rawButtonIndex(RawButton::LeftPaddle2) == 15);
    assert(rawButtonIndex(RawButton::RightPadClick) == 16);
    assert(rawButtonIndex(RawButton::LeftPadClick) == 17);

    const auto capSense = allCapSenseInputs();
    assert(capSense.size() == 4);
}
} // namespace

int main()
{
    test_device_ids();
    test_two_touchpads();
    test_raw_button_contract();
    std::cout << "steam_controller_2026_tests: passed\n";
    return 0;
}