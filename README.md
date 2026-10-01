# MFR-Mayo

[![Build](https://github.com/music-for-robots/MFR-Mayo/actions/workflows/build.yml/badge.svg)](https://github.com/music-for-robots/MFR-Mayo/actions/workflows/build.yml)

A free, open-source guitar effect by [Music For Robots](https://musicforrobots.com): a 90s-style op-amp fuzz into a cranked British amp, with a cheap microphonic pickup in the loop. **When you stop playing, it starts to whistle.**

## How the whistle works

Nothing triggers the whistle; it comes out of a simulated feedback loop. Sound from the virtual cab travels back through the air (a short delay) and shakes a loose, microphonic pickup coil (a resonator), which feeds it back into the fuzz.

While your strings ring, they saturate the fuzz, and the loop doesn't have enough gain left to take off. When you stop, the fuzz opens back up and the loop runs away at the pickup's resonant pitch. The fuzz then squares it into a whistle made mostly of odd harmonics. Distance and Drift make it jump between pitches and sag, the way real feedback does.

## Controls

| Section | Controls |
| --- | --- |
| **Gate** (first in the chain) | On/off, Threshold, Attack, Fade (up to 5 s). It keeps the rig quiet when you're not playing and sets how long the whistle rings after you stop. It listens to your guitar only, so the whistle can't hold it open. |
| **Fuzz** | Sustain, Tone (with the classic mid scoop at noon), Volume |
| **Amp** | Drive, Bass, Middle, Treble, Presence, plus an Amp + Cab bypass for using your own amp sim. The squeal still works when it's bypassed. |
| **Squeal** | Squeal (0 = off), Pitch, Distance, Drift, Bloom, and a pickup-ring meter |
| **Levels** | Input, Output, and an INIT button that resets everything |

Knobs: double-click or Alt-click resets a knob, Ctrl/Shift-drag gives fine control, and you can click a value to type one in.

> With the gate off, the plugin whistles whenever nothing is playing, including right after it loads. Mind your ears.

Formats: VST3 and Standalone (Windows 64-bit).

## Installing

Download the latest release from the [Releases](../../releases) page. Use the installer, or copy `MFR-Mayo.vst3` into `C:\Program Files\Common Files\VST3\` yourself, then rescan plugins in your DAW.

## Building from source

Requirements: CMake 3.22+ and Visual Studio 2022 with the C++ desktop workload. JUCE 8 is included as a git submodule.

```
git clone --recursive https://github.com/music-for-robots/MFR-Mayo.git
cd MFR-Mayo
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

The built plugin ends up in `build/Mayo_artefacts/Release/VST3/`.

There are two offline tools:
- `MayoTest` measures the whistle: whether it starts on its own, whether it's held off while you play and blooms in the stops, the Squeal and Pitch calibration, and the gate. It doesn't need JUCE.
- `MayoSnapshot` renders the UI to a PNG without opening an audio device.

To package it locally, run `scripts/package.ps1`. It needs [Inno Setup 6](https://jrsoftware.org/isinfo.php).

## Releasing

1. Bump `VERSION` in `project(...)` in `CMakeLists.txt`.
2. Add a section for the new version to `CHANGELOG.md`.
3. Commit, then tag and push, e.g. `git tag v1.0.1 && git push origin v1.0.1`.

GitHub Actions then builds the plugin, runs `MayoTest` and [pluginval](https://github.com/Tracktion/pluginval) (strictness 10), and publishes a release with the zip and the installer.

## Source layout

| Path | What's in it |
| --- | --- |
| `Source/MayoDSP.h` | Gate, pickup, fuzz, amp, cab and the microphonic feedback loop (plain C++) |
| `Source/PluginProcessor.*` | Parameters, 4× oversampling, state |
| `Source/PluginEditor.*` | User interface |
| `tools/` | `MayoTest` (offline DSP checks) and `MayoSnapshot` (UI render) |
| `installer/`, `scripts/package.ps1` | Installer script and packaging |
| `.github/workflows/build.yml` | CI build, validation and releases |

## License

MFR-Mayo is licensed under the [GNU AGPLv3](LICENSE) (or any later version). It is built with [JUCE](https://juce.com), which is used under the AGPLv3, and with the Steinberg VST3 SDK.

VST is a trademark of Steinberg Media Technologies GmbH. Big Muff is a trademark of Electro-Harmonix. Marshall and JCM800 are trademarks of Marshall Amplification. MFR-Mayo is an independent project and is not affiliated with or endorsed by any of them.
