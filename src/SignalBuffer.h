#pragma once

#include <cstdint>
#include <vector>
#include <mutex>

#include "Waveform.h"

class SignalBuffer
{
public:
    SignalBuffer(TimeDuration swapInterval = std::chrono::milliseconds(500));

    void Push(std::uint8_t left, std::uint8_t right);

    [[nodiscard]]
    const std::vector<RumbleSignal>& GetSnapshot();

private:
    mutable std::mutex m_Mutex;

    std::vector<RumbleSignal> m_WriteBuffer;
    std::vector<RumbleSignal> m_ReadBuffer;

    const std::chrono::steady_clock::time_point m_StartTime;

    TimeDuration m_SwapInterval;
    TimeStamp m_LastSwapTime;
};
