#include "StudioFeedback.h"

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <mutex>
#include <thread>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#endif

namespace
{

#ifdef _WIN32
using SocketHandle = SOCKET;
constexpr SocketHandle kInvalidSocket = INVALID_SOCKET;
void closeHandle(SocketHandle socket) { closesocket(socket); }
#else
using SocketHandle = int;
constexpr SocketHandle kInvalidSocket = -1;
void closeHandle(SocketHandle socket) { ::close(socket); }
#endif

std::mutex requestMutex;
StudioFeedback::Request latest;
std::atomic<uint32_t> sequence{ 0 };

std::mutex lifecycleMutex;
std::thread listener;
std::atomic<bool> running{ false };

void receiveLoop(SocketHandle socket)
{
	char buffer[128];
	while (running.load())
	{
		const auto received = recv(socket, buffer, sizeof(buffer) - 1, 0);
		if (received <= 0)
			continue; // timeout (to notice Stop), or a transient error
		buffer[received] = '\0';
		StudioFeedback::Request request;
		if (!StudioFeedback::Parse(buffer, request))
			continue;
		request.received = std::chrono::steady_clock::now();
		std::lock_guard<std::mutex> lock(requestMutex);
		request.sequence = sequence.load() + 1;
		latest = request;
		sequence.store(request.sequence);
	}
	closeHandle(socket);
}

} // namespace

namespace StudioFeedback
{

bool Parse(const char *text, Request &out)
{
	int effect = 0, side = 0, rumbleMs = 0, target = 0;
	float intensity = 0.f, rumble = 0.f;
	// The target is optional, so a Studio that predates it still parses.
	const int fields = std::sscanf(text, "FEEDBACK %d %f %d %d %f %d", &effect, &intensity, &side, &rumbleMs, &rumble, &target);
	if (fields != 5 && fields != 6)
		return false;
	// TICK .. TAP: HapticEffect's playable range (OFF is 0, INVALID after TAP).
	if (effect < 1 || effect > 9 || side < 1 || side > 3)
		return false;
	out.effect = effect;
	out.intensity = std::clamp(intensity, 0.f, 100.f);
	out.side = side;
	out.rumbleMs = std::clamp(rumbleMs, 0, 250);
	out.rumble = std::clamp(rumble, 0.f, 100.f);
	out.grips = fields == 6 && target == 1;
	return true;
}

void Start(uint16_t port)
{
	std::lock_guard<std::mutex> lock(lifecycleMutex);
	if (running.load())
		return;
#ifdef _WIN32
	WSADATA data;
	if (WSAStartup(MAKEWORD(2, 2), &data) != 0)
		return;
#endif
	SocketHandle socket = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (socket == kInvalidSocket)
		return;
	sockaddr_in address{};
	address.sin_family = AF_INET;
	address.sin_port = htons(port);
#ifdef _WIN32
	InetPtonA(AF_INET, "127.0.0.1", &address.sin_addr);
	DWORD timeout = 250;
	setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char *>(&timeout), sizeof(timeout));
#else
	inet_pton(AF_INET, "127.0.0.1", &address.sin_addr);
	timeval timeout{ 0, 250000 };
	setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
#endif
	if (bind(socket, reinterpret_cast<sockaddr *>(&address), sizeof(address)) != 0)
	{
		// Another mapper already has it; Studio talks to that one.
		std::cerr << "Studio feedback port " << port << " is in use; controller feedback from JSM Studio is off for this instance.\n";
		closeHandle(socket);
		return;
	}
	running.store(true);
	listener = std::thread(receiveLoop, socket);
}

void Stop()
{
	std::lock_guard<std::mutex> lock(lifecycleMutex);
	if (!running.exchange(false))
		return;
	if (listener.joinable())
		listener.join();
}

uint32_t Sequence()
{
	return sequence.load(std::memory_order_relaxed);
}

Request Latest()
{
	std::lock_guard<std::mutex> lock(requestMutex);
	return latest;
}

} // namespace StudioFeedback
