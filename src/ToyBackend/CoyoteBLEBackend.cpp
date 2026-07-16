#include "CoyoteBLEBackend.h"

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

enum class StrengthMode : std::uint8_t
{
	Unchanged = 0b00,
	Increase = 0b01,
	Decrease = 0b10,
	Absolute = 0b11
};

std::array<std::uint8_t, 20> BuildB0Packet(
	std::uint8_t sequence,
	StrengthMode modeA,
	StrengthMode modeB,
	std::uint8_t strengthA,
	std::uint8_t strengthB,
	const std::array<std::uint8_t, 4>& frequencyA = { 10, 10, 10, 10 },
	const std::array<std::uint8_t, 4>& waveformA = { 100, 100, 100, 100 },
	const std::array<std::uint8_t, 4>& frequencyB = { 10, 10, 10, 10 },
	const std::array<std::uint8_t, 4>& waveformB = { 100, 100, 100, 100 })
{
	std::array<std::uint8_t, 20> packet{};

	sequence &= 0x0F;

	const std::uint8_t modes =
		static_cast<std::uint8_t>(
			(static_cast<std::uint8_t>(modeA) << 2) |
			static_cast<std::uint8_t>(modeB)
		);

	packet[0] = 0xB0;
	packet[1] = static_cast<std::uint8_t>((sequence << 4) | modes);

	packet[2] = strengthA;
	packet[3] = strengthB;

	std::copy(
		frequencyA.begin(),
		frequencyA.end(),
		packet.begin() + 4
	);

	std::copy(
		waveformA.begin(),
		waveformA.end(),
		packet.begin() + 8
	);

	std::copy(
		frequencyB.begin(),
		frequencyB.end(),
		packet.begin() + 12
	);

	std::copy(
		waveformB.begin(),
		waveformB.end(),
		packet.begin() + 16
	);

	return packet;
}

CoyoteBLEBackend::CoyoteBLEBackend() {
	//winrt::init_apartment();
	winrt::init_apartment(winrt::apartment_type::multi_threaded);

	StartScan();
	std::cout << "[BLE][INFO] Scanning...\n";
}

void CoyoteBLEBackend::StartScan()
{
	std::scoped_lock lock(m_Mutex);

	using namespace winrt::Windows::Devices::Bluetooth::Advertisement;

	m_AdvertisementWatcher = BluetoothLEAdvertisementWatcher{};

	// Receive advertisements as they arrive.
	m_AdvertisementWatcher.ScanningMode(
		BluetoothLEScanningMode::Active
	);

	m_AdReceivedToken = m_AdvertisementWatcher.Received(
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

	m_AdStoppedToken = m_AdvertisementWatcher.Stopped(
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

void CoyoteBLEBackend::StopScan()
{
	std::scoped_lock lock(m_Mutex);

	if (!m_AdvertisementWatcher)
		return;

	m_AdvertisementWatcher.Stop();

	m_AdvertisementWatcher.Received(m_AdReceivedToken);
	m_AdvertisementWatcher.Stopped(m_AdStoppedToken);

	m_AdvertisementWatcher = nullptr;
}

std::unordered_map<std::uint64_t, BLEAdvertisementInfo> CoyoteBLEBackend::GetAdvertisements()
{
	std::scoped_lock lock(m_Mutex);
	return m_Advertisements;
}

void CoyoteBLEBackend::UpdateAdvertisements()
{
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

IAsyncOperation<bool> CoyoteBLEBackend::ConnectAsync(std::uint64_t address)
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

		//// ==================== Pairing ====================
		//using namespace winrt::Windows::Devices::Enumeration;
		//const DeviceInformationPairing pairing =
		//	device.DeviceInformation().Pairing();

		//if (!pairing.IsPaired())
		//{
		//	const DevicePairingResult result =
		//		co_await pairing.PairAsync();

		//	switch (result.Status())
		//	{
		//	case DevicePairingResultStatus::Paired:
		//	case DevicePairingResultStatus::AlreadyPaired:
		//		break;

		//	default:
		//		std::cerr
		//			<< "Pairing failed: "
		//			<< static_cast<int>(result.Status())
		//			<< '\n';

		//		co_return false;
		//	}
		//}
		
		// ==================== Services & Characteristics ====================
		// ========== Command Services ==========
		const GattDeviceServicesResult commandServiceResult =
			co_await device.GetGattServicesForUuidAsync(
				CoyoteServiceUuid,
				BluetoothCacheMode::Uncached
			);

		if (commandServiceResult.Status() != GattCommunicationStatus::Success)
		{
			std::scoped_lock lock(m_Mutex);
			m_Advertisements[address].connectionState = BLEConnectionState::Failed;
			co_return false;
		}

		const Collections::IVectorView<GattDeviceService> commandServices =
			commandServiceResult.Services();

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

		GattCharacteristic commandNotifyCharacteristic =
			commandNotifyResult.Characteristics().GetAt(0);
		winrt::event_token cmdNotifyChangedToken =
			commandNotifyCharacteristic.ValueChanged(
				[this](
					const auto&,
					const GattValueChangedEventArgs& args)
				{
					using namespace winrt::Windows::Storage::Streams;

					DataReader reader =
						DataReader::FromBuffer(args.CharacteristicValue());

					const std::uint32_t byteCount =
						reader.UnconsumedBufferLength();

					if (byteCount < 1)
						return;

					std::vector<std::uint8_t> bytes(byteCount);

					reader.ReadBytes(bytes);

					switch (bytes[0])
					{
					case 0xB1:
						// This currently does NOT work as intended
						// The 0-sequenced notification is heavily delayed
						
						//if (bytes.size() == 4 && bytes[1] == 0)
						//{
						//	SetSafety(true);
						//}
						break;

					default:
						std::cerr
							<< "[BLE][WARNING] Unknown notification: 0x"
							<< std::hex
							<< static_cast<int>(bytes[0])
							<< std::dec
							<< '\n';
						break;
					}
				}
			);

		const GattCommunicationStatus status =
			co_await commandNotifyCharacteristic
			.WriteClientCharacteristicConfigurationDescriptorAsync(
				GattClientCharacteristicConfigurationDescriptorValue::Notify
			);

		if (status != GattCommunicationStatus::Success)
		{
			commandNotifyCharacteristic.ValueChanged(
				cmdNotifyChangedToken
			);

			m_Advertisements[address].connectionState = BLEConnectionState::Failed;
			co_return false;
		}

		//// ========== Battery Services ==========
		//const GattDeviceServicesResult batteryServiceResult =
		//	co_await device.GetGattServicesForUuidAsync(
		//		BatteryServiceUuid,
		//		BluetoothCacheMode::Uncached
		//	);

		//if (batteryServiceResult.Status() != GattCommunicationStatus::Success)
		//{
		//	std::scoped_lock lock(m_Mutex);
		//	m_Advertisements[address].connectionState = BLEConnectionState::Failed;
		//	co_return false;
		//}

		//const Collections::IVectorView<GattDeviceService> batteryServices =
		//	batteryServiceResult.Services();

		//if (batteryServices.Size() == 0)
		//{
		//	std::scoped_lock lock(m_Mutex);
		//	m_Advertisements[address].connectionState = BLEConnectionState::Failed;
		//	co_return false;
		//}

		//GattDeviceService batteryService = batteryServices.GetAt(0);

		//// ========== Read/Notify Characteristic ==========
		//const GattCharacteristicsResult batteryReadResult =
		//	co_await batteryService.GetCharacteristicsForUuidAsync(
		//		BatteryCharacteristicUuid,
		//		BluetoothCacheMode::Uncached
		//	);

		//if (
		//	batteryReadResult.Status() != GattCommunicationStatus::Success ||
		//	batteryReadResult.Characteristics().Size() == 0)
		//{
		//	std::scoped_lock lock(m_Mutex);
		//	m_Advertisements[address].connectionState = BLEConnectionState::Failed;
		//	co_return false;
		//}

		//GattCharacteristic batteryReadCharacteristic =
		//	batteryReadResult.Characteristics().GetAt(0);
		//winrt::event_token batteryChangedToken =
		//	batteryReadCharacteristic.ValueChanged(
		//		[this](
		//			const auto&,
		//			const GattValueChangedEventArgs& args)
		//		{
		//			using namespace winrt::Windows::Storage::Streams;

		//			DataReader reader =
		//				DataReader::FromBuffer(args.CharacteristicValue());

		//			if (reader.UnconsumedBufferLength() < 1)
		//				return;

		//			m_BatteryPercentage.store(
		//				static_cast<int>(reader.ReadByte())
		//			);
		//		}
		//	);

		//const GattCommunicationStatus status =
		//	co_await batteryReadCharacteristic
		//	.WriteClientCharacteristicConfigurationDescriptorAsync(
		//		GattClientCharacteristicConfigurationDescriptorValue::Notify
		//	);

		//if (status != GattCommunicationStatus::Success)
		//{
		//	batteryReadCharacteristic.ValueChanged(
		//		batteryChangedToken
		//	);

		//	// For now, allow connection to finish without battery callback
		//	//co_return false;
		//}

		std::scoped_lock lock(m_Mutex);
		// Store these only after every discovery step succeeds.
		m_Device = device;

		m_CommandService = commandService;
		m_CommandWriteCharacteristic =
			commandWriteResult.Characteristics().GetAt(0);
		m_CommandNotifyCharacteristic = commandNotifyCharacteristic;
		m_CommandNotifyToken = cmdNotifyChangedToken;

		//m_BatteryService = batteryService;
		//m_BatteryReadCharacteristic = batteryReadCharacteristic;
		//m_BatteryChangedToken = batteryChangedToken;

		m_Advertisements[address].connectionState = BLEConnectionState::Connected;

		StopScan();

		co_return true;
	}
	catch (const winrt::hresult_error& error)
	{
		std::cerr
			<< "[BLE][Error] CCCD threw: "
			<< winrt::to_string(error.message())
			<< '\n';

		Disconnect();
		m_Advertisements[address].connectionState = BLEConnectionState::Failed;
		co_return false;
	}
}

void CoyoteBLEBackend::Disconnect()
{
	std::scoped_lock lock(m_Mutex);

	// ==================== Tokens ====================
	if (m_CommandNotifyToken)
	{
		m_CommandNotifyCharacteristic.ValueChanged(
			m_CommandNotifyToken
		);
	}
	m_CommandNotifyToken = {};

	// ==================== Characteristics ====================
	m_CommandNotifyCharacteristic = nullptr;
	m_CommandWriteCharacteristic = nullptr;
	//m_BatteryReadCharacteristic = nullptr;

	// ==================== Services ====================
	if (m_CommandService)
	{
		m_CommandService.Close();
		m_CommandService = nullptr;
	}

	//if (m_BatteryService)
	//{
	//	m_BatteryService.Close();
	//	m_BatteryService = nullptr;
	//}

	// ==================== Device ====================
	if (m_Device)
	{
		m_Device.Close();
		m_Device = nullptr;
	}
}

bool CoyoteBLEBackend::IsConnected()
{
	std::scoped_lock lock(m_Mutex);

	return m_Device &&
		m_Device.ConnectionStatus() == Bluetooth::BluetoothConnectionStatus::Connected;
}

IAsyncOperation<bool> CoyoteBLEBackend::WriteCommandAsync(std::uint8_t strength)
{
	using namespace winrt::Windows::Devices::Bluetooth::GenericAttributeProfile;

	GattCharacteristic writeCharacteristic{ nullptr };
	{
		std::scoped_lock lock(m_Mutex);
		writeCharacteristic = m_CommandWriteCharacteristic;
	}

	if (!writeCharacteristic)
		co_return false;

	DataWriter writer;
	writer.WriteBytes(BuildB0Packet(
		10,
		StrengthMode::Absolute,
		StrengthMode::Absolute,
		strength,
		strength
	));

	const GattWriteResult result =
		co_await writeCharacteristic.WriteValueWithResultAsync(
			writer.DetachBuffer(),
			GattWriteOption::WriteWithoutResponse
		);

	co_return
		result.Status() == GattCommunicationStatus::Success;
}