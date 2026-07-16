#pragma once

#include <cstdint>
#include <mutex>

#include "core.h"

class SignalBuffer
{
public:
    SignalBuffer();

    void Push(std::uint8_t left, std::uint8_t right);

    template <std::size_t nSamples>
    WaveformSample<nSamples> Sample(TimeDuration d)
    {
        // The shared pointer guarantees the vector remains alive
        const SignalBufferSnapshot snapshot = m_Snapshot.load();
        const std::vector<RumbleSignal>& signals = *snapshot;

        if (nSamples == 0 || signals.empty()) {
            return WaveformSample<nSamples>{};
        }

		const float strengthAmp = m_StrengthAmp.load();

        if (nSamples == 1) {
            RumbleSignal lastSignal = signals.back();
            return WaveformSample<nSamples>{
                .maxStrengthL = lastSignal.left,
                .maxStrengthR = lastSignal.right,
                .waveformStrengthL = { 1.0f },
                .waveformStrengthR = { 1.0f }
            };
        }

        TimeStamp sampleTime = std::chrono::steady_clock::now();
        TimeDuration sampleInterval = d / (nSamples - 1);
        size_t signalIndex = signals.size() - 1;
        WaveformSample<nSamples> res{
            .waveformStrengthL = std::array<float, nSamples>{},
            .waveformStrengthR = std::array<float, nSamples>{}
        };
        for (size_t i = 1; i <= nSamples; i++) {
            res.maxStrengthL = max(res.maxStrengthL, signals[signalIndex].left);
            res.maxStrengthR = max(res.maxStrengthR, signals[signalIndex].right);
            res.waveformStrengthL[nSamples - i] = signals[signalIndex].left;
            res.waveformStrengthR[nSamples - i] = signals[signalIndex].right;

            sampleTime += sampleInterval;
            while (signalIndex > 0 && signals[signalIndex].time > sampleTime) {
                signalIndex--;
            }
        }

        if (res.maxStrengthL > 0.0f) {
            for (float& s : res.waveformStrengthL) {
                s /= res.maxStrengthL;
            }
        }
        if (res.maxStrengthR > 0.0f) {
            for (float& s : res.waveformStrengthR) {
                s /= res.maxStrengthR;
            }
        }
        res.maxStrengthL *= strengthAmp;
        res.maxStrengthR *= strengthAmp;

        return res;
    }

    [[nodiscard]]
    inline SignalBufferSnapshot GetSnapshot() const {
        return m_Snapshot.load(std::memory_order_acquire);
    }

	[[nodiscard]]
	inline float GetStrengthAmp() const {
		return m_StrengthAmp.load();
	}

	inline void SetStrengthAmp(float amp) {
		m_StrengthAmp.store(amp);
	}

private:
    mutable std::mutex m_Mutex;

    std::vector<RumbleSignal> m_Buffer;
    std::atomic<SignalBufferSnapshot> m_Snapshot;

	std::atomic<float> m_StrengthAmp{ 1.0f };

    //TimeStamp m_LastSwapTime;
};
