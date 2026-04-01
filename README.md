# ⚡ Enhanced SonOff POW CT ESPHome Component

[![ESPHome](https://img.shields.io/badge/ESPHome-2F7B9D?style=for-the-badge&logo=esphome&logoColor=white)](https://esphome.io/)
[![Home Assistant](https://img.shields.io/badge/Home%20Assistant-41A8F5?style=for-the-badge&logo=home-assistant&logoColor=white)](https://www.home-assistant.io/)
[![License](https://img.shields.io/badge/License-MIT-blue.svg?style=for-the-badge)]() 
---

## ✨ Key Features

This custom ESPHome component fully utilises the Sonoff POW CT, providing advanced energy monitoring features, easy calibration and native Home Assistant integration.

* **Bi-directional Power Measurement**
    * Fast reading of **positive** (consumption) and **negative** (production) current and power values.

* **Separate Energy Meters**
    * Direct reading of accumulated energy, with dedicated energy sensors for **import** (total consumption) and **export** (total production).
    * Accumulated energy metrics can be backed up to the ESP's flash memory.

* **Advanced Debugging Tools**
    * Built-in functions to directly read and write to the measurement component registers via Home Assistant **Actions**:
        * `my_sonoff_powct_read_register`
        * `my_sonoff_powct_write_register`

* **Easy Calibration**
    * The ability to perform fine calibration of the POWCT at zero load and to correct proportional (gain) errors in current and power by specifying a multiplier in the YAML configuration.

* **Higher Current and Power Sensitivity**
    * Multiple conductor turns through the CT are supported. Useful for instalations where 100A current readings are excessive, and higher sensitivity at lower powers is desirable.
	
* **Faster display updates and lower network throughput**
    * The LCD screen is now updated twice a second, and 30 second averages are sent to Home Assistant.

* **Native Integration**
    * Integration with Home Assistant via the native ESPHome API (No Cloud).
	
* **Improved net energy metering**
    * Energy display shows net energy (received − exported), so export-dominant installations show a negative kWh value.

## 📚 Documentation & Guides
| Guide | Description | Link |
| :--- | :--- | :--- |
| **Installation Guide** | Procedure for flashing ESPHome | **[documents/INSTALL.md](documents/INSTALL.md)** |
| **Calibration Guide** | Instructions for performing a "zero point" calibration. | **[documents/CALIBRATION.md](documents/CALIBRATION.md)** |
***

## ⚠️ Disclaimer
This project replaces the stock firmware on a Sonoff device with custom ESPHome firmware. By using this project you acknowledge and accept the following:
*Warranty void: flashing third-party firmware will void the manufacturer's warranty on the device.
*No guarantee of safety or correctness: this firmware is provided as-is, without warranty of any kind. Bugs may exist that cause incorrect readings or unexpected device behaviour.
*Electrical safety: only qualified persons should install, open or handle mains-connected equipment. Always follow local electrical codes and regulations. If in doubt, consult a licensed electrician.
*No affiliation: this project is not affiliated with, endorsed by, or supported by Itead (Sonoff), Espressif, or any other manufacturer or vendor.
*Use at your own risk: the author(s) accept no liability for damage to property, data loss, personal injury, or any other harm arising from the use or misuse of this project.


## 🔧 Key Changes from Original (`mazkagaz/sonoff_powct_esphome`)
The following key functional improvements were made to the `components/cse7761` driver after forking, addressing reliability, accuracy, and configurability:

| Theme | Summary |
| :--- | :--- |
| **Chip detection** | Changed from a SYSCON register heuristic (`0x0A04`) to an explicit CHIP\_ID read (`0x776110`). This is a more reliable presence check and logs the actual ID read on failure, making hardware faults easier to diagnose. |
| **UART communication reliability** | The `read_once_()` function now sends the read command as a raw 2-byte frame (matching the CSE7761 protocol) instead of routing through the write helper that appended an unwanted CRC. A per-byte 20 ms timeout with `yield()` replaces the previous fire-and-forget read, preventing silent data corruption and keeping the watchdog alive. Similarly, `write_()` now always emits a full-length frame with a CRC rather than a variable-length one that omitted the CRC for zero-valued payloads. |
| **Data coherency** | `get_data_()` now polls the `DUPDIF` flag in the chip's interrupt-flag register before reading any measurements. This ensures all voltage, current, and power registers belong to the same 27.3 Hz update cycle, eliminating the possibility of reading a mix of old and new values. |
| **Multi-turn CT support** | Added `ct_turns_b` YAML option (integer 1–5, default 1). Current and power readings are divided by this value, allowing the component to be used with multi-turn CT installations. The POWCT uses a 100A split core CT. Sensitivity of lower current and power measurements can be increased by adding multiple turns of the conductor through the CT. This will reduce the maximum measureable current/power reading proportionally (i.e. three turns will yield a maximum current reading of 33.3A, but will increase sensitivity by a factor of 3). This is useful for instalations where 100A maximum readings are excessive. |
| **Optional energy persistence** | Energy accumulation was always saved to flash in the original. The new `persist_energy` boolean option (default `false`) makes flash writes opt-in, reducing wear on devices where energy totals do not need to survive a reboot. |
| **Independent per-channel calibration** | The original derived channel A offsets from channel B using two hardcoded scale factors (`10.46×` current, `−8.89×` power). The reworked calibration collects idle readings from both channels simultaneously and applies the mean directly to each channel independently, removing the dependency on those magic numbers and producing more accurate zero-point correction. |
| **Persistent calibration offsets** | Calibration offsets are now saved to a dedicated flash preference slot (`0x2A3B4C5D`) and restored on every boot, so a calibration run does not need to be repeated after a restart. |
| **Single-shot calibration** | After collecting `CALIBRATION_MEASUREMENTS` samples, calibration mode is now automatically disabled (`calibration_enabled_ = false`). The original immediately restarted a new cycle, making it impossible to inspect the result before further corrections were applied. |
| **Coefficient zero-guard** | `chip_init_()` now individually checks every coefficient for zero after the checksum validation, preventing a divide-by-zero in `coefficient_by_unit_()` even for chips that pass the checksum with a partially corrupt coefficient block. All 8 coefficient slots are also populated with defaults (vs. only 3 in the original). |
| **Channel B start threshold** | The channel B power start threshold (`PSTART_B`) was not written by the original; it is now set to `0x0008` to match channel A, ensuring consistent no-load behaviour on both channels. |
| **Current sign/validity handling** | RMS current values with the MSB set are now treated as invalid (substituted with zero) rather than sign-extended to a negative current, matching the CSE7761 datasheet definition of bit 23 as an invalid-data flag for RMS registers. |
| **Debug service address validation** | `read_register_service()` validated the register address against the 32-bit range (`> 0xFFFFFFFF`), which is always false for an `unsigned long`. The check is corrected to the 8-bit CSE7761 address space (`> 0xFF`). |
| **Code quality** | All French-language comments and log messages translated to English; log levels standardised (`ESP_LOGI` for measurement values); register constant names updated to be self-documenting; `double` literals used consistently throughout where `double` accumulators are involved. |


## 🔧 Known issues
Power display uses no decimal point for values in the −999 to 999 W range, and switches to kW with a decimal point for magnitudes ≥ 1000 (handles both large positive and large negative values within the 4-character display limit). There is no way to indicate when the display changes from W to kW units.


## Credits
mazkagaz for their enhanced version of the sonoff_powct_esphome component, which is was forked from.