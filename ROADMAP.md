# LapCat2A roadmap

## Done in v0.1.0 (test build)
- Native JUCE editor ported from the approved mockup (layout generated from the mockup DOM), three looks, wood cheeks, VU meter, transfer curve, history, sidechain filter, dB meters, info bubbles, no scrollbars.
- C++ core (`src/core/LapCat2A.h`) with its own tests (`sh tests/run.sh`), identical to the mockup's model in a parity test.
- 4x oversampled tube stage, exact 30 sample latency, bit-exact bypass, 10 ms parameter smoothing, NaN and denormal guards.
- Presets: 20 factory, user presets in `Studio 87/LapCat2A/presets`, save, rename, delete, export, import.
- Installers: Windows .exe with uninstaller, macOS .pkg with an Uninstall app, Linux Flatpak extension and .vst3 tarball; PDF manual.

## Measured (on the C++ core, see the manual)
Level calibration, attack and release, knob to sound time, aliasing at one hard setting (-99.6 dB with 4x), harmonics, DC, stability, parity with the mockup model.

## Not verified yet
- Loading in a real DAW on any OS, and pluginval beyond what CI reports.
- The Windows installer, the macOS package and its uninstaller, and the Flatpak extension on real machines (they build in CI only).
- Window resizing and HiDPI in real hosts, Flatpak hosts reading the presets folder.
- Aliasing at every extreme knob combination (some extreme tones measured around -25 to -77 dB before the 4x stage was tuned).
- Sound against a real LA-2A, and tuning by ear.
- No code signing or notarization (cost); installers show OS warnings.

## Next
- Test in Bitwig, Reaper, Ardour and Studio One; fix host issues.
- Listen and tune the cell, tube stage and the supersaw presets by ear.
- A stronger model or a real unit comparison would help tune the release and cell curves.
- Optional stereo link or mid/side sidechain; an HQ switch for 8x oversampling if aliasing at extremes matters.
