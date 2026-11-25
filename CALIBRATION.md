# Calibration

The models use bench recordings, meter readings and patch observations from the author's rack. Manuals supplied nominal ranges, control functions and signal routing. Measurements represent individual units and settings. The engine interpolates between measured points and uses model assumptions where the bench data does not cover the full operating range.

| Module | Bench evidence used | Manual basis and model assumptions |
| --- | --- | --- |
| Susquehanna | Range steps and pitch offsets for two units, pitch tracking, output amplitude, and waveform shape near 224 Hz. A meter reading anchors the slow pulse swing. | Nominal ranges, auxiliary wave choices and sync controls follow documented operation. Waveform shape is extrapolated across pitches. |
| Schuylkill | Linear gain slope, mute region and ceiling, plus exponential gain from a stepped active CV sweep. | The documented summed CV and complementary paths set the routing. The gradual fade near the exponential measurement floor is estimated. |
| Lehigh | Decay and sustain landmarks from timed-gate recordings, plus release behavior from long envelope captures. | The active-low trigger and ADSR stages follow the manual. Attack taper and parts of the timing curves are estimates. |
| Allegheny | Cutoff vs. knob, driven cutoff and resonance sweep, self-oscillation onset near 0.665 of the resonance dial, and oscillation pitch across ranges. | The four-pole ladder, nonlinear feedback and interpolated tapers approximate the circuit and measured response. |
| Monongahela | Noise-through-filter recordings give cutoff at noon in both ranges. | Nominal response and controls follow the manual. The four-pole response and behavior away from measured settings are modeled. |
| Lackawanna | Noon low-pass and high-pass cutoffs and relative white/pink noise levels from recordings with matched gain. | The manual gives the single-pole filter ranges and noise types. Knob laws and noise generation are modeled. |
| Brandywine | The 1 kHz band level was measured at five knob positions. | Nominal band centers and endpoint branches come from the manual. The measured gain taper is applied to all branches, with modeled bandwidths. |
| Juniata | External advance from stop, pulse-clock interoperability, clock duty cycle and third-row timing across clock speeds and range settings. | Sequencing, stage modes and routing follow the manual. Absolute row voltage ranges and precise trigger thresholds include estimates. |
| Conestoga | Timed bank pulse lengths of roughly 45 ms to 4.2 s, equivalent upper and lower jacks, and observed audio-trigger behavior with noise. | Converter routing follows the manual. The audio detector is an approximation. |
| Tioga | Stage advance behavior with the third signal input unpatched was checked on the rack. | Direct select, trigger routing and output duplication follow the manual. Exact electrical thresholds are modeled. |
| Clarion | Meter readings support the linear master gain law with a fixed input and channel setting. | Four-channel summing, complementary outputs and multiples follow the manual. Individual channel gain curves are modeled. |
| Kiski | The first attenuator was metered into an unloaded path, with additional full-scale evidence from a filter sweep. | Cascading normals, multiples and bipolar references follow the manual. The measured attenuation curve is shared across channels. |
| Loyalsock | Rise/fall cycle sweeps, trigger threshold near 1.1 V, rest voltages, and end-of-rise/end-of-cycle levels. | Channel, slew and bus functions follow the documentation. Curvature, edge sensitivity and behavior outside the sampled points include estimates. |
| Swatara | Pitch readings in the first chord and chromatic scale settings, transpose span and output distribution at rest. | Scale/chord selection, hold and shift-register modes follow the information sheet. Some scale and chord intervals are inferred. |
