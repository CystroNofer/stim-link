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

#include "core.h"

using namespace winrt::Windows::Devices;
using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::Storage::Streams;

class CoyoteBLEBackend
{
public:
	CoyoteBLEBackend();

	void StartScan();
	void StopScan();

	[[nodiscard]]
	std::unordered_map<std::uint64_t, BLEAdvertisementInfo> GetAdvertisements();
	void UpdateAdvertisements();

	IAsyncOperation<bool> ConnectAsync(std::uint64_t address);

	void Disconnect();

	[[nodiscard]]
	bool IsConnected();

	IAsyncOperation<bool> WriteCommandAsync(std::uint8_t strength);

	inline void SetSafety(bool safety) { m_Safety.store(safety); }
	[[nodiscard]]
	inline bool IsSafetyOn() const { return m_Safety.load(); }

private:
	std::atomic<bool> m_Safety{ false };

	Bluetooth::Advertisement::BluetoothLEAdvertisementWatcher m_AdvertisementWatcher{ nullptr };
	winrt::event_token
		m_AdReceivedToken{}, m_AdStoppedToken{},
		//m_BatteryChangedToken{},
		//m_CommandWriteToken{},
		m_CommandNotifyToken{};

	Bluetooth::BluetoothLEDevice m_Device{ nullptr };

	Bluetooth::GenericAttributeProfile::GattDeviceService
		m_CommandService{ nullptr };
		//m_BatteryService{ nullptr };

	Bluetooth::GenericAttributeProfile::GattCharacteristic
		m_CommandWriteCharacteristic{ nullptr },
		m_CommandNotifyCharacteristic{ nullptr };
		//m_BatteryReadCharacteristic{ nullptr };

	std::unordered_map<std::uint64_t, BLEAdvertisementInfo> m_Advertisements{};

	std::mutex m_Mutex;
};

