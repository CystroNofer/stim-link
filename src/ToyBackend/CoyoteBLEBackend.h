#pragma once

#include <winrt/Windows.Devices.Bluetooth.h>
#include <winrt/Windows.Devices.Bluetooth.Advertisement.h>
#include <winrt/Windows.Devices.Bluetooth.GenericAttributeProfile.h>
#include <winrt/Windows.Devices.Enumeration.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Storage.Streams.h>

#include <unordered_map>
#include <mutex>
#include <atomic>

#include "core.h"

using namespace winrt::Windows::Devices;
using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::Storage::Streams;

struct BLEConnection {
public:
	Bluetooth::BluetoothLEDevice device{ nullptr };

	Bluetooth::GenericAttributeProfile::GattDeviceService
		commandService{ nullptr };
	//batteryService{ nullptr };

	Bluetooth::GenericAttributeProfile::GattCharacteristic
		writeCharacteristic{ nullptr },
		notifyCharacteristic{ nullptr };
	//batteryReadCharacteristic{ nullptr };

	winrt::event_token
		//batteryChangedToken{},
		//writeToken{},  // Write has no callback
		notifyToken{};
};

class CoyoteBLEBackend
{
public:
	CoyoteBLEBackend();

	void StartScan();
	void StopScan();

	[[nodiscard]]
	std::unordered_map<std::uint64_t, BLEAdvertisementInfo> GetAdvertisements();

	IAsyncOperation<bool> ConnectAsync(std::uint64_t address);

	void DisconnectPulseUnit();

	[[nodiscard]]
	bool IsPulseUnitConnected();
	[[nodiscard]]
	int IsPawPrintConnected();

	IAsyncOperation<bool> WriteCommandAsync(WaveformSample<4> waveformSamples);

	inline void SetSafety(bool safety) { m_SafetyOn.store(safety); }
	[[nodiscard]]
	inline bool IsSafetyOn() const { return m_SafetyOn.load(); }

private:
	std::atomic<bool> m_SafetyOn{ false };

	Bluetooth::Advertisement::BluetoothLEAdvertisementWatcher m_AdvertisementWatcher{ nullptr };
	winrt::event_token
		m_AdReceivedToken{}, m_AdStoppedToken{};

	BLEConnection m_PulseUnit{};
	BLEConnection m_PawPrint{};

	std::unordered_map<std::uint64_t, BLEAdvertisementInfo> m_Advertisements{};

	std::mutex m_Mutex;
};

