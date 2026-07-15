#pragma once

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Devices.Bluetooth.h>
#include <winrt/Windows.Devices.Bluetooth.Advertisement.h>
#include <winrt/Windows.Devices.Bluetooth.GenericAttributeProfile.h>

#include "Waveform.h"

using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::Devices;

struct BLEDeviceInfo
{
    std::uint64_t Address{};
    std::string Name;
    std::int16_t rssi{};
};

class CoyoteBLEClient
{
public:
    CoyoteBLEClient();

    void StartScan();
    void StopScan();

    IAsyncOperation<bool> ConnectAsync(std::uint64_t address);

	void Disconnect();

    [[nodiscard]]
    bool IsConnected() const;

private:
    Bluetooth::Advertisement::BluetoothLEAdvertisementWatcher m_AdvertisementWatcher{ nullptr };
    winrt::event_token m_ReceivedToken{};
    winrt::event_token m_StoppedToken{};

    Bluetooth::BluetoothLEDevice m_Device{ nullptr };

	Bluetooth::GenericAttributeProfile::GattDeviceService
        m_CommandService{ nullptr },
		m_BatteryService{ nullptr };

    Bluetooth::GenericAttributeProfile::GattCharacteristic
        m_CommandWriteCharacteristic{ nullptr },
        m_CommandNotifyCharacteristic{ nullptr },
        m_BatteryReadCharacteristic{ nullptr };
};

