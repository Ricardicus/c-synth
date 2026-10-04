# Keyboard synth

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
cmake -S . -B build
cmake --build build
./build/keyboard_synth
```

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
or scrolling for ten times finer adjustment. Envelope times use 1 ms steps and
sustain uses 1 percent steps.
Discrete choices (layer/operator selection, counts, waveform and envelope mode)
use **-** / **+** selectors. Select a layer and operator to edit their
settings; the layer and operator count controls add or remove parts of the
sound. Changes affect held notes as well as future notes without reopening
the audio device. Waveform and index-envelope controls cycle through their
available modes. The final operator is the output carrier: its FM depth and
index envelope are unused. Operator envelopes control FM timbre. The four **Master Output ADSR** knobs
below the spectrogram control the combined layers' volume for each note: attack
and decay in milliseconds, sustain in percent, and release in milliseconds.
Open **FM equations** or **Output ADSR** beside the preset dropdown for dedicated
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

Open the preset dropdown below the spectrogram, scroll through the list, and
click one entry to replace the complete sound. Up/Down, Home/End and Enter also
select an entry while the list is open; Escape or an outside click dismisses it.
Available presets:
Classic FM, Pure sine, Warm triangle, Saw lead, Pulse bass, Electric piano,
Glass bell, Metal chime, Soft organ, Wide pad, Brass, Space wobble, and Flute.
Flute uses a soft sine FM tone, quiet breath noise, gentle vibrato, and light reverb.
Editing a sound changes its label to **Custom**. Startup command-line settings
populate the panel; edits and preset selections last for the current session.

Open **Effects** next to the master ADSR controls to adjust echo mix, delay (1–2000 ms),
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

## C API

`main.c` handles keyboard events. `synth.h` exposes `synthInit()`,
`synthInitWithConfig(const FmConfig *)`, `synthInitWithLayers(const SynthConfig *)`,
`synthShutdown()`, `registerNote(int)`,
and `deregisterNote(int)`. `synthInit()` uses default FM settings.
Note integers are MIDI numbers (0–127); `synth.c` builds an internal
frequency lookup with A4 = 440 Hz. Invalid numbers are ignored.

SDL2 renders mono floating-point audio through a callback at 48 kHz (or
the device's supported sample rate), requesting 256-sample buffers.
Each active note has an independent FM generator per layer. The callback does no allocation
or I/O. Note updates use SDL's
audio-device lock. Call the public API from the main thread.

`fm.h` / `fm.c` provide the sound-generation entry point independently of SDL:

```c
FmConfig config = fmDefaultConfig();
config.operatorCount = 3;
config.operators[0].ratio = 2.0; /* OP1 */
config.operators[0].rm = 3.0;
config.operators[1].rm = 1.5;    /* OP2 -> OP3 */
config.operators[2].waveform = WAVE_TRIANGLE;
config.operators[2].vibratoRateHz = 5.0;
config.operators[2].vibratoDepthCents = 15.0;
FmSynth generator;
if (fmInit(&generator, 48000.0, &config) != 0) {
    return 1; /* Invalid configuration. */
}
fmNoteOn(&generator, true);
float sample = fmNextSample(&generator, 440.0);
/* Continue calling once per sample; on key release: */
fmNoteOff(&generator);
/* Continue rendering the release while fading its output amplitude. */
```

`fmNextSample` takes the base frequency in Hz and returns one sample in
[-1, 1]. Extend `FmSynth` and this function to add more complex generation.
`synth.c` handles note lookup, amplitude fades, mixing, and SDL output.
`oscillator.h` / `oscillator.c` provide waveform generation and vibrato; signed
frequencies run phase backwards, and zero frequency holds phase.

For layers, initialize a `SynthConfig` with `synthDefaultConfig()`, set
`layerCount` and each `layers[i].fm`, `gain`, and `detuneCents`, then call
`synthInitWithLayers()`. `synthAudioSnapshot()` returns the latest combined
output for analysis. Oscillators can also be used directly:

```c
Oscillator oscillator;
oscillatorInit(&oscillator, 48000.0);
oscillatorSetWaveform(&oscillator, WAVE_PULSE);
oscillatorSetPulseWidth(&oscillator, 0.3);
oscillatorSetVibrato(&oscillator, 5.0, 20.0);
float sample = oscillatorNextSample(&oscillator, 440.0);
```

SDL2 reference: [audio callbacks](https://wiki.libsdl.org/SDL2/SDL_OpenAudioDevice)
and [audio-device locking](https://wiki.libsdl.org/SDL2/SDL_LockAudioDevice).
The view uses SDL2's [streaming textures](https://wiki.libsdl.org/SDL2/SDL_LockTexture)
and [logical rendering size](https://wiki.libsdl.org/SDL2/SDL_RenderSetLogicalSize).

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
