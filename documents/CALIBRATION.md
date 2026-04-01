# Calibration

Two independent calibration mechanisms are available:

| Mechanism | What it corrects | How it is set |
| :--- | :--- | :--- |
| **Zero-point offset** | Fixed idle bias (additive, constant regardless of load) | Home Assistant switch — saved to flash |
| **Gain correction** | Proportional error that scales with load (CT ratio, burden resistor tolerance, etc.) | YAML configuration — compiled into firmware |

Both apply to Channel A current and active power. Channel B has zero-point correction only.

---

## Part 1 — Zero-Point Offset Calibration

### Background

Both channels exhibit a constant measurement bias at idle (zero load). Channel A shows a significantly larger bias than Channel B in practice. Because Channel A's bias can exceed the range of the chip's hardware offset registers, correction is applied in software.

The calibration procedure measures the mean idle reading of each channel independently and adjusts stored software offsets so that each channel reads zero when no load is applied.

### Correction formula

At runtime the full Channel A correction pipeline is:

$$X_{\text{out}} = \frac{X_{\text{raw}}}{C} \times G + \delta$$

where:
- $C$ is the chip's factory conversion coefficient for that channel
- $G$ is the gain multiplier (`current_gain_a`, default `1.0`)
- $\delta$ is the stored software zero-point offset

Gain is applied first (scaling the proportional signal), then the additive offset is applied on top. Channel B uses the same formula with $G = 1.0$ (no gain correction).

Four offsets are maintained and persisted independently:

| Offset | Channel | Quantity |
| :---: | :---: | :---: |
| `software_current_offset_A` | A | Current (A) |
| `software_power_offset_A` | A | Active power (W) |
| `software_current_offset_B` | B | Current (A) |
| `software_power_offset_B` | B | Active power (W) |

### Sampling

When calibration mode is enabled, the component accumulates 20 consecutive idle measurements per channel (one per `update_interval`, default 2 s — total approximately 40 s). Once 20 samples have been collected, the mean of each channel's readings is subtracted from its current stored offset:

$$\delta_{\text{new}} = \delta_{\text{old}} - \frac{1}{N}\sum_{i=1}^{N} X_{\text{out},i}$$

This is additive: running calibration multiple times converges the offsets rather than resetting them, which is useful when fine-tuning after an initial coarse calibration.

### Persistence

After each completed calibration cycle, all four offsets are saved to flash. They are automatically restored on reboot. The preference key is `0x2A3B4C5D`.

### Procedure

1. **Ensure no load is connected** to either channel — no conductor threaded through either CT input.
2. In Home Assistant, turn on the **Zero Point Calibration Mode** switch.
3. The switch turns itself off automatically after 20 samples (~40 s at the default 2 s update interval). The new offsets are logged and saved to flash.
4. Verify the result: current and power on both channels should read at or very close to zero.

> **Note:** If `ct_turns_b` is set to a value other than 1, the conductor must be wrapped the configured number of times during normal operation, but should be **fully removed** from the CT during calibration.

### Observed Bias Characteristics (Sonoff POWCT v2.x)

The following figures were measured on a specific unit and are provided as a reference. Values will differ between units and may drift with temperature.

| Quantity | Channel A bias | Channel B bias |
| :---: | :---: | :---: |
| Current | ~7.2 mA | ~0.7 mA |
| Power | ~0.91 W | ~−0.10 W |

The large ratio between Channel A and Channel B biases (~10× for current, ~−9× for power) is consistent across units. It is noted here as a sanity check: after a valid calibration, the stored offsets should reflect approximately this ratio.

---

## Part 2 — Gain Correction (Channel A)

### Background

Zero-point calibration removes the constant idle bias, but leaves any proportional error untouched. A gain error causes readings to be off by a fixed percentage across the entire load range — for example, consistently reading 4.8% high at every load level. Common sources include CT ratio tolerance, burden resistor tolerance, and wiring attenuation.

The `current_gain_a` YAML option multiplies the raw Channel A current and active power readings by a constant factor before the zero-point offset is applied. Because power is derived from the same current ADC path, a single gain value corrects both quantities simultaneously. Energy accumulation (received and exported) is also corrected automatically, since it integrates the corrected power value.

### How to determine the gain value

1. **Perform zero-point calibration first** (Part 1 above) so that idle bias is already removed.
2. Apply a **known, stable load** to Channel A — ideally a resistive load (heater, incandescent lamp) at a known wattage, or use a calibrated reference meter in series.
3. Read the uncorrected Channel A current or power from Home Assistant.
4. Calculate the gain:

$$G = \frac{X_{\text{reference}}}{X_{\text{measured}}}$$

5. Set `current_gain_a` in `sonoff_powct.yaml` (under `substitutions`) and recompile/upload.

**Example:** reference meter reads 10.48 A, device reports 10.00 A → $G = 10.48 / 10.00 = 1.048$.

### Configuration

In `sonoff_powct.yaml`:

```yaml
substitutions:
  current_gain_a: "1.048"  # adjust to your measured value; 1.0 = no correction
```

The default value is `1.0` (no correction). Values above `1.0` scale readings up; values below `1.0` scale them down.

> **Note:** `current_gain_a` is a compile-time constant. Changing it requires a firmware recompile and upload. It is intentionally separate from the runtime zero-point calibration because it represents a fixed installation characteristic that rarely needs adjustment.

---

## Limitations
- **Temperature dependence:** The zero-point bias drifts slightly with ambient and chip temperature. Recalibrate if the environment changes significantly.
- **Zero-point calibration requires true no-load:** Any residual current or power during calibration will be incorporated into the offsets as if it were bias, causing under-reading at that load level.
- **Gain calibration requires a stable and known reference:** An unstable or reactive load will produce an inaccurate gain estimate.
