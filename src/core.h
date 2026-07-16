#pragma once

#include <chrono>

using TimeStamp = std::chrono::steady_clock::time_point;
using TimeDuration = std::chrono::steady_clock::duration;

struct RumbleSignal
{
    TimeStamp time;
    float left = 0.0f;
    float right = 0.0f;
};

enum class BLEConnectionState
{
    Disconnected,
    Connecting,
    Connected,
    Failed
};

struct BLEAdvertisementInfo
{
    std::string name;
    // Address is used as index
    std::int16_t rssi{};
    TimeStamp lastSeen;
	BLEConnectionState connectionState = BLEConnectionState::Disconnected;
};

struct BLEDeviceInfo
{
    std::string name;
    std::uint64_t address{};
};
