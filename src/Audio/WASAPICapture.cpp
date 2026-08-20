#include "WASAPICapture.h"

#include <algorithm>
#include <audioclient.h>
#include <avrt.h>
//#include <endpointvolume.h>
#include <iostream>
#include <mmdeviceapi.h>
#include <mmreg.h>
#include <windows.h>

#include "core.h"

// Linkage note: project must link against Ole32, Avrt, and Winmm implicitly via build system.

bool WASAPICapture::Start(AudioPkgCallback cb)
{
	if (s_Running.load()) return false;

	s_AudioPkgCallback = std::move(cb);

	s_Thread = std::make_unique<std::thread>([]() {
		HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
		if (FAILED(hr)) {
			std::cerr << "[WASAPI][ERROR] CoInitializeEx failed\n";
			return;
		}

		IMMDeviceEnumerator* pEnumerator = nullptr;
		hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
			__uuidof(IMMDeviceEnumerator), reinterpret_cast<void**>(&pEnumerator));
		if (FAILED(hr) || !pEnumerator) {
			std::cerr << "[WASAPI][ERROR] Failed to create MMDeviceEnumerator\n";
			CoUninitialize();
			return;
		}

		IMMDevice* pDevice = nullptr;
		hr = pEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, &pDevice);
		if (FAILED(hr) || !pDevice) {
			std::cerr
				<< "[WASAPI][ERROR] Failed to get default render device\n";
			pEnumerator->Release();
			CoUninitialize();
			return;
		}

		// Print device id for logging purposes
		LPWSTR pDeviceId = nullptr;
		hr = pDevice->GetId(&pDeviceId);
		if (SUCCEEDED(hr) && pDeviceId) {
			std::wcout << L"[WASAPI][INFO] Capture device id: " << pDeviceId << L"\n";
			CoTaskMemFree(pDeviceId);
		}
		else {
			std::cerr
				<< "[WASAPI][WARN] Unable to get device id: 0x"
				<< std::hex << static_cast<unsigned long>(hr) << std::dec << "\n";
		}

		//IAudioEndpointVolume* endpointVolume = nullptr;
		//pDevice->Activate(
		//	__uuidof(IAudioEndpointVolume),
		//	CLSCTX_ALL,
		//	nullptr,
		//	reinterpret_cast<void**>(&endpointVolume)
		//);

		IAudioClient* pAudioClient = nullptr;
		hr = pDevice->Activate(
			__uuidof(IAudioClient), \
			CLSCTX_ALL,
			nullptr,
			reinterpret_cast<void**>(&pAudioClient)
		);
		if (FAILED(hr) || !pAudioClient) {
			std::cerr << "[WASAPI][ERROR] Failed to activate IAudioClient\n";
			pDevice->Release();
			pEnumerator->Release();
			CoUninitialize();
			return;
		}

		WAVEFORMATEX* pwfx = nullptr;
		hr = pAudioClient->GetMixFormat(&pwfx);
		if (FAILED(hr) || !pwfx) {
			std::cerr << "[WASAPI][ERROR] GetMixFormat failed\n";
			pAudioClient->Release();
			pDevice->Release();
			pEnumerator->Release();
			CoUninitialize();
			return;
		}

		// Initialize in loopback shared mode
		hr = pAudioClient->Initialize(
			AUDCLNT_SHAREMODE_SHARED,
			AUDCLNT_STREAMFLAGS_LOOPBACK,
			0,
			0,
			pwfx,
			nullptr
		);

		if (FAILED(hr)) {
			std::cerr << "[WASAPI][ERROR] IAudioClient Initialize failed\n";
			CoTaskMemFree(pwfx);
			pAudioClient->Release();
			pDevice->Release();
			pEnumerator->Release();
			CoUninitialize();
			return;
		}

		IAudioCaptureClient* pCaptureClient = nullptr;
		hr = pAudioClient->GetService(
			__uuidof(IAudioCaptureClient),
			reinterpret_cast<void**>(&pCaptureClient)
		);
		if (FAILED(hr) || !pCaptureClient) {
			std::cerr << "[WASAPI][ERROR] GetService(IAudioCaptureClient) failed\n";
			pAudioClient->Release();
			pDevice->Release();
			pEnumerator->Release();
			CoUninitialize();
			return;
		}

		hr = pAudioClient->Start();
		if (FAILED(hr)) {
			std::cerr
				<< "[WASAPI][ERROR] Failed to start audio client: 0x"
				<< std::hex << static_cast<unsigned long>(hr) << std::dec << "\n";
			pCaptureClient->Release();
			pAudioClient->Release();
			pDevice->Release();
			pEnumerator->Release();
			CoUninitialize();
			return;
		}

		// Determine channels and effective bits/bytes per sample.
		const int channels = pwfx->nChannels;

		// For WAVE_FORMAT_EXTENSIBLE, prefer Samples.wValidBitsPerSample when present.
		int bitsPerSampleEffective = pwfx->wBitsPerSample;
		if (pwfx->wFormatTag == WAVE_FORMAT_EXTENSIBLE) {
			WAVEFORMATEXTENSIBLE* wfex = reinterpret_cast<WAVEFORMATEXTENSIBLE*>(pwfx);
			if (wfex->Samples.wValidBitsPerSample != 0) {
				bitsPerSampleEffective = wfex->Samples.wValidBitsPerSample;
			}
		}
		int bytesPerSample = (bitsPerSampleEffective + 7) / 8;

		AudioSampleFormat sampleFormat = AudioSampleFormat::Unknown;
		switch (pwfx->wFormatTag) {
		case WAVE_FORMAT_IEEE_FLOAT:
			sampleFormat = AudioSampleFormat::Float;
			break;
		case WAVE_FORMAT_PCM:
			sampleFormat = AudioSampleFormat::PCM;
			break;
		case WAVE_FORMAT_EXTENSIBLE:
		{
			WAVEFORMATEXTENSIBLE* wfex = reinterpret_cast<WAVEFORMATEXTENSIBLE*>(pwfx);
			if (IsEqualGUID(wfex->SubFormat, KSDATAFORMAT_SUBTYPE_IEEE_FLOAT)) {
				sampleFormat = AudioSampleFormat::Float;
			}
			else if (IsEqualGUID(wfex->SubFormat, KSDATAFORMAT_SUBTYPE_PCM)) {
				sampleFormat = AudioSampleFormat::PCM;
			}
		}
		}

		//// Log detailed mix format for diagnosis.
		//std::cout << "[WASAPI][INFO] Mix format:\n";
		//std::cout << "  wFormatTag: " << pwfx->wFormatTag
		//	<< ", nChannels: " << pwfx->nChannels
		//	<< ", nSamplesPerSec: " << pwfx->nSamplesPerSec
		//	<< ", wBitsPerSample: " << pwfx->wBitsPerSample
		//	<< ", cbSize: " << pwfx->cbSize << "\n";

		//if (pwfx->wFormatTag == WAVE_FORMAT_EXTENSIBLE) {
		//	WAVEFORMATEXTENSIBLE* wfex = reinterpret_cast<WAVEFORMATEXTENSIBLE*>(pwfx);
		//	GUID g = wfex->SubFormat;
		//	// Print GUID components in hex for easier identification
		//	std::cout << "  SubFormat GUID: {0x" << std::hex << g.Data1
		//		<< ", 0x" << g.Data2
		//		<< ", 0x" << g.Data3 << ", 0x";
		//	for (int i = 0; i < 8; ++i) {
		//		std::cout << static_cast<int>(g.Data4[i]);
		//		if (i + 1 < 8) std::cout << ",";
		//	}
		//	std::cout << std::dec << "}\n";
		//	std::cout << "  wValidBitsPerSample: " << wfex->Samples.wValidBitsPerSample << "\n";
		//}

		std::cout << "[WASAPI][INFO] Audio capture started\n";
		s_Running.store(true);
		// Capture loop
		while (s_Running.load()) {
			UINT32 packetLength = 0;
			pCaptureClient->GetNextPacketSize(&packetLength);

			while (packetLength != 0) {
				BYTE* pData;
				UINT32 numFramesAvailable;
				DWORD flags;
				hr = pCaptureClient->GetBuffer(
					&pData,
					&numFramesAvailable,
					&flags,
					nullptr,
					nullptr
				);
				if (FAILED(hr)) {
					break;
				}

				if (flags & AUDCLNT_BUFFERFLAGS_SILENT) {
					// Silent buffer
					std::cout << "[WASAPI][WARN] Silent buffer received\n";
					if (s_AudioPkgCallback) s_AudioPkgCallback(0.0f, 0.0f);
				}
				else {
					float volumeL = 0.0;
					float volumeR = 0.0;
					for (UINT32 i = 0; i < numFramesAvailable; i++) {
						if (sampleFormat == AudioSampleFormat::Float) {
							if (bytesPerSample == 4) {
								float* f = reinterpret_cast<float*>(pData);
								if (channels > 1) {
									double l = double(f[i * channels]);
									double r = double(f[i * channels + 1]);
									volumeL = max(volumeL, std::abs(l));
									volumeR = max(volumeR, std::abs(r));
								}
								else {
									double v = double(f[i]);
									volumeL = max(volumeL, std::abs(v));
									volumeR = max(volumeR, std::abs(v));
								}
							}
							else if (bytesPerSample == 8) {
								double* d = reinterpret_cast<double*>(pData);
								if (channels > 1) {
									double l = d[i * channels];
									double r = d[i * channels + 1];
									volumeL = max(volumeL, std::abs(l));
									volumeR = max(volumeR, std::abs(r));
								}
								else {
									double v = d[i];
									volumeL = max(volumeL, std::abs(v));
									volumeR = max(volumeR, std::abs(v));
								}
							}
							else {
								// unexpected float width
								continue;
							}
						}
						else if (sampleFormat == AudioSampleFormat::PCM) {
							if (bytesPerSample == 1) {
								// 8-bit PCM unsigned
								uint8_t* u = reinterpret_cast<uint8_t*>(pData);
								if (channels > 1) {
									double l = (static_cast<int>(u[i * channels]) - 128) / 128.0;
									double r = (static_cast<int>(u[i * channels + 1]) - 128) / 128.0;
									volumeL = max(volumeL, std::abs(l));
									volumeR = max(volumeR, std::abs(r));
								}
								else {
									double v = (static_cast<int>(u[i]) - 128) / 128.0;
									volumeL = max(volumeL, std::abs(v));
									volumeR = max(volumeR, std::abs(v));
								}
							}
							else if (bytesPerSample == 2) {
								int16_t* s16 = reinterpret_cast<int16_t*>(pData);
								if (channels > 1) {
									double lf = static_cast<double>(s16[i * channels]) / 32768.0;
									double rf = static_cast<double>(s16[i * channels + 1]) / 32768.0;
									volumeL = max(volumeL, std::abs(lf));
									volumeR = max(volumeR, std::abs(rf));
								}
								else {
									int16_t v = s16[i];
									double vf = static_cast<double>(v) / 32768.0;
									volumeL = max(volumeL, std::abs(vf));
									volumeR = max(volumeR, std::abs(vf));
								}
							}
							else {
								std::cout
									<< "[WASAPI][WARN] Unhandled PCM width: "
									<< bytesPerSample << "\n";
								continue;
							}
						}
						else {
							std::cout
								<< "[WASAPI][WARN] Unsupported audio format/subformat: "
								<< pwfx->wFormatTag << "\n";
							continue;
						}
					}

					//if (numFramesAvailable == 0) {
					//	std::cout << "[WASAPI][WARN] Zero frames available\n";
					//	volumeL = 0.0f;
					//	volumeR = 0.0f;
					//}
					//else {
					//	volumeL = static_cast<float>(volumeL / numFramesAvailable);
					//	volumeR = static_cast<float>(volumeR / numFramesAvailable);
					//}

					// clamp to [0,1]
					// Technically s_LastL/R always > 0 since rmsL/R is sqrt
					volumeL = std::clamp(volumeL, 0.0f, 1.0f);
					volumeR = std::clamp(volumeR, 0.0f, 1.0f);

					if (s_AudioPkgCallback) s_AudioPkgCallback(volumeL, volumeR);
				}

				hr = pCaptureClient->ReleaseBuffer(numFramesAvailable);
				if (FAILED(hr)) break;
				hr = pCaptureClient->GetNextPacketSize(&packetLength);
				if (FAILED(hr)) break;
			}

			// Sleep briefly to avoid busy loop
			std::this_thread::sleep_for(AUDIO_BLOCK_DURATION);
		}

		pAudioClient->Stop();
		pCaptureClient->Release();
		pAudioClient->Release();
		pDevice->Release();
		pEnumerator->Release();
		CoTaskMemFree(pwfx);
		CoUninitialize();
		s_Running.store(false);
		}
	);

	return true;
}

bool WASAPICapture::Restart()
{
	Stop();
	return Start(s_AudioPkgCallback);
}

void WASAPICapture::Stop()
{
	s_Running.store(false);
	if (s_Thread && s_Thread->joinable()) {
		s_Thread->join();
	}
	s_Thread.reset();
}

int WASAPICapture::IsRunning()
{
	if (s_Thread && s_Thread->joinable()) {
		if (s_Running.load()) {
			return 1; // Running
		}
		else {
			return 0; // Not running
		}
	}
	return -1; // Stopped
}
