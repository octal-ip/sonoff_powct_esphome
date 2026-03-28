# Zero-Point Calibration

## Background

Both channels of the CSE7761 exhibit a constant measurement bias at idle (zero load). Channel A shows a significantly larger bias than Channel B in practice. Because Channel A's bias can exceed the range of the chip's hardware offset registers, correction is applied in software.

The calibration procedure measures the mean idle reading of each channel independently and adjusts stored software offsets so that each channel reads zero when no load is applied.

---

## How It Works

### Correction formula

For each measurement at runtime:

$$X_{\text{corrected}} = \frac{X_{\text{raw}}}{C} + \delta$$

where $C$ is the chip's conversion coefficient for that channel, and $\delta$ is the stored software offset. After a successful calibration, $\delta$ is set such that $X_{\text{corrected}} \approx 0$ at idle.

Four offsets are maintained and persisted independently:

| Offset | Channel | Quantity |
| :---: | :---: | :---: |
| `software_current_offset_A` | A | Current (A) |
| `software_power_offset_A` | A | Active power (W) |
| `software_current_offset_B` | B | Current (A) |
| `software_power_offset_B` | B | Active power (W) |

### Sampling

When calibration mode is enabled, the component accumulates 20 consecutive idle measurements per channel (one per `update_interval`, default 2 s — total approximately 40 s). Once 20 samples have been collected, the mean of each channel's readings is subtracted from its current stored offset:

$$\delta_{\text{new}} = \delta_{\text{old}} - \frac{1}{N}\sum_{i=1}^{N} X_{\text{corrected},i}$$

This is additive: running calibration multiple times converges the offsets rather than resetting them, which is useful when fine-tuning after an initial coarse calibration.

### Persistence

After each completed calibration cycle, all four offsets are saved to flash. They are automatically restored on reboot. The preference key is `0x2A3B4C5D`.

---

## Procedure

1. **Ensure no load is connected** to either channel — no conductor threaded through either CT input.
2. In Home Assistant, turn on the **Zero Point Calibration Mode** switch.
3. The switch turns itself off automatically after 20 samples (~40 s at the default 2 s update interval). The new offsets are logged and saved to flash.
4. Verify the result: current and power on both channels should read at or very close to zero.

> **Note:** If `ct_turns_b` is set to a value other than 1, the conductor must be wrapped the configured number of times during normal operation, but should be **fully removed** from the CT during calibration.

---

## Observed Bias Characteristics (Sonoff POWCT v2.x)

The following figures were measured on a specific unit and are provided as a reference. Values will differ between units and may drift with temperature.

| Quantity | Channel A bias | Channel B bias |
| :---: | :---: | :---: |
| Current | ~7.2 mA | ~0.7 mA |
| Power | ~0.91 W | ~−0.10 W |

The large ratio between Channel A and Channel B biases (~10× for current, ~−9× for power) is consistent across units. It is noted here as a sanity check: after a valid calibration, the stored offsets should reflect approximately this ratio.

---

## Limitations

- **Zero-point only:** Calibration corrects for a fixed idle bias. It does not correct for gain error (proportional error that grows with load). For higher accuracy across the full load range, hardware gain calibration registers (`GAINCAL_A`, `GAINCAL_B`) would need to be used, which is not currently implemented.
- **Temperature dependence:** The bias drifts slightly with ambient and chip temperature. Recalibrate if the environment changes significantly.
- **Requires true no-load:** Any residual current or power during calibration will be incorporated into the offsets as if it were bias, causing under-reading at that load level.
