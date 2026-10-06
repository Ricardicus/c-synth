# Keyboard synth

The Logic Pro Audio Unit lives in [logic-plugin/](logic-plugin/README.md).
That separate CMake project builds CSynth and includes the Logic installation guide.

A small C11 / SDL2 keyboard synth built with CMake. Play up to 8 independent
sound layers together, each with an FM chain of 1–8 operators:
OP1 → OP2 → … → OPn. Each operator modulates the next one's instantaneous
frequency; the final operator supplies that layer's audio output.
Waveforms and vibrato are configurable per operator. Defaults are one
layer, two sine operators, and vibrato off.

Install CMake, a C compiler, SDL2 and FreeType development files. On macOS with Homebrew:

```sh
brew install cmake sdl2 freetype
```

Build and run:

```sh
git submodule update --init --recursive
cmake -S . -B build
cmake --build build
./build/keyboard_synth
```

[libcsynth](https://github.com/Ricardicus/libcsynth) is a Git submodule in
`libcsynth/`. After cloning this repository, initialize it before configuring:

```sh
git submodule update --init --recursive
```

Or clone with `git clone --recurse-submodules <this-repository-url>`.
CMake builds the checked-out library before linking the app; it doesn't download
or update the dependency. Git records the exact library commit in this repository.
To update it later, use `git submodule update --remote libcsynth`, test the build,
and commit the updated submodule pointer.

The core is SDL-free. This project's `csynth::sdl` adapter handles device
playback. See the upstream [API doc](https://github.com/Ricardicus/libcsynth/blob/master/docs/libcsynth-api.md)
for notes, live edits, snapshots, and rendering samples yourself.

Focus the SDL window and hold keys to play. Multiple keys produce chords.

| Row | Keys (left to right) | Notes (left to right) |
| --- | --- | --- |
| Lower | z x c v b n m | C3 D3 E3 F3 G3 A3 B3 |
| Middle | a s d f g h j k l | C4 D4 E4 F4 G4 A4 B4 C5 D5 |
| Upper | q w e r t y u i o p | C5 D5 E5 F5 G5 A5 B5 C6 D6 E6 |

Overlapping keys (`k`/`q` and `l`/`w`) share a note; it continues sounding
until both keys are released.

Release a key to stop its note. Losing window focus releases all notes.
Escape or closing the window quits.

The right side of the window contains live sound controls. Drag a knob upward to increase its value or downward to decrease it; scroll
over a knob for small steps. Ratio, FM depth, index decay rate, and vibrato rate
use 0.01 steps; detune and vibrato depth use 0.1 cent steps. Decimal knobs drag
continuously and support fractional trackpad scrolling. Hold Shift while dragging
or scrolling for finer adjustment. Attack, decay, and release use 25 ms steps
normally and 1 ms steps with Shift, for both operator and master envelopes.
Other decimal controls use ten times finer adjustment with Shift; sustain uses
1 percent steps.
Discrete choices (layer/operator selection, counts, waveform and envelope mode)
use **-** / **+** selectors. Select a layer and operator to edit their
settings; the layer and operator count controls add or remove parts of the
sound. Changes affect held notes as well as future notes without reopening
the audio device. Waveform and index-envelope controls cycle through their
available modes. The final operator is the output carrier: its FM depth and
index envelope are unused. Operator envelopes control FM timbre. The four **Master Output ADSR** knobs
below the spectrogram control the combined layers' volume for each note: attack
and decay in milliseconds, sustain in percent, and release in milliseconds.
The top row is **Preset → Effects → MIDI player**, above the spectrogram heading.
Open **Effects** or **MIDI player**, then choose **FM equations** or **Output ADSR** for dedicated
views with large, smooth text and raw functions. Equations substitute the
configured waveform (`sin`, `triangle`, `pulse`, etc.) and numeric settings
formatted to two decimal places. Frequencies follow the most recently played
note (440 Hz before playing); long chains can be scrolled with the mouse wheel. **Back to sound** or Escape returns to the controls; keyboard notes
still play while viewing equations. The window supports HiDPI rendering and
has a minimum size to keep the text readable. Fonts use FreeType and a system
monospace font (Menlo, DejaVu Sans Mono, Liberation Mono, or Consolas); set
`SYNTH_FONT` to a TTF/TTC path to choose another font.
Master edits affect sounding notes; each note has its own envelope so chords
retain independent articulation. Presets include their output envelope settings.

Open the preset dropdown above the spectrogram, scroll through the list, and
click one entry to replace the complete sound. Up/Down, Home/End and Enter also
select an entry while the list is open; Escape or an outside click dismisses it.
The **<** and **>** buttons beside the dropdown select the previous or next
preset, including saved settings, and wrap around at the ends of the list.
The factory bank contains **64 presets**. The original 13 sounds are followed by
families of flutes, reeds/brass, keys, bells, basses, leads, pads, plucks, and effects.
Flutes include Concert, Alto, Bass, Piccolo, Bamboo, Pan, and Dream variations,
with different FM brightness, breath levels, vibrato, articulation, and ambience.
Try Keys Tine EP, Bell Singing Bowl, Bass Rubber FM, Lead Liquid, Pad Aurora,
Pluck Echo Harp, or FX Cosmic Transmission. Alto/Bass/Piccolo flutes and several
basses deliberately transpose the played note; Pad Fifth Horizon layers a fifth.
See [the factory sound guide](https://github.com/Ricardicus/libcsynth/blob/master/presets/README.md) for the complete bank and playing tips.
Editing a sound changes its label to **Custom**. Startup command-line settings
populate the panel. Click **Save setting** beneath the preset dropdown, enter a name, and click
Save or press Enter. Saved sounds are appended to the dropdown and reappear after
restarting. Names can contain up to 32 letters/numbers, spaces, hyphens and
underscores. Duplicate names are rejected, so existing presets are preserved.
Select a saved preset and click **Delete setting** to remove its file; deleting
keeps the current sound loaded as Custom. Factory presets cannot be deleted.

The dedicated `presets/` folder contains `factory/*.synth` (the shipped sounds)
and `user/*.synth` (your saved sounds). Files use a versioned, readable text format
and store all layers/operators, including inactive settings, master ADSR, and
effects at full floating-point precision. Invalid user files are skipped at startup.
CMake copies the submodule's factory sounds into this folder; saved sounds stay
outside the submodule checkout. Set the CMake `SYNTH_PRESET_DIR` cache variable
to choose a different default folder. The executable uses that folder regardless of its working
directory; set the `SYNTH_PRESET_DIR` environment variable to override it at runtime.

The main view includes **Low-pass Hz** and **High-pass Hz** knobs above the
master ADSR. Turn up from **Off** to enable a filter; turn below 20 Hz to bypass
it. Cutoffs cover 20–20000 Hz on a logarithmic dial. Drag or scroll to tune them,
with Shift for finer changes. The filters affect all notes before echo/reverb,
so the spectrogram shows the filtered sound. They use 12 dB/octave slopes and
smooth live changes over about 20 ms. Saved settings include both cutoffs;
older setting files load with filters off.

Open **Effects** in the top row to adjust echo mix, delay (1–2000 ms),
and feedback, plus reverb mix, room size, and damping. Effects apply after all notes
and their output envelopes are mixed, so echoes and reverb continue after note-off.
Echo feeds into reverb. Mix 0 bypasses the wet signal; effects default to off except
for the Flute preset's light reverb. Settings follow preset selection and can be
changed while playing; effect parameters glide over about 20 ms.

Startup options: `--echo-mix`, `--echo-delay`, `--echo-feedback`, `--reverb-mix`,
`--reverb-room`, and `--reverb-damping`. Mix/damping values are 0–1;
feedback/room values are 0–0.95. For example:

```sh
./build/keyboard_synth --echo-mix 0.3 --echo-delay 280 --echo-feedback 0.35 --reverb-mix 0.15
```

**Noise** is a white-noise waveform with an independent random generator for each
voice/layer/operator. Use it as a carrier for noise or as a modulator for a noisy
FM tone. Its output is independent of oscillator pitch and phase.

Click **MIDI player** in the top row to open the MIDI section. Use
**Browse MIDI file** to navigate folders and select a `.mid` or `.midi` file.
**Play** starts or resumes, **Stop** pauses and releases MIDI-held notes, and
**Restart** plays from the beginning. A time counter and progress bar show playback.
You can return to the sound controls and change the patch while the file plays.

MIDI formats 0 and 1 are supported, including tempo changes, running status,
sustain pedal, velocity, and SMPTE timing. All channels use the current synth
sound; General MIDI instrument/drum mappings and live MIDI input are not included.
Events are dispatched by the main loop (roughly every 16 ms). Manual keyboard
notes and MIDI notes have separate ownership, so stopping one leaves the other
sounding. Master release and effect tails continue after Stop.

The window shows a live spectrogram of the combined audio output.
Frequency runs upward on a logarithmic scale from 40 Hz to 20 kHz (or
Nyquist, if lower); time scrolls from left to right with the newest audio
at the right edge. Brighter colors mean stronger components, using the
-80 to 0 dBFS color scale. The window is resizable.

The display uses a 2048-sample Hann-windowed FFT with amplitude normalization.
At 48 kHz, frequency bins are about 23.4 Hz apart. It refreshes when at least
1024 new samples are available; the time axis shows ordering rather than
a calibrated duration. The audio callback captures samples into a fixed
ring buffer. FFT analysis and SDL rendering run on the main thread.

## FM controls

The played note provides the base frequency. Each operator's nominal
frequency follows the base at its own ratio. For each link OPn → OPn+1:

```text
layer base frequency = played note frequency * 2^(layer detune cents / 1200)
OPn nominal frequency = layer base frequency * OPn ratio
OPn peak deviation = OPn rm * OPn indexEnvelope * OPn nominal frequency
OPn+1 instantaneous frequency = OPn+1 nominal frequency + OPn peak deviation * OPn output
```

Each operator integrates its signed instantaneous frequency into its phase
each sample. Its selected waveform output is used by the next operator in that same sample.
`rm` is the dimensionless depth relative to the modulator's nominal frequency. For example,
base 440 Hz, ratio 2, and rm 3 give OP1 = 880 Hz and peak deviation = 2640 Hz
at full envelope in a two-operator chain. `--rm 0` produces the final
carrier's waveform without incoming modulation. `--ops 1` plays a single
oscillator without FM.
This uses the index/deviation relationship described in
[Stanford's sound synthesis notes](https://theory.stanford.edu/~blynn/sound/synth.html#_frequency_modulation).

| Option | Meaning | Default |
| --- | --- | --- |
| `--ops N` | Number of operators in layer 1, 1–8 | 2 |
| `--rm NUMBER` | Set nonnegative depth for all layer 1 modulators | 2 |
| `--opN-ratio NUMBER` | OP N nominal frequency divided by base frequency, positive | 1 |
| `--opN-waveform NAME` | sine, square, triangle, sawtooth (alias: saw), pulse, noise | sine |
| `--opN-pulse-width NUMBER` | Pulse high fraction, strictly between 0 and 1 | 0.25 |
| `--opN-vibrato-rate NUMBER` | Vibrato rate in Hz, nonnegative | 5 |
| `--opN-vibrato-depth NUMBER` | Vibrato depth in cents, 0–1200; 0 switches it off | 0 |
| `--opN-rm NUMBER` | Nonnegative depth of OP N into OP N+1 | 2 |
| `--opN-i sustain` | Constant full depth while held | Default mode |
| `--opN-i default` | Alias for `sustain` | |
| `--opN-i decay` | Depth decays exponentially from full depth on note-on | |
| `--opN-i-decay NUMBER` | Decay rate per second; envelope = exp(-rate * seconds) | 2 |
| `--opN-i adsr` | Linear attack, decay to sustain, release on note-off | |
| `--opN-i-adsr A,D,S,R` | Integer attack ms, decay ms, sustain percent, release ms | 10,200,50,300 |
| `--help` | Show options without opening audio or a window | |

Replace `N` in `--opN-*` with the operator number, e.g. `--op2-rm 3`.
Ratio, waveform, pulse width, and vibrato apply to the final carrier; depth/envelope flags
for it are rejected because it has no outgoing modulation link. References
to operators outside the selected chain are also rejected. `--ops` may
appear anywhere. Depth options apply in argument order: use `--rm` first,
then per-operator overrides. Existing two-operator commands still work.

ADSR times may be zero; sustain must be 0–100. A larger exponential decay
rate fades depth faster; rate 0 holds it constant. For example, rate 2
leaves about 13.5% depth after one second. Envelope parameter flags configure
their respective modes; select each mode explicitly with `--opN-i`.
Every operator in every note has independent phase and envelope timing.

Square is a 50% duty wave; pulse uses the configured duty cycle. Triangle
starts at zero and rises, while sawtooth rises from -1 to +1 before wrapping.
These are direct phase-based waveforms without band limiting. Square,
sawtooth, and pulse can alias at high frequencies. A pulse with unequal
duty cycle contains a DC component, which can shift the next operator's
average frequency when used as a modulator.

Vibrato applies a sine pitch variation to each oscillator's instantaneous
frequency. Depth is the peak deviation in cents: 100 cents is one semitone.
Rate 0 freezes the vibrato phase; on a fresh note its sine starts at zero.
Changing settings through the C oscillator setters preserves phase.

```sh
# Constant FM depth, modulator twice the played frequency
./build/keyboard_synth --op1-ratio 2 --rm 3

# Bright attack that decays toward a plain sine
./build/keyboard_synth --op1-ratio 2 --rm 5 --op1-i decay --op1-i-decay 3

# Index ADSR: 20 ms attack, 300 ms decay, 35% sustain, 500 ms release
./build/keyboard_synth --rm 4 --op1-i adsr --op1-i-adsr 20,300,35,500

# OP1 -> OP2 -> OP3, with different depths, ratios, and envelopes
./build/keyboard_synth --ops 3 \
  --op1-ratio 3 --op1-rm 2 --op1-i decay --op1-i-decay 3 \
  --op2-ratio 2 --op2-rm 4 --op2-i adsr --op2-i-adsr 20,300,35,500 \
  --op3-ratio 1

# Square modulator, triangle carrier, carrier vibrato at 5 Hz +/- 15 cents
./build/keyboard_synth --op1-waveform square --op1-rm 0.8 \
  --op2-waveform triangle --op2-vibrato-rate 5 --op2-vibrato-depth 15

# Standalone pulse oscillator
./build/keyboard_synth --ops 1 --op1-waveform pulse --op1-pulse-width 0.2
```

Modulators' envelopes change timbre. The master output envelope controls volume
after the layers are mixed for each note. Its default ADSR is `5,0,100,5`;
configure startup values with `--master-adsr A,D,S,R`, for example
`--master-adsr 100,400,65,800`. Times are nonnegative milliseconds and sustain
is 0–100 percent. Zero durations skip their stages. The master release determines
how long audio remains audible, independently of the FM index release.
Losing focus triggers the same note-off behavior. Higher pitches, ratios, or depths can alias;
this initial generator renders at the device sample rate without oversampling.

## Independent sound layers

| Option | Meaning | Default |
| --- | --- | --- |
| `--layers N` | Simultaneous independent sound layers, 1–8 | 1 |
| `--oscillators N` | Alias for `--layers` | |
| `--layerL-ops N` | Number of FM operators in layer L, 1–8 | 2 |
| `--layerL-rm NUMBER` | Set all modulator depths in layer L | 2 |
| `--layerL-gain NUMBER` | Layer's mix gain, 0–1 | 1 |
| `--layerL-detune NUMBER` | Pitch offset for the whole layer, -4800 to 4800 cents | 0 |
| `--layerL-opN-*` | Any operator setting above, for OP N in layer L | |

Use actual numbers in place of L and N. Unprefixed `--ops`, `--rm`, and
`--opN-*` flags address layer 1, preserving existing commands. Counts may
appear anywhere; references are checked against the final layer/operator
counts. Each key starts all layers together. Layers have independent FM
phases, vibrato, index envelopes, and amplitude release tails.

Layer outputs are summed with gain divided by layer count, keeping the
nominal per-note volume comparable as layers are added. Gain 0 mutes a
layer. Chords share a final output clamp at [-1, 1].

```sh
# Two different FM chains, with detuning and separate carrier vibrato
./build/keyboard_synth --layers 2 \
  --ops 2 --op1-ratio 2 --op1-rm 1.2 --op2-vibrato-depth 12 \
  --layer2-ops 3 --layer2-detune 7 --layer2-gain 0.7 \
  --layer2-op1-ratio 3 --layer2-op1-rm 0.5 \
  --layer2-op2-ratio 2 --layer2-op2-rm 1 \
  --layer2-op3-waveform triangle --layer2-op3-vibrato-rate 4.8 \
  --layer2-op3-vibrato-depth 9
```

More starting points are in [suggestions.txt](suggestions.txt).

## Playing recordings as an instrument

Open **Samples** in the SDL window. Click **Add sound file**, browse to a WAV
or MP3, then click the **Base Hz** field and enter the recording's pitch.
Ctrl+A (or Cmd+A on Mac) replaces the field; Enter finishes editing.
Add more recordings the same way, or just use one. **Apply files** loads the
bank and switches the keyboard and MIDI player to sample playback. Edits and
removals affect the draft until you apply it. A failed load leaves the current
bank playing.

For your piano recordings, use A3 = **220.00 Hz**, A4 = **440.00 Hz**, and
A5 = **880.00 Hz**. The ready-made [piano map](libcsynth/media/piano.csamples)
contains those three files; select it with **Load sample map**. Maps load
directly as the active bank, separately from the editable replacement draft.
Relative recording paths are resolved from the map's folder.

**FM source** and **Sample source** switch between engines without reloading
the bank. Selecting a sound preset switches back to FM; you can return to the
sample bank with **Sample source**. The nearest recorded pitch is chosen for
each note and transposed to match what you play.

The **master output ADSR**, **lowpass/highpass filters**, **echo**, and
**reverb** all work in sample mode and can be changed while playing. Layer
count, gain and detune also apply; FM operator controls only shape the FM
source. Files added through the panel play once, so ADSR cannot extend a
recording beyond its end. Maps can define loops for sustained sounds; see
[libcsynth's sample documentation](libcsynth/README.md).
Saved `.synth` settings store the processing configuration; keep the sample
map and recordings separately.

## C API

The reusable core comes from [Ricardicus/libcsynth](https://github.com/Ricardicus/libcsynth).
CMake uses `add_subdirectory(libcsynth)` to build the submodule and provide
the `csynth::csynth` target. The core is maintained in its own repository.
Start with `synthCreate()`, send notes with `synthNoteOn()` / `synthNoteOff()`,
and call `synthRender()` to fill a mono float buffer. `synthConfigure()` applies
settings to held and future notes. See the upstream
[API doc](https://github.com/Ricardicus/libcsynth/blob/master/docs/libcsynth-api.md).

The app's `main.c` handles keyboard input. `synth_sdl.h` / `synth_sdl.c` adapt the
core to SDL device playback, requesting mono float audio at 48 kHz with
256-sample buffers. The adapter uses the device's obtained sample rate and locks
the audio callback around main-thread note and settings updates.

## Checks

```sh
ctest --test-dir build --output-on-failure
```

Tests cover chains of 1–8 operators, waveform shapes and signed phase,
vibrato timing, independent envelopes and layers, decay/ADSR retriggering,
command-line defaults and validation, mixed audio snapshots and note release,
and FFT frequency/amplitude scaling. An application regression test injects
SDL backend touch warnings to verify that keyboard input, rendering, and
quit still work. Audio and application integration tests use SDL's dummy drivers.
