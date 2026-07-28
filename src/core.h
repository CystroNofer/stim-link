#pragma once

#define NOMINMAX

#include <chrono>
#include <memory>
#include <vector>
#include <string>

using TimeStamp = std::chrono::steady_clock::time_point;
using TimeDuration = std::chrono::steady_clock::duration;

// Based on Coyote
constexpr TimeDuration OUTPUT_INTERVAL = std::chrono::milliseconds(100);
constexpr float MAX_STRENGTH = 200.0f;

struct RumbleSignal
{
    TimeStamp time;
    float left = 0.0f;
    float right = 0.0f;
};

using SignalBufferSnapshot = std::shared_ptr<const std::vector<RumbleSignal>>;

template <std::size_t SampleCount>
struct WaveformSample
{
    float maxStrengthL = 0.0f;
    float maxStrengthR = 0.0f;

    std::array<float, SampleCount> waveformStrengthL{};
    std::array<float, SampleCount> waveformStrengthR{};
};

enum class BLEDeviceType
{
    Unknown,
    CoyoteV3,
    PawPrintV1_1
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
    BLEDeviceType type;
    // Address is used as index
    std::int16_t rssi{};
    TimeStamp lastSeen;
	BLEConnectionState connectionState = BLEConnectionState::Disconnected;
};
