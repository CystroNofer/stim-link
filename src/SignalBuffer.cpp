#include "SignalBuffer.h"

SignalBuffer::SignalBuffer()
{
    m_Buffer.resize(BUFFER_MAX_SIZE + 1);
    m_Snapshot.store(std::move(std::make_shared<const std::vector<Signal>>(
        m_Buffer.end() - std::min(BUFFER_SIZE, m_Buffer.size()),
        m_Buffer.end()
    )));
}

void SignalBuffer::Push(float left, float right)
{
    std::scoped_lock lock(m_Mutex);

	if (m_Saturate.load())
	{
        m_Buffer.push_back(Signal{
            .time = std::chrono::steady_clock::now(),
            .left = left > 0.0f ? 1.0f : 0.0f,
            .right = right > 0.0f ? 1.0f : 0.0f,
            });
	}
    else {
        m_Buffer.push_back(Signal{
            .time = std::chrono::steady_clock::now(),
            .left = left,
            .right = right,
            });
    }

    if (m_Buffer.size() >= BUFFER_MAX_SIZE)
    {
        m_Buffer.erase(
            m_Buffer.begin(),
            m_Buffer.begin() + (m_Buffer.size() - BUFFER_SIZE)
        );
    }

    m_Snapshot.store(std::move(std::make_shared<const std::vector<Signal>>(
        m_Buffer.end() - std::min(BUFFER_SIZE, m_Buffer.size()),
        m_Buffer.end()
    )));
}
