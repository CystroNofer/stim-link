**English** | [简体中文](README.zh.md)

# StimLink

  **Controller vibration feels unimpressive during your epic gameplay?**

**StimLink** is a Windows desktop application for converting real-time controller vibrations into controllable output for supported e-stim devices.

# ⚠️ SAFETY WARNING — READ BEFORE USE

> [!CAUTION]  
> **This software is designed for hardware capable of causing pain, injury, or other harm.**
> 
> It is strongly advised to personally review and verify all safety limits, emergency-stop behavior, disconnection handling, output caps, and device-specific assumptions before use.
> 
> Make any changes necessary for your hardware and use case before operation.
> 
> **Use entirely at your own risk. The author assumes no responsibility or liability for injury, damage, or other harm resulting from the distribution, modification, use, or misuse of this project.**

----------

The project currently focuses on direct Bluetooth Low Energy communication with the **DG-LAB Coyote V3**, including vibration recording, waveform generation, strength control, device notifications, and a Dear ImGui-based interface.

  

> This project is under active development. Interfaces, behavior, and protocol handling are subject to change.

  

## Supported Devices

  

### DG-LAB Coyote V3

  

The current backend communicates directly with the Coyote V3 through Bluetooth Low Energy.

  

Implemented functionality includes:

  

* BLE advertisement scanning

* GATT service and characteristic discovery

* Command writes through the Coyote write characteristic

* Device notifications through the notify characteristic

* Continuous waveform packet transmission

  
----------
**Additional device backends may be added in the future**

  

The signal buffer and sampler do not produce Coyote packets directly. Instead, it returns normalized waveform data that each backend can translate into its own protocol.

  

## Requirements

  

* Windows 10 or Windows 11

* A Bluetooth Low Energy adapter

* A C++20-compatible compiler

* Windows SDK

* Visual Studio 2022 or a compatible MSVC toolchain

* Premake (VS 2026 solution included)

  

## Building

Generate the project files with Premake:

```bash

cd StimLink
premake5  vs2022

```


## Usage

  

1. Start the application.

2. Find nearby Bluetooth LE devices in the side panel.

4. Select any device from the list
	> The program currently only shows Coyotes nearby.
	> Coyotes usually do not have a human-readable name

5. Connect to the device.

6. Configure channel strength.

7. Start your game that supports controller vibration.

8. Use the emergency stop control whenever necessary.

  

The application samples the current signal state and sends waveform data to the device at a fixed interval.

  

For the Coyote V3, each waveform packet contains four 25ms samples, representing approximately 100 ms of output.

  
## Safety

  

This software controls hardware capable of producing electrical stimulation.

  

Use conservative values during development.

  

Use the emergency stop button to pause output

  

The authors are not responsible for injury, hardware damage, or misuse.

  

## Development Status

  

Current status:


* [x] Dear ImGui controls

* [x] BLE advertisement scanning

* [x] Coyote V3 connection

* [x] Command writes

* [x] Device notifications

* [x] Basic strength controls

* [ ] Battery percentage reads

* [ ] Improved disconnect handling

* [ ] Configurable safety limits

* [ ] Persistent settings

* [ ] Additional device support

* [ ] Release builds

  

## Contributing

  

Contributions, bug reports, and suggestions are welcome.

  

## License

This repository’s original source code is licensed under **GPL-3.0-or-later** per the requirements from VIIPER

Third-party components and materials remain subject to their respective
licenses and terms:

- VIIPER — GPL-3.0-or-later
- Dear ImGui — MIT License
- DG-LAB Bluetooth Protocol — subject to DG-LAB’s published  terms, including its restriction on unauthorized commercial use

The project license does not grant rights to third-party documentation, protocol materials, or other assets.

  

## Acknowledgements

  

* Microsoft C++/WinRT

* VIIPER

* Dear ImGui

* DG-LAB open-source protocol documentation

