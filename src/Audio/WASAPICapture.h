#pragma once

#include <atomic>
#include <functional>
#include <thread>

enum class AudioSampleFormat { Float, PCM, Unknown };

class WASAPICapture
{
public:
	using AudioPkgCallback = std::function<void(float left, float right)>;

	WASAPICapture() = delete;
	~WASAPICapture() = delete;

	static bool Start(AudioPkgCallback cb);
	static bool Restart();
	static void Stop();
	[[nodiscard]]
	static int IsRunning();

private:
	static inline std::unique_ptr<std::thread> s_Thread;
	static inline std::atomic<bool> s_Running{ false };
	static inline AudioPkgCallback s_AudioPkgCallback;
};
