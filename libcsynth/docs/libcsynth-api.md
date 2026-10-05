# libcsynth API

`libcsynth` makes sound. You give it a sample rate and a patch, send it notes,
and ask it to fill an audio buffer. It doesn't open a device, create a window,
or rely on depencencies like SDL. Use it in your own audio callback, render a file, or hand its
samples to whatever audio system your project uses.

## Build and link

From the libcsynth folder, build the library and its tests:

```sh
cmake -S . -B build-lib
cmake --build build-lib
ctest --test-dir build-lib --output-on-failure
```

This builds `libcsynth.a`. You need a C11 compiler and CMake. On Unix the CMake
target also carries the system math library; there are no third-party core
dependencies. Add `-DBUILD_TESTING=OFF` if you only want the library.

In another project using this repo as a subdirectory:

```cmake
set(CSYNTH_BUILD_TESTS OFF CACHE BOOL "" FORCE)
add_subdirectory(path/to/libcsynth synth-build)
add_executable(my_player player.c)
target_link_libraries(my_player PRIVATE csynth::csynth)
```

Or install it, then use `find_package`:

```sh
cmake --install build-lib --prefix /your/install/folder
```

```cmake
find_package(csynth CONFIG REQUIRED)
add_executable(my_player player.c)
target_link_libraries(my_player PRIVATE csynth::csynth)
```

Set `CMAKE_PREFIX_PATH` to your install folder when configuring that project.
The install contains the archive, headers, CMake package, this doc, and the
factory preset files under `share/csynth/presets/factory` in the usual layout.
The installed package doesn't look for an audio backend.

## A small example

```c
#include "synth.h"

SynthConfig sound = synthPresetConfig(5); /* Electric piano. */
Synth *engine = synthCreate(48000, &sound);
if (!engine) {
    /* Invalid settings/rate, or not enough memory. */
    return;
}

float samples[256];
synthNoteOn(engine, 60); /* Middle C. */
synthRender(engine, samples, 256);
/* Send these mono floats to your audio system. */

/* Later, while the note is still held: */
sound.layers[0].fm.operators[0].rm = 0.7;
sound.outputEnvelope.releaseMs = 600;
sound.effects.echoMix = 0.2;
if (synthConfigure(engine, &sound) != 0) {
    /* Invalid settings. The old patch is still in use. */
}
synthRender(engine, samples, 256);

synthNoteOff(engine, 60);
/* Keep rendering to hear the release and effects tails. */
synthDestroy(engine);
```

Nothing advances between render calls. At 48 kHz, 256 samples span about
5.33 ms. For an event that needs to happen partway through a buffer, render up
to that sample, apply the event, then render the rest.

[examples/live.c](../examples/live.c) renders a three-second chord to a WAV,
changes the patch after one second, and releases the chord after two. It uses
no audio device:

```sh
cmake -S . -B build-lib -DCSYNTH_BUILD_EXAMPLES=ON
cmake --build build-lib --target csynth_live
./build-lib/csynth_live my-chord.wav
```

## Engine calls

Include `synth.h`. `Synth` is opaque: create it through the API rather than
putting one on the stack.

| Function | What it does |
| --- | --- |
| `Synth *synthCreate(int sampleRate, const SynthConfig *config)` | Creates an independent engine. A null config selects defaults. Returns null for invalid settings/rate or allocation failure. |
| `void synthDestroy(Synth *engine)` | Frees the engine and delay buffers. Null is harmless. |
| `void synthRender(Synth *engine, float *samples, size_t count)` | Writes `count` mono floats and advances the engine by that many samples. |
| `int synthConfigure(Synth *engine, const SynthConfig *config)` | Copies a patch into the engine, affecting held notes and future notes. Returns 0 or −1. |
| `int synthGetConfig(const Synth *engine, SynthConfig *config)` | Copies the current patch out. Returns 0 or −1 for invalid pointers. |

Sample rate must be 1–384000 Hz; use the actual rate of your audio system.
An engine's rate stays fixed for its lifetime. Output samples are clamped to
−1…1. You can render any block size, including one sample. A zero-length render
doesn't advance time. A null output pointer is ignored; rendering with a null
engine fills the supplied buffer with silence.

Each instance has its own voices, patch, effects, and history. Creating and
destroying engines allocates/frees memory. Rendering, note calls, snapshots,
configuration changes, and config reads don't allocate or do device/file I/O.

There are no built-in locks. Keep each instance on one thread, or synchronize
access yourself. In a real-time host, a good pattern is to queue note/parameter
events from the UI and apply them on the audio thread between render calls.
Don't configure, inspect, or destroy an instance concurrently with rendering.
Separate instances can run on separate threads.

The core doesn't keep a global error string. Check return values and use
`synthConfigValid()` before creation/configuration if you need to distinguish
an invalid patch from an allocation failure.

## Notes

```c
void synthNoteOn(Synth *engine, int note);
void synthNoteOff(Synth *engine, int note);
void synthMidiNoteOn(Synth *engine, int note, int velocity);
void synthMidiNoteOff(Synth *engine, int note);
```

Note numbers are 0–127: C4 is 60 and A4 is 69, at 440 Hz. Invalid numbers and
null engines are ignored. Every pitch has its own voice and output envelope,
so chords work. The current patch is shared by all notes in an engine.

Manual notes use full velocity. MIDI velocity is clamped to 0–127 and changes
smooth over about 5 ms. A zero velocity is a silent hold; use `synthMidiNoteOff()`
for a note-off, including MIDI note-on messages whose velocity is zero.

Manual and MIDI ownership are independent. If both hold C4, releasing MIDI
leaves the manual C4 sounding. Once the manual hold is released, a remaining
MIDI hold uses its stored velocity. Holds aren't reference counted: repeated
note-ons do not retrigger a held pitch, and one matching note-off releases that
source. Playing a note during its release retriggers from the current level.

These functions accept note events; the core doesn't read MIDI files or devices.
A MIDI player or another event source can call them at the right sample time.

## Settings and live edits

Start with `synthDefaultConfig()` or `synthPresetConfig(index)`, then edit fields.
They return `SynthConfig` by value. An all-zero struct isn't a valid starting
point: a ratio and pulse width must be positive even for a sine operator.

| `SynthConfig` field | Meaning / range |
| --- | --- |
| `layerCount` | Active layers, 1–8. |
| `layers[i].gain` | 0–1. Each layer gain is divided by the active layer count when mixed. |
| `layers[i].detuneCents` | −4800 to +4800 cents for the whole chain. 100 cents = one semitone. |
| `layers[i].fm` | That layer's FM chain. |
| `outputEnvelope` | Nonnegative attack/decay/release in milliseconds, sustain in 0–100 percent. Each note has its own amplitude envelope. |
| `effects` | Echo and reverb applied after mixing all notes. |

`synthConfigValid()` checks the active layers/operators; `synthEffectsConfigValid()`
checks just effects. Both return false for null pointers. Eight layer slots and
eight operator slots per layer are stored, but only the selected counts sound.

Edit a local copy and call `synthConfigure(engine, &sound)`. Editing the struct
alone doesn't change the engine. The engine copies it; your pointer doesn't need
to stay alive. Use `synthGetConfig()` if you need to start from the current sound.
Invalid updates leave that sound intact.

Live edits affect held notes, release tails, and future notes. Existing phases
are kept. A modulator's envelope progress is kept if it remains a modulator
with the same envelope mode. Added layers/operators start fresh. Changing
waveforms or chain length can make an audible jump; this isn't fully smoothed
automation. Master sustain and effects are smoothed. Editing a master envelope
time restarts the current stage from its current level with the new duration.

### FM chain

`FmConfig.operatorCount` is 1–8. `operators[]` runs OP1 → OP2 → … → OPn.
Each operator modulates the next one's frequency; the last is the audible
carrier. With one operator, it's just an oscillator.

| `FmOperatorConfig` field | Meaning |
| --- | --- |
| `waveform` | `WAVE_SINE`, `WAVE_SQUARE`, `WAVE_TRIANGLE`, `WAVE_SAWTOOTH`, `WAVE_PULSE`, or `WAVE_NOISE`. |
| `ratio` | Positive, finite multiple of the played frequency. 2 is an octave up. |
| `rm` | Nonnegative, finite depth into the next operator: deviation divided by this operator's nominal frequency. |
| `pulseWidth` | Fraction of the cycle high, strictly between 0 and 1. Used by pulse. |
| `vibratoRateHz` | Nonnegative, finite rate. |
| `vibratoDepthCents` | 0–1200; zero disables vibrato. |
| `indexMode` | `FM_INDEX_SUSTAIN`, `FM_INDEX_DECAY`, or `FM_INDEX_ADSR`. |
| `decayRate` | Nonnegative, finite exponential decay rate per second, used by decay mode. |
| `attackMs`, `decayMs`, `sustainPercent`, `releaseMs` | Modulation ADSR: nonnegative times in ms, sustain 0–100. |

The carrier's depth/index envelope are unused: it has no next operator.
Operator envelopes change timbre; master ADSR changes volume. Noise is white
noise, so its pitch ratio doesn't create a pitched tone.

The API accepts wider ranges than some GUI knobs. High ratios and depths can
alias. Square, saw, and pulse are direct waveforms without a band-limiting
filter. Each voice has a base gain of 0.1; many notes together can still clip.

### Effects

| `SynthEffectsConfig` field | Range / units |
| --- | --- |
| `echoMix` | 0–1. |
| `echoDelayMs` | 1–2000 ms. |
| `echoFeedback` | 0–0.95. |
| `reverbMix` | 0–1. |
| `reverbRoom` | 0–0.95; higher means longer tails. |
| `reverbDamping` | 0–1; higher absorbs more high frequencies. |

Echo is a feedback delay. Reverb uses six damped comb delays and two all-pass
stages. Their buffers survive live edits, so a previous patch can leave a tail.

## Presets and files

`SYNTH_PRESET_COUNT` is currently 64. `synthPresetName(index)` returns a name and
`synthPresetConfig(index)` returns its compiled patch. Valid indices run from
0 to `SYNTH_PRESET_COUNT - 1`. Invalid indices return `"Custom"` and the default
patch. These calls don't read files or require an engine.

Include `presets.h` for file-backed sounds:

```c
PresetLibrary library;
char error[256];
if (presetLibraryInit(&library, "presets", error, sizeof(error)) != 0) {
    return; /* error contains the explanation. */
}
SynthConfig sound = library.items[5].config;
int saved = presetLibrarySave(&library, "My soft keys", &sound,
                              error, sizeof(error));
/* saved is the new index, or -1 on failure. */
(void)saved;
presetLibraryDestroy(&library);
```

Initialization loads `folder/factory` (falling back to compiled sounds for
missing/invalid files), then `folder/user`. Bad user files are skipped and
counted in `library.skipped`. Saved sounds are sorted on load; new saves append.
Each entry has `name`, `config`, `path`, and `builtin`. The limit is 256 entries.

Names allow 1–32 ASCII letters/numbers, spaces, hyphens, and underscores, with
at least one letter/number and no leading/trailing spaces. Duplicate names or
filenames are rejected. `presetLibraryDelete(&library, index, error, size)`
returns 0 or −1 and removes a saved file/list entry. Factory entries can't be
deleted. Deleting an entry doesn't change an engine's sound.

For individual files:

```c
char name[PRESET_NAME_MAX + 1], error[256];
SynthConfig sound;
int result = presetRead("my-sound.synth", name, &sound, error, sizeof(error));
/* presetWrite(path, name, &sound, error, sizeof(error)) writes a new file. */
(void)result;
```

Both return 0 or −1. Writing refuses to overwrite a file and keeps full float
precision. It also saves and validates inactive slots. Start from a default or
preset so those slots contain valid settings. Pass valid pointers/buffers.
Filesystem calls belong outside audio callbacks. Destroy an initialized library
before reinitializing it. The folder is explicit; the core doesn't read the
app's `SYNTH_PRESET_DIR` environment variable.

## Output history

```c
float history[SYNTH_ANALYSIS_SAMPLES];
uint64_t position;
int rate = synthAudioSnapshot(engine, history, &position);
```

This copies the last 2048 rendered samples, oldest first, with unused startup
history filled with zeros. It includes envelopes, velocity, effects, and clipping.
`position` counts rendered samples since creation. Time is
`position / (double)rate` seconds; each sample spans `1.0 / rate` seconds.

Returns the rate, or 0 for a null engine/output/position pointer. Invalid calls
leave outputs untouched. This is rolling history, not a recording queue; reads
can overlap or miss audio. For recording, save the buffers from `synthRender()`.

## The smaller DSP pieces

You can also use the pieces directly if you don't need the full polyphonic engine:

| Header | Functions |
| --- | --- |
| `fm.h` | `fmDefaultConfig()`, `fmConfigValid()`, `fmInit()`, `fmNoteOn()`, `fmNoteOff()`, `fmNextSample()`, `fmReleaseDurationMs()` |
| `oscillator.h` | `oscillatorInit()`, waveform/pulse/vibrato setters, `oscillatorNextSample()`, waveform name/parser helpers |
| `envelope.h` | `outputEnvelopeConfigure()`, `outputEnvelopeOn()`, `outputEnvelopeOff()`, `outputEnvelopeNext()` |
| `effects.h` | `effectsInit()`, `effectsConfigure()`, `effectsNext()`, `effectsDestroy()` |

`fmInit()` takes an instance, a finite positive sample rate, and a valid config;
it returns 0 or −1. `fmNoteOn(&voice, true)` resets phases, while `false` keeps
them for retriggering. `fmNextSample(&voice, frequencyHz)` returns one sample in
−1…1. Note-off releases modulation, not amplitude. `fmReleaseDurationMs()`
reports the longest modulator ADSR release.

Initialize `OutputEnvelope envelope = {0};`, configure valid ADSR settings,
and multiply the waveform by `outputEnvelopeNext(&envelope, sampleRate)` once
per sample. It returns an amplitude in 0–1. On/off calls start attack/release.

Initialize effects with `effectsInit()` (0 or −1), process the mixed sample with
`effectsNext()`, then call `effectsDestroy()`. Initialization allocates buffers;
processing/configuration doesn't. These helpers don't all validate arguments
for you: use validators and a finite positive sample rate. Own each instance
on one thread. For phase-preserving whole-patch edits, use the full engine.

## Audio backends

This library doesn't ship an audio backend. The keyboard app in the parent
project has its own SDL adapter, outside this folder. It calls `synthRender()`
from its device callback and locks the device around control calls. Your own
project can use the same pattern with whichever audio backend it already has.
