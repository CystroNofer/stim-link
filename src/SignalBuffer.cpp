#include "SignalBuffer.h"

constexpr size_t BUFFER_SIZE = 100;
constexpr size_t BUFFER_MAX_SIZE = 150;

SignalBuffer::SignalBuffer(TimeDuration swapInterval)
    : m_StartTime(std::chrono::steady_clock::now()),
    m_SwapInterval(swapInterval),
    m_LastSwapTime(std::chrono::steady_clock::now())
{
    m_WriteBuffer.reserve(BUFFER_MAX_SIZE + 1);
    m_ReadBuffer.reserve(BUFFER_SIZE);
}

void SignalBuffer::Push(std::uint8_t left, std::uint8_t right)
{
    std::scoped_lock lock(m_Mutex);

    m_WriteBuffer.push_back(RumbleSignal{
        .time = std::chrono::steady_clock::now(),
        .left = static_cast<float>(left) / 255.0f,
        .right = static_cast<float>(right) / 255.0f,
        });

    if (m_WriteBuffer.size() >= BUFFER_MAX_SIZE)
    {
        m_WriteBuffer.erase(
            m_WriteBuffer.begin(),
            m_WriteBuffer.begin() + (m_WriteBuffer.size() - BUFFER_SIZE)
        );
    }
}

const std::vector<RumbleSignal>& SignalBuffer::GetSnapshot()
{
    if (std::chrono::steady_clock::now() - m_LastSwapTime >= m_SwapInterval)
    {
        std::scoped_lock lock(m_Mutex);
        m_ReadBuffer.assign(
            m_WriteBuffer.end() - std::min(BUFFER_SIZE, m_WriteBuffer.size()),
            m_WriteBuffer.end()
        );
        m_LastSwapTime = std::chrono::steady_clock::now();
    }

    //std::scoped_lock lock(m_Mutex);
    return m_ReadBuffer;
}
