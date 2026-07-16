#include "SignalBuffer.h"

constexpr size_t BUFFER_SIZE = 100;
constexpr size_t BUFFER_MAX_SIZE = 150;

SignalBuffer::SignalBuffer()
	: m_Snapshot(std::make_shared<const std::vector<RumbleSignal>>(std::vector<RumbleSignal>{}))
    //m_LastSwapTime(std::chrono::steady_clock::now())
{
    m_Buffer.reserve(BUFFER_MAX_SIZE + 1);
}

void SignalBuffer::Push(std::uint8_t left, std::uint8_t right)
{
    std::scoped_lock lock(m_Mutex);

    m_Buffer.push_back(RumbleSignal{
        .time = std::chrono::steady_clock::now(),
        .left = static_cast<float>(left) / 255.0f,
        .right = static_cast<float>(right) / 255.0f,
        });

    if (m_Buffer.size() >= BUFFER_MAX_SIZE)
    {
        m_Buffer.erase(
            m_Buffer.begin(),
            m_Buffer.begin() + (m_Buffer.size() - BUFFER_SIZE)
        );
    }

    //if (std::chrono::steady_clock::now() - m_LastSwapTime > OUTPUT_INTERVAL)
    //{
    //}
    m_Snapshot.store(std::move(std::make_shared<const std::vector<RumbleSignal>>(
        m_Buffer.end() - std::min(BUFFER_SIZE, m_Buffer.size()),
        m_Buffer.end()
    )));

    //m_LastSwapTime = std::chrono::steady_clock::now();
}
