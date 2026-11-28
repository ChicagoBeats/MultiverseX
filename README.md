# MultiverseX v0.3 - Synth + Sampler + Multi-FX + Endless Skins

Two plugins from one codebase (GitHub Actions builds both):
* **MultiverseX.vst3**   - instrument: synth + WAV sampler + FX rack
* **MultiverseXFX.vst3** - effect: put any audio through the FX rack (Mixer insert)

## Features
* **Blend everywhere**: Engine A<>B blend, Synth<>Sampler blend, FX Wet/Dry, per-slot Mix, A/B Morph of the whole rack
* **Amp ADSR + Filter ADSR** with draggable envelope graphs, filter-envelope amount
* **8-slot FX rack**, each slot any of: Drive, Bitcrusher, Auto Filter, Chorus, Flanger, Phaser, Delay (ping-pong), Reverb,
  Tremolo, Auto Pan, Compressor, Stutter Gate (tempo-synced), Ring Mod, Tape, Limiter
* **Skin / VFX**: 4 stackable layers, every colour editable (+ rainbow mode), speed / size / density / opacity / audio-react
  * Matrix Code (any colour), Fire, Pyro Fireworks (also fire when you play notes), Lightning/Thunder, Water Ripples,
    Tropical Island (moving palms + waves), Wisdom Trees (pine, cedar, oak, ginkgo, fig, beech, willow, forest mode),
    Neon Notes (Keysight style), Chicago Studio orbit camera (editable sign), Dub-style Concept Car
    (huge rims, spinners/floaters, underglow, subs, giant dash TV), Galaxy, Aurora, Starfield, Gradient
  * Image layers: PNG, JPG, BMP, **animated GIF**, SVG (drop on the window or Browse)
  * Video on the stage strip (MP4/AVI/WMV/MOV via DirectShow)
  * 23 factory skins, Randomize Skin, Save/Load `.mvxskin`, "Set as Default"
* **Resize** from the bottom-right corner to ANY size (UI scales to fit, the skin fills the whole window)
* Everything is remembered in `Documents\Hazel Bandz\MultiverseX\` (Media, Skins, default skin) and inside the project

## Build
Push to GitHub -> Actions -> download artifacts. Local: `cmake -B build -A x64 && cmake --build build --config Release`.
If the build fails inside `juce_video`, set `option(MVX_VIDEO ... OFF)` in CMakeLists.txt.

## Mix & levels (v0.3.1)
* **Synth<>Sample** blend knob, **Synth Vol** (-100..0 dB), **Sample Vol** (-100..+21.6 dB), **Sample>FX** send
  (0 = sample stays dry and bypasses the FX rack so it stands out; raise it to run some of the sample through the FX).
* Randomizers never leave -5..0 dB for volumes and keep make-up/limiter gains tame. Toggle "Rand levels" off to make
  the randomizer leave your volumes alone. Only you can push levels beyond 0 dB.
* Video is OFF by default (MVX_VIDEO in CMakeLists.txt); switch it ON when you want to try it.
