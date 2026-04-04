#include "cse7761.h"

#include "esphome/core/log.h"

#include <sstream>
#include <iomanip>
#include <inttypes.h>
#include <cmath>

namespace esphome {
  namespace cse7761 {

    static const char *const TAG = "cse7761";

    /*********************************************************************************************\
     * CSE7761 - Energy  (Sonoff PowCT v2.x)
     *
     * Based on Tasmota source code
     * See https://github.com/arendst/Tasmota/discussions/10793
     * https://github.com/arendst/Tasmota/blob/development/tasmota/xnrg_19_cse7761.ino
     *
     * 2025 mazkagaz : Réimplémentation pour le Sonoff POWCT
     * 2026 octal-ip : Extended and overhauled to improve reliability and accuracy.
     * \*********************************************************************************************/

    static const int CSE7761_UREF = 42563;  // RmsUc
    static const int CSE7761_IREF = 52241;  // RmsIAC
    static const int CSE7761_PREF = 44513;  // PowerPAC

    // Configuration registers
    static const uint8_t CSE7761_REG_SYSCON = 0x00;           // (2) System control register (0x0A04)
    static const uint8_t CSE7761_REG_METER_CONTROL1 = 0x01;   // (2) Metering control register (0x0000)
    static const uint8_t CSE7761_REG_HFCONST = 0x02;          // (2) Pulse frequency register (0x1000)
    static const uint8_t CSE7761_REG_PSTART_A = 0x03;         // (2) Channel A start threshold (0x0060)
    static const uint8_t CSE7761_REG_PSTART_B = 0x04;         // (2) Channel B start threshold (0x0060)
    static const uint8_t CSE7761_REG_GAINCAL_A = 0x05;        // (2) Channel A gain calibration (0x0000)
    static const uint8_t CSE7761_REG_GAINCAL_B = 0x06;        // (2) Channel B gain calibration (0x0000)
    static const uint8_t CSE7761_REG_PHASECAL_A = 0x07;       // (1) Channel A phase calibration (0x0000)
    static const uint8_t CSE7761_REG_PHASECAL_B = 0x08;       // (1) Channel B phase calibration (0x0000)
    static const uint8_t CSE7761_REG_POWER_OFFSET_A = 0x0A;   // (2) Channel A power offset calibration (0x0000)
    static const uint8_t CSE7761_REG_POWER_OFFSET_B = 0x0B;   // (2) Channel B power offset calibration (0x0000)
    static const uint8_t CSE7761_REG_RMS_I_OFFSET_A = 0x0E;   // (2) Channel A current RMS offset (0x0000)
    static const uint8_t CSE7761_REG_RMS_I_OFFSET_B = 0x0F;   // (2) Channel B current RMS offset (0x0000)
    static const uint8_t CSE7761_REG_I_GAIN_B = 0x10;         // (2) Channel B current gain (0x0000)
    static const uint8_t CSE7761_REG_POWER_GAIN = 0x11;       // (2) Apparent power gain  calibration (0x0000)
    static const uint8_t CSE7761_REG_POWER_OFFSET = 0x12;     // (2) Apparent power offset calibration (0x0000)
    static const uint8_t CSE7761_REG_METER_CONTROL2 = 0x13;   // (2) Metering control register 2 (0x0001)
    static const uint8_t CSE7761_REG_V_SAG = 0x17;            // (2) Voltage sag register (0x0000)
    static const uint8_t CSE7761_REG_V_SAG_THRESHOLD = 0x18;  // (2) Voltage sag threshold (0x0000)
    static const uint8_t CSE7761_REG_OVER_V = 0x19;           // (2) Over-voltage threshold (0xFFFF)
    static const uint8_t CSE7761_REG_OVER_I_A = 0x1A;         // (2) Channel A over-current threshold (0xFFFF)
    static const uint8_t CSE7761_REG_OVER_I_B = 0x1B;         // (2) Channel B over-current threshold (0xFFFF)
    static const uint8_t CSE7761_REG_OVER_POWER = 0x1C;       // (2) Over-power threshold (0xFFFF)
    static const uint8_t CSE7761_REG_PULSE1SEL = 0x1D;        // (2) Pin function output select register (0x3210)


    // Metric registers
    static const uint8_t CSE7761_REG_IV_PHASE_ANGLE = 0x22; // (2) Current and voltage phase angle (0x0000)
    static const uint8_t CSE7761_REG_FREQ = 0x23;           // (2) Frequency (0x0000)
    static const uint8_t CSE7761_REG_RMS_I_A = 0x24;        // (3) Channel A current (0x000000)
    static const uint8_t CSE7761_REG_RMS_I_B = 0x25;        // (3) Channel B current (0x000000)
    static const uint8_t CSE7761_REG_RMS_V = 0x26;          // (3) Voltage (0x000000)
    static const uint8_t CSE7761_REG_POWER_FACTOR = 0x27;   // (3) Power factor (0x7FFFFF)
    static const uint8_t CSE7761_REG_ENERGY_A = 0x28;       // (3) Channel A energy (0x7FFFFF)
    static const uint8_t CSE7761_REG_ENERGY_B = 0x29;       // (3) Channel B energy (0x7FFFFF)
    static const uint8_t CSE7761_REG_POWER_A = 0x2C;        // (4) Channel A active power, update rate 27.2Hz (0x00000000)
    static const uint8_t CSE7761_REG_POWER_B = 0x2D;        // (4) Channel B active power, update rate 27.2Hz (0x00000000)
    static const uint8_t CSE7761_REG_APPARENT_POWER = 0x2E; // (4) Apparent power (0x00000000)

    static const uint8_t CSE7761_REG_PEAK_CURRENT_A = 0x30;      // (3) Channel A peak current (0x00000000)
    static const uint8_t CSE7761_REG_PEAK_CURRENT_B = 0x31;      // (3) Channel B peak current (0x00000000)
    static const uint8_t CSE7761_REG_PEAK_VOLTAGE = 0x32;        // (3) Peak voltage (0x00000000)
    static const uint8_t CSE7761_REG_INST_CURRENT_A = 0x33;      // (3) Channel A instantaneous current (0x00000000)
    static const uint8_t CSE7761_REG_INST_CURRENT_B = 0x34;      // (3) Channel B instantaneous current (0x00000000)
    static const uint8_t CSE7761_REG_INST_VOLTAGE = 0x35;        // (3) Instantaneous voltage (0x00000000)
    static const uint8_t CSE7761_REG_WAVEFORM_A = 0x36;          // (3) Channel A waveform (0x00000000)
    static const uint8_t CSE7761_REG_WAVEFORM_B = 0x37;          // (3) Channel B waveform (0x00000000)
    static const uint8_t CSE7761_REG_WAVEFORM_V = 0x38;          // (3) Voltage waveform (0x00000000)
    static const uint8_t CSE7761_REG_INST_POWER = 0x3C;          // (4) Instantaneous power (0x00000000)
    static const uint8_t CSE7761_REG_INST_APPARENT_POWER = 0x3D; // (4) Instantaneous apparent power (0x00000000)

    static const uint8_t CSE7761_REG_IF = 0x41;          // (2) Interrupt flag register (read-only, clears on read)
    static const uint8_t CSE7761_REG_SYSSTATUS = 0x43;  // (1) System status register

    static const uint8_t CSE7761_REG_COEFFCHKSUM = 0x6F;           // (2) Coefficient checksum
    static const uint8_t CSE7761_REG_CURRENT_CONV_A = 0x70;        // (2) Channel A effective current conversion coefficient
    static const uint8_t CSE7761_REG_CURRENT_CONV_B = 0x71;        // (2) Channel B effective current conversion coefficient
    static const uint8_t CSE7761_REG_VOLTAGE_CONV = 0x72;          // (2) Voltage effective conversion coefficient
    static const uint8_t CSE7761_REG_ACTIVE_POWER_CONV_A = 0x73;   // (2) Channel A active power conversion coefficient
    static const uint8_t CSE7761_REG_ACTIVE_POWER_CONV_B = 0x74;   // (2) Channel B active power conversion coefficient
    static const uint8_t CSE7761_REG_APPARENT_POWER_CONV = 0x75;   // (2) Apparent power conversion coefficient
    static const uint8_t CSE7761_REG_ENERGY_CONV_A = 0x76;         // (2) Channel A energy conversion coefficient
    static const uint8_t CSE7761_REG_ENERGY_CONV_B = 0x77;         // (2) Channel B energy conversion coefficient

    static const uint8_t CSE7761_REG_CHIP_ID = 0x7F;       // (2) Chip ID register

    static const uint8_t CSE7761_SPECIAL_COMMAND = 0xEA;   // Start special command
    static const uint8_t CSE7761_CMD_RESET = 0x96;         // Reset command, after receiving the command, the chip resets
    static const uint8_t CSE7761_CMD_FINISH_WRITE = 0xDC;  // Close write operation
    static const uint8_t CSE7761_CMD_START_WRITE = 0xE5;   // Enable write operation


    // Calibration constants
    static const int CALIBRATION_MEASUREMENTS = 20;


    enum CSE7761 { RMS_IAC, RMS_IBC, RMS_UC, POWER_PAC, POWER_PBC, POWER_SC, ENERGY_AC, ENERGY_BC };

    //***********************************************************************************************
    // setup: starting routine
    //***********************************************************************************************
    void CSE7761Component::setup() {
      this->write_(CSE7761_SPECIAL_COMMAND, CSE7761_CMD_RESET); // Reset the chip to ensure it's in a known state
      delay(100); // Wait for the chip to reset
      uint32_t chipID = this->read_(CSE7761_REG_CHIP_ID, 3);  // Check the chip ID to verify communication and presence of the CSE7761
      if ((0x776110 == chipID) && this->chip_init_()) {
        // CSE7761 is present and working
        this->write_(CSE7761_SPECIAL_COMMAND, CSE7761_CMD_FINISH_WRITE);
        ESP_LOGD(TAG, "CSE7761 found");
        this->data_.ready = true;
        // Static numeric identifier (random) 0x1F2B4A7D
        if (this->persist_energy_) {
          this->pref_ = global_preferences->make_preference<EnergyDataStruct>(0x1F2B4A7D, true);
          EnergyDataStruct saved_values;

          if (this->pref_.load(&saved_values)) {
            this->accumulated_energy_received_ = saved_values.received;
            this->accumulated_energy_exported_ = saved_values.exported;
            ESP_LOGCONFIG(TAG, "Loaded accumulated energy: %.3f Wh (Received), %.3f Wh (Exported)", this->accumulated_energy_received_, this->accumulated_energy_exported_);
          } else {
            ESP_LOGCONFIG(TAG, "No accumulated energy found, starting from 0.0 Wh.");
            this->accumulated_energy_received_ = 0.0f;
            this->accumulated_energy_exported_ = 0.0f;
          }
        } else {
          ESP_LOGCONFIG(TAG, "Energy persistence disabled, starting from 0.0 Wh.");
          this->accumulated_energy_received_ = 0.0f;
          this->accumulated_energy_exported_ = 0.0f;
        }

        // Static numeric identifier for calibration (random) 0x2A3B4C5D
        this->calibration_pref_ = global_preferences->make_preference<CalibrationDataStruct>(0x2A3B4C5D, true);
        CalibrationDataStruct saved_calibration;

        if (this->calibration_pref_.load(&saved_calibration)) {
          this->software_current_offset_A_ = saved_calibration.software_current_offset_A;
          this->software_current_offset_B_ = saved_calibration.software_current_offset_B;
          this->software_power_offset_A_ = saved_calibration.software_power_offset_A;
          this->software_power_offset_B_ = saved_calibration.software_power_offset_B;
          ESP_LOGCONFIG(TAG, "Loaded calibration offsets: Current A: %.3f A, Current B: %.3f A, Power A: %.3f W, Power B: %.3f W",
                        this->software_current_offset_A_, this->software_current_offset_B_,
                        this->software_power_offset_A_, this->software_power_offset_B_);
        } else {
          ESP_LOGCONFIG(TAG, "No calibration offsets found, starting from 0.0.");
          this->software_current_offset_A_ = 0.0f;
          this->software_current_offset_B_ = 0.0f;
          this->software_power_offset_A_ = 0.0f;
          this->software_power_offset_B_ = 0.0f;
        }
      } else {
        ESP_LOGE(TAG, "CSE7761 not found, expected 0x776110, got 0x%06" PRIX32, chipID);
        this->mark_failed();
      }
    }

    //***********************************************************************************************
    // set_calibration_mode : start/stop calibration process
    //***********************************************************************************************
    void CSE7761Component::set_calibration_mode(bool state) {
      if (this->calibration_enabled_ != state) {
        this->calibration_enabled_ = state;
        ESP_LOGI(TAG, "Calibration mode %s", state ? "ENABLED" : "DISABLED");
        if (!state) {
          this->calibration_count_ = 0;
          this->sum_current_A_ = 0;
          this->sum_power_A_ = 0;
        }
      }
    }

    //***********************************************************************************************
    // perform_calibration_write_ : perform calibration as soon as CALIBRATION_MEASUREMENTS
    // measurements have been collected
    //***********************************************************************************************
    void CSE7761Component::perform_calibration_write_() {

      float calibration_count_F = (float)this->calibration_count_;
      // Each channel is corrected directly from its own mean idle reading.
      // Subtracting the mean drives the post-offset value toward zero.
      this->software_current_offset_A_ -= this->sum_current_A_ / calibration_count_F;
      this->software_power_offset_A_   -= this->sum_power_A_   / calibration_count_F;
      ESP_LOGI(TAG, "Calibration done: Software offsets - Current A: %.3f A, Power A: %.3f W",
              this->software_current_offset_A_, this->software_power_offset_A_);

      // Persist the new offsets so they survive a reboot
      CalibrationDataStruct cal_data = {
        .software_current_offset_A = this->software_current_offset_A_,
        .software_current_offset_B = this->software_current_offset_B_,
        .software_power_offset_A   = this->software_power_offset_A_,
        .software_power_offset_B   = this->software_power_offset_B_,
      };
      this->calibration_pref_.save(&cal_data);

      // Reset accumulators and stop — re-enable the switch to run another cycle.
      this->calibration_count_ = 0;
      this->sum_current_A_ = 0;
      this->sum_power_A_ = 0;
      this->calibration_enabled_ = false;
      ESP_LOGI(TAG, "Calibration complete: mode disabled.");
    }

    //***********************************************************************************************
    // dump_config
    //***********************************************************************************************
    void CSE7761Component::dump_config() {
      ESP_LOGCONFIG(TAG, "CSE7761:");
      if (this->is_failed()) {
        ESP_LOGE(TAG, ESP_LOG_MSG_COMM_FAIL);
      }
      LOG_UPDATE_INTERVAL(this);
      this->check_uart_settings(38400, 1, uart::UART_CONFIG_PARITY_EVEN, 8);
    }

    //***********************************************************************************************
    // get_setup_priority
    //***********************************************************************************************
    float CSE7761Component::get_setup_priority() const { return setup_priority::DATA; }

    //***********************************************************************************************
    // update : measurements update
    //***********************************************************************************************
    void CSE7761Component::update() {
      if (this->data_.ready) {this->get_data_();}
    }

    //***********************************************************************************************
    // write_ : write data "data" to rgister "reg"
    // - uint8_t reg : register address
    // - uint16_t data : data to write
    //***********************************************************************************************
    void CSE7761Component::write_(uint8_t reg, uint16_t data) {
      uint8_t buffer[5];

      buffer[0] = 0xA5;
      buffer[1] = reg;
      uint32_t len = 4;
      if (reg == CSE7761_SPECIAL_COMMAND) {
        buffer[2] = data & 0xFF;
        len = 3;
      } else {
        buffer[2] = (data >> 8) & 0xFF;
        buffer[3] = data & 0xFF;
      }

      uint8_t crc = 0;
      for (uint32_t i = 0; i < len; i++) {
        crc += buffer[i];
      }
      buffer[len] = ~crc;
      len++;

      this->write_array(buffer, len);
    }

    //***********************************************************************************************
    // read_once_ : try one register read
    // - uint8_t reg : register address
    // - uint8_t size : register size
    // - uint32_t *value : pointer to read data returned
    // TODO: add an enum with register sizes to simplify this function and all reading and writing
    //       functions.
    //***********************************************************************************************
    bool CSE7761Component::read_once_(uint8_t reg, uint8_t size, uint32_t *value) {
      // 1. Clear the RX buffer to remove any stale data that might interfere with the new read operation
      while (this->available()) {
        this->read();
      }

      // 2. Send the read command frame: 0xA5 + reg (no data/CRC for read request)
      uint8_t read_cmd[2] = {0xA5, reg};
      this->write_array(read_cmd, sizeof(read_cmd));

      uint8_t buffer[8] = {0};
      uint32_t rcvd = 0;

      // 3a. Receive data
      for (uint32_t i = 0; i <= size; i++) {
        uint32_t start_time = millis();
        bool byte_received = false;

        while (millis() - start_time < 20) { // Wait up to 20ms for each byte
          if (this->available()) {
            int val = this->read();
            if (val > -1 && rcvd < sizeof(buffer) - 1) {
              buffer[rcvd++] = val;
              byte_received = true;
              break; // Exit the while loop, move to next 'i'
            }
          }
          yield(); // Let ESPHome handle background tasks (WiFi/Watchdog)
        }

        if (!byte_received) {
          ESP_LOGE(TAG, "Timeout waiting for byte %d of register 0x%02X", i, reg);
          return false;
        }
      }

      // 3b. Validate checksum and extract value
      rcvd--;
      uint32_t result = 0;
      uint8_t crc = 0xA5 + reg;
      for (uint32_t i = 0; i < rcvd; i++) {
        result = (result << 8) | buffer[i];
        crc += buffer[i];
      }
      crc = ~crc;
      if (crc != buffer[rcvd]) {
        ESP_LOGE(TAG, "Checksum mismatch for register 0x%02X", reg);
        return false;
      }

      *value = result;
      return true;
    }

    //***********************************************************************************************
    // read_ : read register data by trying 3 times max to call read_once_
    // - uint8_t reg : register address
    // - uint8_t size : register size
    // return uint32_t value : data read in register
    // TODO: add an enum with register sizes to simplify this function and all reading and writing
    //       functions.
    //***********************************************************************************************
    uint32_t CSE7761Component::read_(uint8_t reg, uint8_t size) {
      uint8_t retry = 3;    // Retry up to three times
      uint32_t value = 0;   // Default no value
      while (retry > 0) {
        retry--;
        if (this->read_once_(reg, size, &value))
          return value;
      }
      ESP_LOGE(TAG, "Reading register 0x%02X failed!", reg);
      return value;
    }

    //***********************************************************************************************
    // coefficient_by_unit_ : coef to convert raw measurements
    // - uint32_t unit : index of measurements, see enum CSE7761
    //***********************************************************************************************
    uint32_t CSE7761Component::coefficient_by_unit_(uint32_t unit) {
      switch (unit) {
        case RMS_IAC:
          return (0x800000 * 100 / this->data_.coefficient[RMS_IAC]) * 10 / 4.7;  // Stay within 32 bits // *100*10 mA->A // K_1=4.7
        case RMS_IBC:
          return (0x800000 * 100 / this->data_.coefficient[RMS_IBC]) * 10 / 4.7;  // Stay within 32 bits // *100*10 mA->A // K_1=4.7
        case RMS_UC:
          return 0x400000 * 100 / this->data_.coefficient[RMS_UC]; // *100 10mV->V // K_2=1
        case POWER_PAC:
          return 0x80000000 / this->data_.coefficient[POWER_PAC] / 4.7; // K_1=4.7 // K_2=1
        case POWER_PBC:
          return 0x80000000 / this->data_.coefficient[POWER_PBC] / 4.7; // K_1=4.7 // K_2=1
          // TODO: to verify the folowing formulas are OK with CSE7761 datasheet
        case POWER_SC:
          return 0x80000000 / this->data_.coefficient[POWER_SC];
        case ENERGY_AC:
          return 0x80000000 / this->data_.coefficient[ENERGY_AC];
        case ENERGY_BC:
          return 0x80000000 / this->data_.coefficient[ENERGY_BC];
      }
      return 0;
    }

    //***********************************************************************************************
    // chip_init_ : init all the sys_status registers of the chip to make
    // return TRUE if OK, else return FALSE
    // TODO: link the configuration choices with external triggers and be able to change them at any
    //       time
    //***********************************************************************************************
    bool CSE7761Component::chip_init_() {
      // Perform a data integrity check of the coefficients read from the chip, and if the check fails, set defaults
      uint16_t calc_chksum = 0xFFFF;
      for (uint32_t i = 0; i < 8; i++) {
        this->data_.coefficient[i] = this->read_(CSE7761_REG_CURRENT_CONV_A + i, 2);
        calc_chksum += this->data_.coefficient[i];
      }
      calc_chksum = ~calc_chksum;
      uint16_t coeff_chksum = this->read_(CSE7761_REG_COEFFCHKSUM, 2);
      if ((calc_chksum != coeff_chksum) || (!calc_chksum)) {
        ESP_LOGW(TAG, "Coefficient checksum mismatch, using default calibration values");
        this->data_.coefficient[RMS_IAC] = CSE7761_IREF;
        this->data_.coefficient[RMS_IBC] = CSE7761_IREF;
        this->data_.coefficient[RMS_UC] = CSE7761_UREF;
        this->data_.coefficient[POWER_PAC] = CSE7761_PREF;
        this->data_.coefficient[POWER_PBC] = CSE7761_PREF;
        this->data_.coefficient[POWER_SC] = CSE7761_PREF;
        this->data_.coefficient[ENERGY_AC] = CSE7761_PREF;
        this->data_.coefficient[ENERGY_BC] = CSE7761_PREF;
      }

      // Prevent divide-by-zero and invalid scaling even when checksum looks valid.
      if (!this->data_.coefficient[RMS_IAC]) {
        this->data_.coefficient[RMS_IAC] = CSE7761_IREF;
      }
      if (!this->data_.coefficient[RMS_IBC]) {
        this->data_.coefficient[RMS_IBC] = CSE7761_IREF;
      }
      if (!this->data_.coefficient[RMS_UC]) {
        this->data_.coefficient[RMS_UC] = CSE7761_UREF;
      }
      if (!this->data_.coefficient[POWER_PAC]) {
        this->data_.coefficient[POWER_PAC] = CSE7761_PREF;
      }
      if (!this->data_.coefficient[POWER_PBC]) {
        this->data_.coefficient[POWER_PBC] = CSE7761_PREF;
      }
      if (!this->data_.coefficient[POWER_SC]) {
        this->data_.coefficient[POWER_SC] = CSE7761_PREF;
      }
      if (!this->data_.coefficient[ENERGY_AC]) {
        this->data_.coefficient[ENERGY_AC] = CSE7761_PREF;
      }
      if (!this->data_.coefficient[ENERGY_BC]) {
        this->data_.coefficient[ENERGY_BC] = CSE7761_PREF;
      }

      this->write_(CSE7761_SPECIAL_COMMAND, CSE7761_CMD_START_WRITE); // Enable write to protected registers
      uint8_t sys_status = this->read_(CSE7761_REG_SYSSTATUS, 1); // Verify write enable status

      if (sys_status & 0x10) {  // Write enable to protected registers (WREN)
        this->write_(CSE7761_REG_SYSCON | 0x80, 0xFE00); // Enable ADC 1 and 2, PGA=1x for all channels
        // EMUCON: comparator disabled, Pmode=01 (accumulate positive energy only),
        //         HPF enabled on all channels, PARUN+PBRUN=1 (pulse/energy accumulation active)
        this->write_(CSE7761_REG_METER_CONTROL1 | 0x80, 0x1583);
        // EMUCON2: EPA_CB+EPB_CB=1 (required in UART mode – energy registers do not auto-clear),
        //          DUPSEL=11 (mean register update rate = 27.3 Hz), CHS_IB=1 (measure Ch B current,
        //          not temperature), PfactorEN+WaveEN+SAGEN+OverEN+ZxEN+PeakEN all enabled
        this->write_(CSE7761_REG_METER_CONTROL2 | 0x80, 0x0FC1);
        this->write_(CSE7761_REG_PSTART_A | 0x80, 0x0008);  // Channel A no-load threshold: high sensitivity
        this->write_(CSE7761_REG_PSTART_B | 0x80, 0x0008);  // Channel B no-load threshold: match channel A
        this->write_(CSE7761_REG_PULSE1SEL | 0x80, 0x3290); // Set channel A active power as the source for pulse output 1
      } else {
        ESP_LOGE(TAG, "Write failed at chip_init");
        return false;
      }
      return true;
    }

    //***********************************************************************************************
    // get_data_ : get measurements from chip and convert them to SI units
    // Reads voltage and channel A current/power from CSE7761; channel B only if sensors are configured.
    // Applies calibration offsets and calculates accumulated energy
    //***********************************************************************************************
    void CSE7761Component::get_data_() {
      // Poll DUPDIF (IF register bit 0) to ensure the chip has completed a full mean-data
      // update cycle before reading any measurement registers.  The chip signals this at the
      // DUPSEL rate (27.3 Hz ≈ every 37 ms).  Reading IF clears the flag automatically.
      // This prevents reading a mix of old and new values across the register set.
      uint32_t poll_start = millis();
      bool data_ready = false;
      while ((millis() - poll_start) < 50) {  // 50 ms max — just over one 27.3 Hz cycle
        if (this->read_(CSE7761_REG_IF, 2) & 0x01) {
          data_ready = true;
          break;
        }
        delay(2);
      }
      if (!data_ready) {
        ESP_LOGW(TAG, "Timed out waiting for DUPDIF – reading possibly stale data");
      }

      // Read voltage (24-bit unsigned, invalid if MSB set)
      uint32_t raw_voltage = this->read_(CSE7761_REG_RMS_V, 3);
      this->data_.voltage_rms = (raw_voltage >= 0x800000) ? 0 : raw_voltage;
      float voltage = (float) this->data_.voltage_rms / this->coefficient_by_unit_(RMS_UC);
      if (this->voltage_sensor_ != nullptr) {
        this->voltage_sensor_->publish_state(voltage);
        ESP_LOGI(TAG, "Voltage: %.1f V", voltage);
      }

      // Read current channel A (24-bit format, treat MSB-set as invalid)
      uint32_t raw_current_a = this->read_(CSE7761_REG_RMS_I_A, 3);
      this->data_.current_rms[0] = (raw_current_a >= 0x800000) ? 0 : raw_current_a;
      this->active_current_A_ = (((float) this->data_.current_rms[0]) / this->coefficient_by_unit_(RMS_IAC)) / (float)this->ct_turns_ * this->current_gain_a_ + this->software_current_offset_A_;
      if (this->current_sensor_1_ != nullptr) {
        this->current_sensor_1_->publish_state(this->active_current_A_);
        ESP_LOGI(TAG, "Current A: %.3f A", this->active_current_A_);
      }

      // Read current channel B (24-bit format, treat MSB-set as invalid)
      if (this->current_sensor_2_ != nullptr) {
        uint32_t raw_current_b = this->read_(CSE7761_REG_RMS_I_B, 3);
        this->data_.current_rms[1] = (raw_current_b >= 0x800000) ? 0 : raw_current_b;
        float active_current_B = (((float) this->data_.current_rms[1]) / this->coefficient_by_unit_(RMS_IBC)) + this->software_current_offset_B_;
        this->current_sensor_2_->publish_state(active_current_B);
        ESP_LOGI(TAG, "Current B: %.3f A", active_current_B);
      }

      // Read power channel A (32-bit signed in two's complement format)
      uint32_t now = esphome::millis();
      int32_t raw_power_a = this->read_(CSE7761_REG_POWER_A, 4);
      this->data_.active_power[0] = raw_power_a;
      this->active_power_A_ = (((float) this->data_.active_power[0]) / this->coefficient_by_unit_(POWER_PAC)) / (float)this->ct_turns_ * this->current_gain_a_ + this->software_power_offset_A_;
      if (this->power_sensor_1_ != nullptr) {
        this->power_sensor_1_->publish_state(this->active_power_A_);
        ESP_LOGI(TAG, "Active Power A: %.3f W", this->active_power_A_);
      }

      // Calculate energy accumulation for channel A
      if (!this->ok_energy_) {
        // First measurement - initialize energy tracking
        this->last_active_power_A_ = this->active_power_A_;
        this->last_update_time_ = now;
        this->ok_energy_ = true;
      } else {
        // Unsigned subtraction is wrap-safe across millis() overflow.
        uint32_t elapsed_ms = now - this->last_update_time_;
        double time_delta_s = elapsed_ms / 1000.0;
        this->last_update_time_ = now;
        double mean_power = (this->last_active_power_A_ + this->active_power_A_) / 2.0;
        this->last_active_power_A_ = this->active_power_A_;

        // Energy = Power (W) * Time (s) / 3600 (s/h) = Wh
        double energy_delta_wh = (mean_power * time_delta_s) / 3600.0;

        if (mean_power > 0.0) {
          // Consuming energy
          this->accumulated_energy_received_ += energy_delta_wh;
          if (this->energy_received_ != nullptr) {
            this->energy_received_->publish_state(this->accumulated_energy_received_ / 1000.0); // Convert to kWh
          }
        } else {
          // Producing energy (negative power)
          this->accumulated_energy_exported_ -= energy_delta_wh;
          if (this->energy_exported_ != nullptr) {
            this->energy_exported_->publish_state(this->accumulated_energy_exported_ / 1000.0); // Convert to kWh
          }
        }

        ESP_LOGI(TAG, "Energy: Time delta=%.1fs, Power average=%.3fW, Energy delta=%.6fWh, Energy received=%.3fkWh, Energy exported=%.3fkWh",
                 time_delta_s, mean_power, energy_delta_wh,
                 this->accumulated_energy_received_ / 1000.0,
                 this->accumulated_energy_exported_ / 1000.0);
      }

      // Read power factor and reactive power (channel A)
      if (this->power_factor_ != nullptr || this->reactive_power_ != nullptr) {
        // Power factor register: 24-bit signed, 0x7FFFFF = 1.0
        uint32_t raw_pf = this->read_(CSE7761_REG_POWER_FACTOR, 3);
        int32_t signed_pf = (raw_pf >= 0x800000) ? (int32_t)(raw_pf - 0x1000000) : (int32_t)raw_pf;
        float pf = (float)signed_pf / (float)0x7FFFFF;
        if (this->power_factor_ != nullptr) {
          this->power_factor_->publish_state(pf);
          ESP_LOGI(TAG, "Power Factor: %.3f", pf);
        }
        if (this->reactive_power_ != nullptr) {
          // Compute apparent power from calibrated V and I rather than the hardware
          // register (whose POWER_SC coefficient is missing the CT K_1=4.7 factor).
          float apparent_power = voltage * this->active_current_A_;
          float p = this->active_power_A_;
          float q_sq = apparent_power * apparent_power - p * p;
          float reactive_power = (q_sq > 0.0f) ? sqrtf(q_sq) : 0.0f;
          this->reactive_power_->publish_state(reactive_power);
          ESP_LOGI(TAG, "Reactive Power: %.3f VAR (Apparent: %.3f VA)", reactive_power, apparent_power);
        }
      }

      // Read power channel B (32-bit signed in two's complement format)
      if (this->power_sensor_2_ != nullptr) {
        int32_t raw_power_b = this->read_(CSE7761_REG_POWER_B, 4);
        this->data_.active_power[1] = raw_power_b;
        float active_power_B = (((float) this->data_.active_power[1]) / this->coefficient_by_unit_(POWER_PBC)) + this->software_power_offset_B_;
        this->power_sensor_2_->publish_state(active_power_B);
        ESP_LOGI(TAG, "Power B: %.3f W", active_power_B);
      }

      // Handle calibration if enabled
      if (this->calibration_enabled_) {
        if (this->calibration_count_ == 0) {
          // Reset calibration accumulators at the start of each cycle
          this->sum_current_A_ = 0;
          this->sum_power_A_   = 0;
        }

        // Accumulate channel A (assumed idle / no-load during calibration)
        this->sum_current_A_ += this->active_current_A_;
        this->sum_power_A_   += this->active_power_A_;
        this->calibration_count_++;

        // Perform calibration when enough samples collected
        if (this->calibration_count_ >= CALIBRATION_MEASUREMENTS) {
          this->perform_calibration_write_();
        }
      }

      // Save accumulated energy to persistent storage every hour
      if (this->persist_energy_) {
        if (this->last_save_time_ == 0 || (esphome::millis() - this->last_save_time_) >= 3600000) {
          this->last_save_time_ = esphome::millis();

          EnergyDataStruct energy_data = {
            .received = this->accumulated_energy_received_,
            .exported = this->accumulated_energy_exported_
          };
          this->pref_.save(&energy_data);
          ESP_LOGV(TAG, "Saved accumulated energy: received=%.3f Wh, exported=%.3f Wh",
                   this->accumulated_energy_received_, this->accumulated_energy_exported_);
        }
      }
    }

    //***********************************************************************************************
    // read_register_service : advanced debug function to read registers and push datas in
    // home assistant entities. Make debug easier without recompile the code several times.
    // - std::string register_number_str: register number come as a string from home assistant
    // - int size : register size
    // TODO: add an enum with register sizes to simplify this function and all reading and writing
    //       functions.
    //***********************************************************************************************
    void CSE7761Component::read_register_service(std::string register_number_str, int size) {
      // Optional: Log the call
      ESP_LOGD(TAG, "Service called: Reading register %s (%d bytes).", register_number_str.c_str(), size);

      uint32_t register_number;
      char *end_ptr;
      unsigned long val = std::strtoul(register_number_str.c_str(), &end_ptr, 0);
      if (end_ptr == register_number_str.c_str() || *end_ptr != '\0') {
        ESP_LOGE(TAG, "Error: Invalid or non-numeric register input: '%s'", register_number_str.c_str());
        if (this->debug_sensor_hex_) this->debug_sensor_hex_->publish_state("Error: Invalid input format.");
        if (this->debug_sensor_bin_) this->debug_sensor_bin_->publish_state("Error: Invalid input format.");
        return;
      }
      if (val > 0xFF) {
        ESP_LOGE(TAG, "Error: The register address is outside the 8-bit range (0-0xFF): '%s'", register_number_str.c_str());
        if (this->debug_sensor_hex_) this->debug_sensor_hex_->publish_state("Error: Register out of range.");
        if (this->debug_sensor_bin_) this->debug_sensor_bin_->publish_state("Error: Register out of range.");
        return;
      }
      register_number = (uint32_t)val;

      std::vector<uint8_t> raw_data = read_register(register_number, size);

      std::stringstream ss_hex,ss_bin;
      for (uint8_t byte_value : raw_data){
        // hex
        ss_hex << std::uppercase << std::hex << std::setw(2) << std::setfill('0')
        << (int)byte_value << " ";
        // bin
        for (int i = 7; i >= 0; --i) {
          ss_bin << ((byte_value >> i) & 1);
        }
        ss_bin << " ";
      }

      std::string hex_data = ss_hex.str();
      std::string bin_data = ss_bin.str();
      ESP_LOGI(TAG, "Register 0x%lX value: %s=%s", register_number, hex_data.c_str(), bin_data.c_str());
      if (this->debug_sensor_hex_) {
        this->debug_sensor_hex_->publish_state(hex_data);
      }
      if (this->debug_sensor_bin_) {
        this->debug_sensor_bin_->publish_state(bin_data);
      }
    }

    //***********************************************************************************************
    // read_register : yet another read register function for debugging
    // TODO: add an enum with register sizes to simplify this function and all reading and writing
    //       functions.
    //***********************************************************************************************
    std::vector<uint8_t> CSE7761Component::read_register(int register_number, int size) {

      // PREPARATORY STEP: Clear the UART buffer (like in read_once_)
      while (this->available()) {
        this->read();
      }

      // STEP 1: SENDING THE READ COMMAND
      // Read command frame is 0xA5 + reg.
      uint8_t read_cmd[2] = {0xA5, static_cast<uint8_t>(register_number)};
      this->write_array(read_cmd, sizeof(read_cmd));

      // STEP 2: RAW READING
      std::vector<uint8_t> data;
      data.reserve(size + 1); // Reserve for data bytes + 1 CRC byte

      // The CSE7761 reads (size + 1) bytes: 'size' data bytes + 1 CRC byte
      for (int i = 0; i <= size; i++) {
        // Read immediately, without delay, because reading is blocking in the loop.
        int read_result = this->read();

        if (read_result == -1) {
          // Incomplete reading or empty buffer
          ESP_LOGW(TAG, "CSE7761: Incomplete reading. %zu bytes read out of %d requested for 0x%X.", data.size(), size + 1, register_number);
          return {}; // Returns an empty vector
        }

        data.push_back((uint8_t)read_result);
      }

      // If the reading went well, data.size() should be equal to (size + 1)
      if (data.empty() || data.size() != static_cast<size_t>(size + 1)) {
        ESP_LOGE(TAG, "CSE7761: Frame reading error. Size: %zu (expected %d)", data.size(), size + 1);
        return {};
      }

      // STEP 3: CRC VERIFICATION (similar to read_once_)
      // The last byte is the CRC. It is stored at data[size].
      uint8_t received_crc = data[size];
      uint8_t calculated_crc = 0xA5 + register_number;

      // CRC calculation on all bytes EXCEPT the last one (the received CRC byte)
      for (int i = 0; i < size; i++) {
        calculated_crc += data[i];
      }

      calculated_crc = ~calculated_crc; // Inversion of the result

      if (calculated_crc != received_crc) {
        ESP_LOGE(TAG, "CSE7761: CRC error for 0x%X. Calculated: 0x%02X, Received: 0x%02X",
                 register_number, calculated_crc, received_crc);
        return {}; // Returns an empty vector in case of failure
      }

      // STEP 4: RETURN USEFUL DATA
      // The CRC is valid. We remove the CRC byte from the vector before returning it.
      data.pop_back(); // Removes the last element (the CRC)

      ESP_LOGI(TAG, "Successful reading of register 0x%X. %u data bytes.", register_number, data.size());
      return data;
    }
    
    //***********************************************************************************************
    // write_register_service : home assistant service to write data to register
    // - std::string register_number_str
    // - std::string value_str
    // TODO: add an enum with register sizes to simplify this function and all reading and writing
    //       functions.
    //***********************************************************************************************
    void CSE7761Component::write_register_service(std::string register_number_str, std::string value_str) {
      ESP_LOGD(TAG, "Service called: Writing to register %s with value %s.", register_number_str.c_str(), value_str.c_str());

      uint8_t register_number;
      uint16_t value;

      // --- 1. Processing the register address ---
      char *end_ptr_reg;
      unsigned long reg_val = std::strtoul(register_number_str.c_str(), &end_ptr_reg, 0); // Base 0 allows auto-detection (0x for hex)
      if (end_ptr_reg == register_number_str.c_str() || *end_ptr_reg != '\0' || reg_val > 0xFF) {
        ESP_LOGE(TAG, "Error: Invalid register address or out of range (0-0xFF): '%s'", register_number_str.c_str());
        if (this->debug_sensor_hex_) {
          this->debug_sensor_hex_->publish_state("Error: Invalid register address.");
        }
        if (this->debug_sensor_bin_) {
          this->debug_sensor_bin_->publish_state("Error: Invalid register address.");
        }
        return;
      }
      register_number = (uint8_t)reg_val;

      // --- 2. Processing the value to write (16-bit) ---
      char *end_ptr_val;
      unsigned long val_to_write = std::strtoul(value_str.c_str(), &end_ptr_val, 0);
      if (end_ptr_val == value_str.c_str() || *end_ptr_val != '\0' || val_to_write > 0xFFFF) {
        ESP_LOGE(TAG, "Error: Invalid write value or out of range (0-0xFFFF): '%s'", value_str.c_str());
        if (this->debug_sensor_hex_) {
          this->debug_sensor_hex_->publish_state("Error: Invalid write value.");
        }
        if (this->debug_sensor_bin_) {
          this->debug_sensor_bin_->publish_state("Error: Invalid write value.");
        }
        return;
      }
      value = (uint16_t)val_to_write;

      // --- 3. Executing the write ---
      this->write_(CSE7761_SPECIAL_COMMAND, CSE7761_CMD_START_WRITE);
      
      // 0x80 is ORed in unconditionally: all register writes go through the write-enable path.
      this->write_(register_number | 0x80, value);
      
      // Disable writing
      this->write_(CSE7761_SPECIAL_COMMAND, CSE7761_CMD_FINISH_WRITE);

      // --- 4. Log and publish the result ---
      std::stringstream ss;
      ss << std::uppercase << std::hex << "OK: Wrote 0x" << std::setw(4) << std::setfill('0')
         << value << " to register 0x" << std::setw(2) << std::setfill('0')
         << (int)register_number;

      std::string result_msg = ss.str();
      ESP_LOGI(TAG, "%s", result_msg.c_str());
      if (this->debug_sensor_hex_) {
        this->debug_sensor_hex_->publish_state(result_msg);
      }
      if (this->debug_sensor_bin_) {
        this->debug_sensor_bin_->publish_state(result_msg);
      }
    }    

  }  // namespace cse7761
}  // namespace esphome
