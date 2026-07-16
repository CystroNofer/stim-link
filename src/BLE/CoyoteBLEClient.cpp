#include "CoyoteBLEClient.h"

#include <cstdint>
#include <functional>
#include <iostream>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>

constexpr winrt::guid CoyoteServiceUuid{
	0x0000180c,
	0x0000,
	0x1000,
	{ 0x80, 0x00, 0x00, 0x80, 0x5f, 0x9b, 0x34, 0xfb }
};

constexpr winrt::guid CoyoteWriteUuid{
	0x0000150a,
	0x0000,
	0x1000,
	{ 0x80, 0x00, 0x00, 0x80, 0x5f, 0x9b, 0x34, 0xfb }
};

constexpr winrt::guid CoyoteNotifyUuid{
	0x0000150b,
	0x0000,
	0x1000,
	{ 0x80, 0x00, 0x00, 0x80, 0x5f, 0x9b, 0x34, 0xfb }
};

constexpr winrt::guid BatteryServiceUuid{
	0x0000180a,
	0x0000,
	0x1000,
	{ 0x80, 0x00, 0x00, 0x80, 0x5f, 0x9b, 0x34, 0xfb }
};

constexpr winrt::guid BatteryCharacteristicUuid{
	0x00001500,
	0x0000,
	0x1000,
	{ 0x80, 0x00, 0x00, 0x80, 0x5f, 0x9b, 0x34, 0xfb }
};

CoyoteBLEClient::CoyoteBLEClient() {
	winrt::init_apartment();
	//winrt::init_apartment(winrt::apartment_type::multi_threaded);

	StartScan();
	std::cout << "[BLE][INFO] Scanning...\n";
}

void CoyoteBLEClient::StartScan()
{
	using namespace winrt::Windows::Devices::Bluetooth::Advertisement;

	m_AdvertisementWatcher = BluetoothLEAdvertisementWatcher{};

	// Receive advertisements as they arrive.
	m_AdvertisementWatcher.ScanningMode(
		BluetoothLEScanningMode::Active
	);

	m_ReceivedToken = m_AdvertisementWatcher.Received(
		[this](
			const BluetoothLEAdvertisementWatcher&,
			const BluetoothLEAdvertisementReceivedEventArgs& args)
		{
			const BluetoothLEAdvertisement advertisement = args.Advertisement();
			const winrt::hstring localName = advertisement.LocalName();

			if (localName.empty())
				return;

			const std::string name = winrt::to_string(localName);

			// Coyote V3 pulse host.
			if (name != "47L121000")
				return;

			std::scoped_lock lock(m_Mutex);
			BLEAdvertisementInfo& ad = m_Advertisements[args.BluetoothAddress()];
			ad.name = name;
			ad.rssi = args.RawSignalStrengthInDBm();
			ad.lastSeen = std::chrono::steady_clock::now();
		}
	);

	m_StoppedToken = m_AdvertisementWatcher.Stopped(
		[](
			const BluetoothLEAdvertisementWatcher&,
			const BluetoothLEAdvertisementWatcherStoppedEventArgs& args)
		{
			std::cerr
				<< "[BLE][Error] BLE scan stopped. Error code: "
				<< static_cast<int>(args.Error())
				<< '\n';
		}
	);
	m_AdvertisementWatcher.Start();
}

void CoyoteBLEClient::StopScan()
{
	if (!m_AdvertisementWatcher)
		return;

	m_AdvertisementWatcher.Stop();

	m_AdvertisementWatcher.Received(m_ReceivedToken);
	m_AdvertisementWatcher.Stopped(m_StoppedToken);

	m_AdvertisementWatcher = nullptr;
}

std::unordered_map<std::uint64_t, BLEAdvertisementInfo> CoyoteBLEClient::GetAdvertisements() const
{
	return m_Advertisements;
}

void CoyoteBLEClient::UpdateAdvertisements()
{
	if (IsConnected())
		return;

	/* The scanning is not frequent, so locking at every frame is fine */
	std::scoped_lock lock(m_Mutex);

	std::erase_if(
		m_Advertisements,
		[](const std::pair<const std::uint64_t, BLEAdvertisementInfo>& p) {
			return
				std::chrono::steady_clock::now() - p.second.lastSeen >
				std::chrono::seconds(5);
		}
	);
}

IAsyncOperation<bool> CoyoteBLEClient::ConnectAsync(std::uint64_t address)
{
	{
		std::scoped_lock lock(m_Mutex);
		if (!m_Advertisements.contains(address))
			co_return false;

		m_Advertisements[address].connectionState = BLEConnectionState::Connecting;
	}

	using namespace winrt::Windows::Devices::Bluetooth;
	using namespace winrt::Windows::Devices::Bluetooth::GenericAttributeProfile;

	Disconnect();

	try
	{
		BluetoothLEDevice device =
			co_await BluetoothLEDevice::FromBluetoothAddressAsync(address);

		if (!device) {
			std::scoped_lock lock(m_Mutex);
			m_Advertisements[address].connectionState = BLEConnectionState::Failed;
			co_return false;
		}

		// ==================== Services & Characteristics ====================
		// ========== Command Services ==========
		const GattDeviceServicesResult mainServiceResult =
			co_await device.GetGattServicesForUuidAsync(
				CoyoteServiceUuid,
				BluetoothCacheMode::Uncached
			);

		if (mainServiceResult.Status() != GattCommunicationStatus::Success)
		{
			std::scoped_lock lock(m_Mutex);
			m_Advertisements[address].connectionState = BLEConnectionState::Failed;
			co_return false;
		}

		const Collections::IVectorView<GattDeviceService> commandServices =
			mainServiceResult.Services();

		if (commandServices.Size() == 0)
		{
			std::scoped_lock lock(m_Mutex);
			m_Advertisements[address].connectionState = BLEConnectionState::Failed;
			co_return false;
		}

		GattDeviceService commandService = commandServices.GetAt(0);

		// ========== Write Characteristic ==========
		const GattCharacteristicsResult commandWriteResult =
			co_await commandService.GetCharacteristicsForUuidAsync(
				CoyoteWriteUuid,
				BluetoothCacheMode::Uncached
			);

		if (
			commandWriteResult.Status() != GattCommunicationStatus::Success ||
			commandWriteResult.Characteristics().Size() == 0
			) {
			std::scoped_lock lock(m_Mutex);
			m_Advertisements[address].connectionState = BLEConnectionState::Failed;
			co_return false;
		}

		// ========== Notify Characteristic ==========
		const GattCharacteristicsResult commandNotifyResult =
			co_await commandService.GetCharacteristicsForUuidAsync(
				CoyoteNotifyUuid,
				BluetoothCacheMode::Uncached
			);

		if (
			commandNotifyResult.Status() != GattCommunicationStatus::Success ||
			commandNotifyResult.Characteristics().Size() == 0
			) {
			std::scoped_lock lock(m_Mutex);
			m_Advertisements[address].connectionState = BLEConnectionState::Failed;
			co_return false;
		}

		// ========== Battery Services ==========
		const GattDeviceServicesResult batteryServiceResult =
			co_await device.GetGattServicesForUuidAsync(
				BatteryServiceUuid,
				BluetoothCacheMode::Uncached
			);

		if (batteryServiceResult.Status() != GattCommunicationStatus::Success)
		{
			std::scoped_lock lock(m_Mutex);
			m_Advertisements[address].connectionState = BLEConnectionState::Failed;
			co_return false;
		}

		const Collections::IVectorView<GattDeviceService> batteryServices =
			batteryServiceResult.Services();

		if (batteryServices.Size() == 0)
		{
			std::scoped_lock lock(m_Mutex);
			m_Advertisements[address].connectionState = BLEConnectionState::Failed;
			co_return false;
		}

		GattDeviceService batteryService = batteryServices.GetAt(0);

		// ========== Read/Notify Characteristic ==========
		const GattCharacteristicsResult batteryReadResult =
			co_await batteryService.GetCharacteristicsForUuidAsync(
				BatteryCharacteristicUuid,
				BluetoothCacheMode::Uncached
			);

		if (
			batteryReadResult.Status() != GattCommunicationStatus::Success ||
			batteryReadResult.Characteristics().Size() == 0)
		{
			std::scoped_lock lock(m_Mutex);
			m_Advertisements[address].connectionState = BLEConnectionState::Failed;
			co_return false;
		}

		std::scoped_lock lock(m_Mutex);
		// Store these only after every discovery step succeeds.
		m_Device = device;

		m_CommandService = commandService;
		m_CommandWriteCharacteristic =
			commandWriteResult.Characteristics().GetAt(0);
		m_CommandNotifyCharacteristic =
			commandNotifyResult.Characteristics().GetAt(0);

		m_BatteryService = batteryService;
		m_BatteryReadCharacteristic =
			batteryReadResult.Characteristics().GetAt(0);

		m_Advertisements[address].connectionState = BLEConnectionState::Connected;

		StopScan();

		co_return true;
	}
	catch (const winrt::hresult_error&)
	{
		Disconnect();
		m_Advertisements[address].connectionState = BLEConnectionState::Failed;
		co_return false;
	}
}

void CoyoteBLEClient::Disconnect()
{
	m_CommandNotifyCharacteristic = nullptr;
	m_CommandWriteCharacteristic = nullptr;
	m_BatteryReadCharacteristic = nullptr;

	if (m_CommandService)
	{
		m_CommandService.Close();
		m_CommandService = nullptr;
	}

	if (m_BatteryService)
	{
		m_BatteryService.Close();
		m_BatteryService = nullptr;
	}

	if (m_Device)
	{
		m_Device.Close();
		m_Device = nullptr;
	}
}

bool CoyoteBLEClient::IsConnected() const
{
	return m_Device &&
		m_Device.ConnectionStatus() == Bluetooth::BluetoothConnectionStatus::Connected;
}