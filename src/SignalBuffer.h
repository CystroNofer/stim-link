#pragma once

#include <cstdint>
#include <mutex>

#include "core.h"

class SignalBuffer
{
public:
    SignalBuffer();

    void Push(float left, float right);

    template <std::size_t nSamples>
    WaveformSample<nSamples> Sample(TimeDuration d)
    {
        // The shared pointer guarantees the vector remains alive
        const SignalBufferSnapshot snapshot = m_Snapshot.load();
        const std::vector<Signal>& signals = *snapshot;

        if (nSamples == 0 || signals.empty()) {
            return WaveformSample<nSamples>{};
        }

		const float strengthAmp = m_StrengthAmp.load();

        if (nSamples == 1) {
            Signal lastSignal = signals.back();
            return WaveformSample<nSamples>{
                .maxStrengthL = lastSignal.left,
                .maxStrengthR = lastSignal.right,
                .waveformStrengthL = { 1.0f },
                .waveformStrengthR = { 1.0f }
            };
        }

        TimeDuration sampleInterval = d / nSamples;
        TimeStamp sampleTimeStart = std::chrono::steady_clock::now() - sampleInterval;
        size_t signalIndex = signals.size() - 1;
        size_t resIndex = nSamples - 1;
        WaveformSample<nSamples> res;
		while (signalIndex > 0) {
            if (signals[signalIndex].time < sampleTimeStart) {
                if (resIndex < 1) {
                    break;
                }
                resIndex--;
                sampleTimeStart -= sampleInterval;
            }

            res.maxStrengthL = max(res.maxStrengthL, signals[signalIndex].left);
            res.maxStrengthR = max(res.maxStrengthR, signals[signalIndex].right);

            res.waveformStrengthL[resIndex] =
                max(res.waveformStrengthL[resIndex], signals[signalIndex].left);
            res.waveformStrengthR[resIndex] =
                max(res.waveformStrengthR[resIndex], signals[signalIndex].right);

			signalIndex--;
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

    std::vector<Signal> m_Buffer;
    std::atomic<SignalBufferSnapshot> m_Snapshot;

	std::atomic<float> m_StrengthAmp{ 1.0f };
};
