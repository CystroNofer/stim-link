#include "CoyoteBLEBackend.h"

#include <cstdint>
#include <functional>
#include <iostream>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>

// ==================== UUIDs ====================
// All the latest versions of DGLab devices
// Uses the same GATT layout
// PawPrint does not support battery services
inline const winrt::guid ServiceUUID =
Bluetooth::BluetoothUuidHelper::FromShortId(0x180c);

inline const winrt::guid WriteUUID =
Bluetooth::BluetoothUuidHelper::FromShortId(0x150a);

inline const winrt::guid NotifyUUID =
Bluetooth::BluetoothUuidHelper::FromShortId(0x150b);

inline const winrt::guid BatteryServiceUUID =
Bluetooth::BluetoothUuidHelper::FromShortId(0x180a);

inline const winrt::guid BatteryReadNotifyUUID =
Bluetooth::BluetoothUuidHelper::FromShortId(0x1500);

constexpr std::array<std::uint8_t, 17> PawPrintConfigPacket{
	0x50, // Expected header
	0x01, // Yellow
	// (For some reason, 0x07 (white) causes the device to disconnect)
	0xD0, // Mode: Report

	// Placeholder/Setting bytes * 14
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00
};

enum class StrengthMode : std::uint8_t
{
	Unchanged = 0b00,
	Increase = 0b01,
	Decrease = 0b10,
	Absolute = 0b11
};

constexpr std::uint8_t packageStrengthMode =
static_cast<std::uint8_t>(
	(static_cast<std::uint8_t>(StrengthMode::Absolute) << 2) |
	(static_cast<std::uint8_t>(StrengthMode::Absolute))
	);

std::array<std::uint8_t, 20> BuildB0Packet(
	std::uint8_t sequence,
	WaveformSample<4> waveformSamples,
	const std::array<std::uint8_t, 4>& frequencyA = { 10, 10, 10, 10 },
	const std::array<std::uint8_t, 4>& frequencyB = { 10, 10, 10, 10 }
) {
	std::array<std::uint8_t, 20> packet{};

	sequence &= 0x0F;

	packet[0] = 0xB0;
	packet[1] = static_cast<std::uint8_t>((sequence << 4) | packageStrengthMode);

	packet[2] = static_cast<std::uint8_t>(
		std::clamp(waveformSamples.maxStrengthL, 0.0f, MAX_STRENGTH)
		);
	packet[3] = static_cast<std::uint8_t>(
		std::clamp(waveformSamples.maxStrengthR, 0.0f, MAX_STRENGTH)
		);

	std::copy(
		frequencyA.begin(),
		frequencyA.end(),
		packet.begin() + 4
	);

	std::array<std::uint8_t, 4> waveformStrengthLBytes{};
	std::transform(
		waveformSamples.waveformStrengthL.begin(),
		waveformSamples.waveformStrengthL.end(),
		waveformStrengthLBytes.begin(),
		[](float value)
		{
			value = std::clamp(value, 0.0f, 1.0f) * 100.0f;
			return static_cast<std::uint8_t>(value);
		}
	);
	std::copy(
		waveformStrengthLBytes.begin(),
		waveformStrengthLBytes.end(),
		packet.begin() + 8
	);

	std::copy(
		frequencyB.begin(),
		frequencyB.end(),
		packet.begin() + 12
	);

	std::array<std::uint8_t, 4> waveformStrengthRBytes{};
	std::transform(
		waveformSamples.waveformStrengthR.begin(),
		waveformSamples.waveformStrengthR.end(),
		waveformStrengthRBytes.begin(),
		[](float value)
		{
			value = std::clamp(value, 0.0f, 1.0f) * 100.0f;
			return static_cast<std::uint8_t>(value);
		}
	);

	std::copy(
		waveformStrengthRBytes.begin(),
		waveformStrengthRBytes.end(),
		packet.begin() + 16
	);

	return packet;
}

CoyoteBLEBackend::CoyoteBLEBackend() {
	//winrt::init_apartment();
	winrt::init_apartment(winrt::apartment_type::multi_threaded);
}

void CoyoteBLEBackend::StartScan()
{
	std::scoped_lock lock(m_Mutex);

	using namespace winrt::Windows::Devices::Bluetooth::Advertisement;

	m_AdvertisementWatcher = BluetoothLEAdvertisementWatcher{};
	m_AdvertisementWatcher.ScanningMode(
		BluetoothLEScanningMode::Active
	);

	m_AdReceivedToken = m_AdvertisementWatcher.Received(
		[this](
			const BluetoothLEAdvertisementWatcher&,
			const BluetoothLEAdvertisementReceivedEventArgs& args)
		{
			const winrt::hstring localName = 
				args.Advertisement().LocalName();

			if (localName.empty())
				return;

			std::string name = winrt::to_string(localName);
			BLEDeviceType deviceType;

			if (name == "47L121000")
			{
				name = "Coyote Pulse Unit V3";
				deviceType = BLEDeviceType::CoyoteV3;
			}
			else if (name == "47L120300")
			{
				name = "PawPrint V1.1";
				deviceType = BLEDeviceType::PawPrintV1_1;
			}
			// PawPrint V1.0 advertises as 47L120100
			// But requires a firmware update via the app
			else
			{
				//deviceType = BLEDeviceType::Unknown;
				return;
			}

			std::scoped_lock lock(m_Mutex);
			BLEAdvertisementInfo& ad = m_Advertisements[args.BluetoothAddress()];
			ad.name = name;
			ad.type = deviceType;
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
	/* The scanning is not frequent, so locking at every frame is fine */
	std::scoped_lock lock(m_Mutex);

	std::erase_if(
		m_Advertisements,
		[](const std::pair<const std::uint64_t, BLEAdvertisementInfo>& p) {
			return
				p.second.connectionState == BLEConnectionState::Connected ||
				std::chrono::steady_clock::now() - p.second.lastSeen >
				std::chrono::seconds(5);
		}
	);

	return m_Advertisements;
}

IAsyncOperation<bool> CoyoteBLEBackend::ConnectAsync(std::uint64_t address)
{
	BLEDeviceType deviceType;
	{
		std::scoped_lock lock(m_Mutex);
		if (!m_Advertisements.contains(address))
			co_return false;

		deviceType = m_Advertisements[address].type;
		if (deviceType == BLEDeviceType::Unknown)
			co_return false;
		m_Advertisements[address].connectionState = BLEConnectionState::Connecting;
	}

	using namespace winrt::Windows::Devices::Bluetooth;
	using namespace winrt::Windows::Devices::Bluetooth::GenericAttributeProfile;

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
		const GattDeviceServicesResult commandServiceResult =
			co_await device.GetGattServicesForUuidAsync(
				ServiceUUID,
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
				WriteUUID,
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
				NotifyUUID,
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
		winrt::event_token cmdNotifyChangedToken{};
		switch (deviceType)
		{
		case BLEDeviceType::CoyoteV3:
			cmdNotifyChangedToken =
				commandNotifyCharacteristic.ValueChanged(
					[this](
						const auto&,
						const GattValueChangedEventArgs& args
						)
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
			break;
		case BLEDeviceType::PawPrintV1_1:
			cmdNotifyChangedToken =
				commandNotifyCharacteristic.ValueChanged(
					[this](
						const auto&,
						const GattValueChangedEventArgs& args
						)
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
						case 0x51:
							// After 0x50 configuration
							// Or when battery changed
							break;
						case 0x5A:
							// Trigger fired
							break;
						case 0x5B:
							// Trigger cancelled
							break;
						case 0x5C:
							// Trigger parameter changed
							break;
						case 0xD0:
						{
							// The x y and z describes the gravity direction w.r.t the
							// Left-handed XYZ axes relative to the device
							// X+: Right when facing the logo
							// Y+: The belt clip on the shorter edge
							// Z+: Normal of the chargeport & power panel
							float gx = static_cast<int8_t>(bytes[5]) / 64.0f;
							float gy = static_cast<int8_t>(bytes[6]) / 64.0f;
							float gz = static_cast<int8_t>(bytes[7]) / 64.0f;

							std::string msg = std::format(
								"[BLE][INFO] D0: Light {}, {}, Acc {}, X {:.2f}, Y {:.2f}, Z {:.2f}\n",
								(bytes[1] == 0x00 ? "Off" : "On"),
								(bytes[3] == 0x00 ? "Unpressed" : "Pressed"),
								bytes[4],
								gx,
								gy,
								gz
							);
							std::cout << msg;
							break;
						}
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
			break;
		default:
			break;
		}

		const GattCharacteristic commandWriteCharacteristic =
			commandWriteResult.Characteristics().GetAt(0);

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

		if (deviceType == BLEDeviceType::PawPrintV1_1)
		{
			DataWriter configWriter;
			configWriter.WriteBytes(PawPrintConfigPacket);

			const GattWriteResult configResult =
				co_await commandWriteCharacteristic.WriteValueWithResultAsync(
					configWriter.DetachBuffer(),
					GattWriteOption::WriteWithoutResponse
				);

			if (configResult.Status() != GattCommunicationStatus::Success)
			{
				commandNotifyCharacteristic.ValueChanged(cmdNotifyChangedToken);

				std::scoped_lock lock(m_Mutex);
				m_Advertisements[address].connectionState = BLEConnectionState::Failed;
				co_return false;
			}
		}

		DisconnectPulseUnit();

		std::scoped_lock lock(m_Mutex);
		// Store these only after every discovery step succeeds.
		switch (deviceType)
		{
		case BLEDeviceType::CoyoteV3:
			m_PulseUnit.device = device;

			m_PulseUnit.commandService = commandService;
			m_PulseUnit.writeCharacteristic = commandWriteCharacteristic;
			m_PulseUnit.notifyCharacteristic = commandNotifyCharacteristic;
			m_PulseUnit.notifyToken = cmdNotifyChangedToken;

			m_Advertisements[address].connectionState = BLEConnectionState::Connected;
			std::cerr << "[BLE][WHY??] " << address << "\n";
			break;
		case BLEDeviceType::PawPrintV1_1:
			m_PawPrint.device = device;

			m_PawPrint.commandService = commandService;
			m_PawPrint.writeCharacteristic = commandWriteCharacteristic;
			m_PawPrint.notifyCharacteristic = commandNotifyCharacteristic;
			m_PawPrint.notifyToken = cmdNotifyChangedToken;

			m_Advertisements[address].connectionState = BLEConnectionState::Connected;
			break;
		default:
			break;
		}

		co_return true;
	}
	catch (const winrt::hresult_error& error)
	{
		std::cerr
			<< "[BLE][Error] CCCD threw: "
			<< winrt::to_string(error.message())
			<< '\n';

		DisconnectPulseUnit();
		m_Advertisements[address].connectionState = BLEConnectionState::Failed;
		co_return false;
	}
}

void CoyoteBLEBackend::DisconnectPulseUnit()
{
	std::scoped_lock lock(m_Mutex);

	if (!m_PulseUnit.device)
		return;

	// ========== Tokens ==========
	if (m_PulseUnit.notifyToken)
	{
		m_PulseUnit.notifyCharacteristic.ValueChanged(
			m_PulseUnit.notifyToken
		);
	}
	// ========== Services ==========
	if (m_PulseUnit.commandService)
	{
		m_PulseUnit.commandService.Close();
	}
	//if (m_PulseUnit.batteryService)
	//{
	//	m_PulseUnit.batteryService.Close();
	//}
	// ========== Device ==========
	if (m_PulseUnit.device)
	{
		m_PulseUnit.device.Close();
		m_PulseUnit.device = nullptr;
	}
}

bool CoyoteBLEBackend::IsPulseUnitConnected()
{
	std::scoped_lock lock(m_Mutex);

	return m_PulseUnit.device &&
		m_PulseUnit.device.ConnectionStatus() ==
		Bluetooth::BluetoothConnectionStatus::Connected;
}

int CoyoteBLEBackend::IsPawPrintConnected()
{
	std::scoped_lock lock(m_Mutex);

	if (m_PawPrint.device &&
		m_PawPrint.device.ConnectionStatus() ==
		Bluetooth::BluetoothConnectionStatus::Connected)
	{
		return 1;
	}
	return 0;
}

IAsyncOperation<bool> CoyoteBLEBackend::WriteCommandAsync(WaveformSample<4> waveformSamples)
{
	if (m_SafetyOn.load())
		co_return true;

	using namespace winrt::Windows::Devices::Bluetooth::GenericAttributeProfile;

	GattCharacteristic writeCharacteristic{ nullptr };
	{
		std::scoped_lock lock(m_Mutex);
		writeCharacteristic = m_PulseUnit.writeCharacteristic;
	}

	if (!writeCharacteristic)
		co_return false;

	DataWriter writer;
	writer.WriteBytes(BuildB0Packet(10, waveformSamples));

	const GattWriteResult result =
		co_await writeCharacteristic.WriteValueWithResultAsync(
			writer.DetachBuffer(),
			GattWriteOption::WriteWithoutResponse
		);

	co_return
		result.Status() == GattCommunicationStatus::Success;
}