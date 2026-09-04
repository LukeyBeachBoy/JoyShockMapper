#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

struct TelemetryStickState
{
	float x = 0.0f;
	float y = 0.0f;
};

struct TelemetryTriggerState
{
	float left = 0.0f;
	float right = 0.0f;
};

struct TelemetryGyroState
{
	float x = 0.0f;
	float y = 0.0f;
	float z = 0.0f;
};

struct TelemetryPadState
{
	float x = 0.0f;        // -1..1 normalized position
	float y = 0.0f;
	bool touched = false;  // finger on pad
	float pressure = 0.0f; // raw driver pressure (diagnostic for threshold tuning)
};

struct TelemetryGripState
{
	// The grip signal is a single bit on the wire (SDL capacitive sense over
	// TRITON_LEFT/RIGHT_GRIP_TOUCH); how hard you must squeeze to set it is
	// decided in the controller from LEFT_GRIP_RANGE / RIGHT_GRIP_RANGE.
	bool pressed = false;
};

struct TelemetryDeviceStatus
{
	uint64_t buttons = 0;
	TelemetryStickState leftStick;
	TelemetryStickState rightStick;
	TelemetryTriggerState triggers;
	TelemetryGyroState gyro;
	TelemetryPadState leftPad;
	TelemetryPadState rightPad;
	TelemetryGripState leftGrip;
	TelemetryGripState rightGrip;
	// Capacitive thumbstick contact, the same kind of signal as the pads and grips.
	bool leftStickTouch = false;
	bool rightStickTouch = false;
};

struct TelemetryDevice
{
	int handle = 0;
	int controllerType = 0;
	int splitType = 0;
	int vendorId = 0;
	int productId = 0;
	std::optional<TelemetryDeviceStatus> status;
};

struct TelemetrySample
{
	uint64_t timestampMs = 0;
	float omega = 0.0f;
	float normalized = 0.0f;
	float sensX = 0.0f;
	float sensY = 0.0f;
	float minThreshold = 0.0f;
	float maxThreshold = 0.0f;
	float sMinX = 0.0f;
	float sMaxX = 0.0f;
	float sMinY = 0.0f;
	float sMaxY = 0.0f;
	std::string curve = "LINEAR";
	std::string paramsJson = "{}";
	std::vector<TelemetryDevice> devices;
	float sampleRateHz = 0.0f;
};

namespace Telemetry
{

constexpr int kProtoVersion = 3;
constexpr int kDefaultPort = 8974;
constexpr int kMaxRateHz = 120;

void Configure(bool enabled, uint16_t port);
void Shutdown();
void MaybeSend(const TelemetrySample &sample);

} // namespace Telemetry
