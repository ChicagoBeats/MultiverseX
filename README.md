# MultiverseX (Hazel Bandz / H.G. Fortune)
Hybrid synth + multi-sampler VST3. Built with JUCE 7.0.12 via GitHub Actions (x64 and Win32).
1. Push this folder to a GitHub repo. 2. Actions tab > "Build VST3" > download the artifact.
3. Copy `MultiverseX.vst3` to `C:\Program Files\Common Files\VST3`.
Phase 1: WAV sampler. Phase 2: 8-slot modulation matrix (sources: LFO1/2, ENV1, Velocity, Aftertouch, Mod Wheel, Breath, Random; targets: Cutoff, Pitch, Amp, Reso, Drive, Pan, FX Mix, Reverb, Delay). Visualizer reacts to the matrix. Next: 12+ effects, Click=Randomize/Right-Click=Lock, neon skin.

## Effects rack (13) - On / Mix / Rate
Chain: Filter FX > Ring Mod > Bit Crusher > Distortion > Pitch Shifter > Trance Gate > Chorus > Flanger > Phaser > EQ Tilt > Stereo Enhancer > Delay > Reverb > Compressor > Limiter
Rate means: Filter=cutoff, Ring=carrier freq, Crusher=bits+rate reduction, Dist=drive, Pitch=-12..+12 st, Gate=step speed (Mix=depth),
Chorus/Flanger/Phaser=LFO speed, EQ=tilt (Mix=amount), Stereo=side level, Comp=threshold, Limiter=ceiling (Mix=input drive).
Not yet: Vocoder, Sonar and other items in the mockup list.

## Randomizer
Sections: Synth, Matrix, FX, Sampler. Click a section button = randomize it. Right-click = lock it. Full Mix randomizes every unlocked section.
Amount = how far toward the new random values (100% = full jump). Undo/Redo keep the last 50 randomizations. The Limiter and Master are never randomized.

## Look
The whole UI accent colour follows the selected engine, like the colour variants in the mockups. Logo and crowns are drawn in code - to use your own artwork, add a PNG via juce_add_binary_data and draw it in `MultiverseXEditor::paint`.
