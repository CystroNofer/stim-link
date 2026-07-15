#pragma once

#include <vector>
#include <chrono>

using TimeStamp = std::chrono::steady_clock::time_point;
using TimeDuration = std::chrono::steady_clock::duration;

struct RumbleSignal
{
    TimeStamp time;
    float left = 0.0f;
    float right = 0.0f;
};
