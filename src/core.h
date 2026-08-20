#pragma once

#define NOMINMAX

#include <chrono>
#include <memory>
#include <vector>
#include <string>

// Input mode
enum class InputMode { VirtualController, SystemAudio };

// Time
using TimeStamp = std::chrono::steady_clock::time_point;
using TimeDuration = std::chrono::steady_clock::duration;

// Based on Coyote
constexpr TimeDuration AUDIO_BLOCK_DURATION = std::chrono::milliseconds(25);
constexpr TimeDuration OUTPUT_INTERVAL = std::chrono::milliseconds(100);
constexpr float MAX_STRENGTH = 200.0f;

// Signal & Buffer
constexpr size_t BUFFER_SIZE = 100;
constexpr size_t BUFFER_MAX_SIZE = 150;

struct Signal
{
    TimeStamp time;
    float left = 0.0f;
    float right = 0.0f;
};

using SignalBufferSnapshot = std::shared_ptr<const std::vector<Signal>>;

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
