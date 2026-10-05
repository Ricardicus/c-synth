# libcsynth

A small C11 synth that renders mono float audio into your buffers. There is no
audio device backend and no SDL dependency. Feed the samples to your own audio
system, or render them straight to a file.

The engine supports chords across 128 note pitches, up to eight independent
layers, and an FM chain of up to eight operators in each layer. Waveforms are
sine, square, triangle, saw, pulse, and noise. Each note has a master ADSR;
modulators have their own timbre envelopes. Echo and reverb run after mixing.
The 64 factory sounds include flutes, keys, bells, basses, leads, pads, and
some stranger textures.

Settings can change while notes are held. Each engine has its own state, so
several engines can coexist. The host owns timing and thread synchronization.

## Build

From this folder (or from the repository root once this is a separate repo):

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

You need CMake 3.16+ and a C11 compiler. The Unix build links the system math
library. There are no third-party dependencies. Tests are enabled by default;
use `-DBUILD_TESTING=OFF` for just the library, or `-DCSYNTH_BUILD_TESTS=OFF` to
disable only these tests when embedding it in a larger build.

## Use it

```c
#include "synth.h"

SynthConfig sound = synthPresetConfig(12); /* Flute. */
Synth *engine = synthCreate(48000, &sound);
if (!engine) return;

float samples[256];
synthNoteOn(engine, 60);
synthRender(engine, samples, 256);
/* Hand samples to your audio system. Keep rendering while the note plays. */

sound.effects.reverbMix = 0.25;
synthConfigure(engine, &sound); /* Changes the held note too. */
synthNoteOff(engine, 60);
/* Keep rendering for the release/reverb tail before destroying the engine. */
synthDestroy(engine);
```

Read the [API doc](docs/libcsynth-api.md) for the full interface, settings ranges,
live editing behavior, and threading rules. The [sound guide](presets/README.md)
describes the factory patches.

For an offline example that renders a chord, edits its sound, and releases it:

```sh
cmake -S . -B build -DCSYNTH_BUILD_EXAMPLES=ON
cmake --build build --target csynth_live
./build/csynth_live my-chord.wav
```

## Link from another CMake project

With a source checkout:

```cmake
set(CSYNTH_BUILD_TESTS OFF CACHE BOOL "" FORCE)
add_subdirectory(path/to/libcsynth libcsynth-build)
add_executable(my_player player.c)
target_link_libraries(my_player PRIVATE csynth::csynth)
```

Or install and use its CMake package:

```sh
cmake --install build --prefix /your/install/folder
```

```cmake
find_package(csynth CONFIG REQUIRED)
add_executable(my_player player.c)
target_link_libraries(my_player PRIVATE csynth::csynth)
```

Pass `-DCMAKE_PREFIX_PATH=/your/install/folder` when configuring your project.
The package carries the include paths and math dependency for you.

## What's in here

- `include/`: public C headers.
- `src/`: engine, DSP, preset definitions, and preset file handling.
- `presets/factory/`: all 64 shipped sound files.
- `docs/`: API documentation.
- `examples/`: the WAV renderer.
- `tests/`: engine, FM, envelopes, effects, and preset tests.
- `cmake/`: installed-package configuration.

This folder builds on its own and doesn't refer to its parent directory. The
keyboard window and its SDL adapter live outside it. To split this into a new
repository later, copy this folder's contents, including `.gitignore`.
Saved settings in `presets/user/` are ignored by Git and aren't shipped.
