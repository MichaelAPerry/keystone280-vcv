# Keystone 280

Keystone 280 is a collection of 14 software modules for VCV Rack 2. Michael Perry built them from the modules in his own Behringer System 15 rack, which includes Moog 900-series-derived designs, and calibrated them against bench recordings and measurements of that rack. The result is close, not identical. See [Calibration](CALIBRATION.md) for the measurements and assumptions behind each module.

## Modules

- **Susquehanna Oscillator** — Pitch-controlled oscillator with six ranges, sub-audio operation and clamp sync.
- **Schuylkill VCA** — Linear or exponential amplifier with summed control inputs and complementary signal paths.
- **Lehigh Envelope** — Attack, decay, sustain and release envelope with active-low triggering.
- **Allegheny Low-Pass** — Four-pole low-pass filter with three cutoff ranges and self-oscillating resonance.
- **Monongahela High-Pass** — Four-pole high-pass filter with two cutoff ranges and summed cutoff modulation.
- **Lackawanna Filters & Noise** — Independent low-pass and high-pass filters plus white and pink noise.
- **Brandywine Filter Bank** — Twelve fixed band-pass bands with independent levels and low-pass and high-pass branches.
- **Juniata Sequencer** — Three rows of eight stages with an internal clock, stage modes and optional third-row timing.
- **Conestoga Interface** — Converts audio and voltage triggers to patchable trigger signals, with two timed active-low banks.
- **Tioga Sequential Switch** — Two- or three-stage signal switch with advance, direct stage selection and stage gates.
- **Clarion Mixer** — Four-channel mixer with master gain, complementary outputs and two signal multiples.
- **Kiski Attenuators** — Four cascaded attenuators, two signal multiples and bipolar reference voltages.
- **Loyalsock Function Generator** — Two rise/fall generators and four attenuverters with sum, inversion and maximum outputs.
- **Swatara Quantizer** — Scale and chord quantizer with three outputs, a shift register and a hold loop.

## Build

Install GNU Make, a C++17 compiler, `jq` and `zstd`. Download the [Rack 2 SDK](https://vcvrack.com/downloads/) for your target platform and architecture. On macOS, use the Xcode Command Line Tools.

```sh
make -j4 RACK_DIR=/path/to/Rack-SDK
make dist RACK_DIR=/path/to/Rack-SDK
```

The package is written to `dist/`. For Windows cross-compilation, use a Windows x64 Rack SDK and pass the MinGW compiler prefix, including the final dash, as `CROSS`.

```sh
make dist RACK_DIR=/path/to/windows/Rack-SDK CROSS=/path/to/bin/x86_64-w64-mingw32-
make -C tests
```

Clean the build before switching target platforms. The tests use native `g++` and can run without a Rack SDK.

## Panels

The YAML files in `layout/` define the panel artwork and widget positions. The generated SVG files use outlined glyphs. The Rack build uses the checked-in SVGs and headers directly.

```sh
python3 -m pip install fonttools brotli pyyaml
python3 tools/panel.py
python3 tools/check_panels.py
```

## Credits and license

DSP, bench calibration and public panel designs by Michael Perry. Rack integration uses the VCV Rack SDK and its standard widgets. The panels use [Bricolage Grotesque](https://github.com/ateliertriay/bricolage) under the [SIL Open Font License 1.1](tools/fonts/OFL.txt).

Keystone 280 source is licensed under [GPL-3.0-or-later](LICENSE).
