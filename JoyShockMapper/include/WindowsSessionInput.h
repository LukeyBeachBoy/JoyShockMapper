#pragma once
#include <cstdint>
#include <functional>
#include <string>
#ifdef _WIN32
bool windowsSessionLocked();
bool captureWindowsSignIn(int handle,uint64_t buttons,const std::string &profile,const std::function<void()> &releaseNormal,const std::function<void()> &requestPowerOff);
bool windowsSignInCapturing(int handle);
void forgetWindowsSignInController(int handle);
#endif
