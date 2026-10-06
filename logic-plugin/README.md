# CSynth for Logic Pro

This is a software-instrument plugin built around
[libcsynth](https://github.com/Ricardicus/libcsynth). It turns MIDI notes into
FM synth audio and presents the sound controls inside Logic Pro.

The Logic version is an **Audio Unit v2 instrument** called **CSynth**, under
manufacturer **Ricardicus**. The build also creates a standalone app for trying
sounds without a DAW. There is no SDL dependency in this project: Logic supplies
the audio device and MIDI, JUCE supplies the plugin wrapper/editor, and
libcsynth renders the samples.

![CSynth editor](docs/editor.png)

## What you can do

- Choose any of the 64 factory sounds and step through them with the arrows.
- Play chords with velocity and a sustain pedal.
- Edit up to eight layers and eight FM operators per layer, including waveforms,
  FM depth, ratios, modulation envelopes, vibrato, pulse width, gain, and detune.
- Change master ADSR, low-pass/high-pass cutoffs, echo, and reverb while playing.
- Automate the sound controls in Logic. Each layer/operator has its own parameter
  IDs, even though the editor shows one selected slot at a time.
- Watch the final output in a scrolling spectrogram visible on every tab.
- Save a Logic project and get the whole edited patch back when you reopen it.
- Import/export `.synth` files shared with the SDL app.

The engine is mono; the stereo plugin sends the same signal to both channels.
A stereo Logic track is supported, but this does not add stereo width. Place a
Logic stereo effect after it if you want that.

## What you need

- macOS, with an Apple Silicon or Intel Mac.
- Xcode or the Command Line Tools, with the macOS SDK.
- CMake 3.22 or newer, a C/C++ compiler, and Git.
- Internet access for the first JUCE download, unless you supply a local checkout.
- Logic Pro to use the Audio Unit. The standalone app and automated tests do not
  need Logic.

This project targets macOS 11 or newer; Logic itself may require a newer OS.

If you need Command Line Tools, run `xcode-select --install`. If you use Homebrew,
CMake can be installed with `brew install cmake`. With full Xcode, check that
`xcode-select -p` points at the Xcode installation you intend to use.

## Build it

From the parent synth-garage folder:

```sh
git submodule update --init --recursive
cmake -S logic-plugin -B logic-plugin/build -DCMAKE_BUILD_TYPE=Release
cmake --build logic-plugin/build --config Release --parallel 4
ctest --test-dir logic-plugin/build -C Release --output-on-failure
```

Or, from this folder:

```sh
git submodule update --init --recursive
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel 4
ctest --test-dir build -C Release --output-on-failure
```

libcsynth is a Git submodule inside this project at `libcsynth/`. Initialize it
before configuring; CMake uses that checkout and reports a clear error if it is
missing. Git records the exact library revision, so CMake doesn't download or
update libcsynth. JUCE 8.0.12 is still downloaded by CMake at a pinned commit.

To use an existing JUCE checkout:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
  -DFETCHCONTENT_SOURCE_DIR_JUCE=/absolute/path/to/JUCE
```

To update the library intentionally, from the parent repository run:

```sh
git submodule update --remote logic-plugin/libcsynth
```

Build and test afterward, then commit the changed submodule pointer. The current
revision includes `SynthConfig.filters`, which the plugin requires.

The first full build takes longer because JUCE is compiled too. Subsequent
source edits reuse those objects. The build doesn't install anything into your
Library folders automatically.

### Build results

For the commands above, the important files are:

```text
build/CSynth_artefacts/Release/AU/CSynth.component
build/CSynth_artefacts/Release/Standalone/CSynth.app
```

`.component` is the plugin bundle Logic loads. Keep the entire bundle together;
you don't copy just the executable inside it. The standalone `.app` is separate.

To build only the Logic plugin:

```sh
cmake --build build --config Release --target CSynth_AU --parallel 4
```

### Apple Silicon, Intel, and universal builds

The default build targets the architecture of the machine doing the build.
An Apple Silicon build is suitable for native Logic on Apple Silicon; an Intel
build is suitable for Intel Logic or Logic running under Rosetta.

For a bundle containing both architectures, use a separate build folder:

```sh
cmake -S . -B build-universal -DCMAKE_BUILD_TYPE=Release \
  '-DCMAKE_OSX_ARCHITECTURES=arm64;x86_64'
cmake --build build-universal --config Release --parallel 4
```

Check the result with:

```sh
lipo -archs build-universal/CSynth_artefacts/Release/AU/CSynth.component/Contents/MacOS/CSynth
```

Both libcsynth and the plugin wrapper are built for the selected architectures.
CMake signs each completed bundle with an ad-hoc signature.
The default development build is locally signed; it isn't a notarized public
release. For distribution, sign with your Developer ID and notarize/package the
bundle as part of your release process.

## Install it for Logic

Quit Logic before replacing a build, so it doesn't keep the previous binary in
memory. From this project's folder, run:

```sh
./scripts/install-au.sh
```

For another build folder:

```sh
./scripts/install-au.sh ./build-universal
```

The script copies the component to:

```text
~/Library/Audio/Plug-Ins/Components/CSynth.component
```

It installs for your user, without `sudo`. If a CSynth component already exists,
it moves that bundle to `~/Library/Audio/CSynth-backups/` first. It then checks the
new bundle's signature. The build itself never runs this script.

You can do the same in Finder: Go > Go to Folder, enter
`~/Library/Audio/Plug-Ins/Components`, and copy **CSynth.component** there.
Apple documents both the user and system-wide component directories in
[its Audio Units installation-folder guide](https://support.apple.com/en-us/102239).

Do not put the standalone app in the Components folder. It belongs wherever you
normally keep apps. You also don't need to copy libcsynth separately: its code
and factory presets are compiled into the component.

## Load it in Logic Pro

1. Reopen Logic and allow its Audio Unit scan to finish.
2. Open or create a project and add a **Software Instrument** track.
3. In that track's channel strip, open the **Instrument** slot.
4. Choose **AU Instruments → Ricardicus → CSynth → Stereo**. Some Logic versions
   call the Audio Unit submenu **Audio Units**. A mono option is available too.
5. Select the track, play your MIDI keyboard, or open Logic's Musical Typing with
   **Command-K**. CSynth follows MIDI from Logic’s selected instrument track; its Keyboard tab
   also lets you test sounds with the mouse.
6. Pick a factory preset. Try Flute Concert, Keys Tine EP, Bass Rubber FM,
   Pad Aurora, or Bell Singing Bowl.

CSynth belongs in the instrument slot of a software-instrument channel strip.
It doesn't process audio sent through an Audio FX insert. Apple describes the
instrument-slot workflow in [its instrument/plugin guide](https://help.apple.com/logicpro/mac/9.1.6/en/logicpro/usermanual/chapter_10_section_2.html).

Once loaded, you can record MIDI into the track or drag a MIDI file into Logic
and put its region on this instrument track. Logic handles the file playback;
there is no MIDI-file player inside the plugin.

## If it doesn't show up

Open **Logic Pro → Settings → Plug-In Manager** (older versions use
**Preferences**). Find **Ricardicus / CSynth**, check its validation status, and
use **Reset & Rescan Selection** if needed. Make sure it is enabled. Apple's
[Plug-in Manager guide](https://support.apple.com/en-au/guide/logicpro/lgcp9e26ef17/mac)
explains these controls.

For a command-line validation after installation:

```sh
auval -v aumu Csyn Rica
```

The codes are case-sensitive: `aumu` is an instrument, `Csyn` is this plugin,
and `Rica` is the manufacturer. A successful validation should report that the
Audio Unit passed. `auval -a` lists registered Audio Units if you need to check
whether the component has been discovered at all.

If discovery fails:

- Confirm the component is directly inside your Components folder, rather than
  inside another copied directory.
- Quit and reopen Logic after installing/replacing it; restarting macOS can also
  refresh Audio Unit discovery.
- Check architecture with `lipo -archs`, especially if Logic is using Rosetta.
- Check the local signature with `codesign --verify --deep --strict` followed by
  the component path.
- Look for another installed **CSynth.component** in `/Library/Audio/Plug-Ins/Components`
  that could be competing with your user-installed build.

Start with the targeted rescan. There is no need to delete Logic's preferences
or unrelated plugin caches as part of this project's installation.

## Working with the controls

The editor is 1,180 × 700 by default and has four tabs: **Synth** for layers
and operators, **Output & filters** for master ADSR, filters, and gain, and
**Effects** for echo and reverb, and **Keyboard** for testing sounds with the mouse.
Presets and the live spectrogram stay visible
on every tab. Switching tabs preserves your edits and does not interrupt audio.
![Keyboard tab](docs/keyboard.png)

On Keyboard, click or drag across the piano to play. Switching away releases
notes held by the on-screen keyboard. MIDI from Logic and Musical Typing work
on every tab.

The top row chooses a **complete factory patch**. Its arrows wrap at either end.
Factory selection replaces the synth settings while preserving the Output gain.
The name remains the last chosen factory preset when you edit it; your edits
are still saved in the Logic project. Importing a `.synth` setting shows Custom.

**Output / Master ADSR** controls note volume and final gain. A filter set to
**Off** is bypassed; enter 0 to turn it off. Filter cutoffs run from 20 to
20000 Hz, clamped internally below Nyquist for the host sample rate. The filters
run on the voice mix before echo/reverb, so changing a cutoff doesn't erase
an effect tail that is already ringing.

**Echo / Reverb** controls the shared effects. Echo delay is in milliseconds,
not musical divisions, and isn't synchronized to Logic's tempo.

**Edit layer** and **Edit operator** choose which stored slot the controls show.
They don't change the active counts. Turn up Active layers / Active operators
to bring more slots into the sound. Inactive slots keep their settings and are
included in saved state.

The final active operator is the carrier. It has no next operator to modulate,
so its FM-depth/index-envelope controls are disabled. Modulators shape timbre;
the master ADSR shapes amplitude. Waveforms include noise for breath/texture.

Knobs support dragging, wheel changes, and typing into their value boxes.
Shift-drag provides fine control. **Release notes** releases the held MIDI notes;
the configured release and effect tails still finish normally.

Disabled operator controls show a reason below the knob. For example, pulse
width asks you to choose Pulse, and index-envelope knobs ask for Decay or ADSR
mode. The carrier has no FM depth or index envelope; select an earlier operator
or increase Active operators to make it a modulator. Hover a control’s label or
reason for the full explanation. These hints update when you change the selected
operator, waveform, envelope mode, or preset.

![Echo timing controls](docs/effects.png)

On the Effects tab, **Echo timing** offers **Free (ms)** plus 1/32, 1/16, 1/8,
1/4, 1/2, and whole-note divisions, each straight, dotted, or triplet. Synced
notes follow the project tempo, including tempo changes. A dotted note is 1.5
times the straight length; a triplet is two-thirds. At 120 BPM, a quarter note
is 500 ms and a dotted eighth is 375 ms. These are note values, not fractions
of a bar, so a quarter stays a quarter when the time signature changes.

Free mode keeps the existing 1–2,000 ms knob. In sync mode that knob shows why
it is disabled, and the readout beside the menu shows the actual delay and tempo.
Switching back to Free restores the millisecond value you left there. Without
host tempo, sync starts at 120 BPM; once a valid tempo arrives, the last valid
value is retained if the host stops reporting it. Delays are limited to 1 ms–30
seconds, with a visible limit notice when necessary. Changes are smoothed and
may briefly bend the pitch of an existing echo tail.

Echo timing is automatable and saved in Logic projects/plugin settings. Older
project states open in Free mode. Factory presets and `.synth` imports select
Free; `.synth` export stores the free millisecond patch, since the shared format
has no host-tempo division field. Use Logic settings to preserve sync mode.

The live spectrogram shows the final signal after filters, effects, and output
gain. Time runs left to right (newest at the right), frequency runs bottom to
top on a logarithmic scale, and brighter colours mean stronger frequency bins.
The display spans 40 Hz to 20 kHz, capped by the current sample rate’s Nyquist
frequency. Its 2,048-sample Hann-windowed FFT uses 50% overlap and a 30 Hz UI
timer. At 48 kHz, the history covers about 10.9 seconds.

The audio thread only copies samples into a fixed-size queue while an editor
exists. FFTs and drawing run on the UI thread, and closing the editor disables
capture. If the UI falls behind, visualization data is discarded; audio never
waits for the display. The spectrogram is not stored with your patch.

## Saving sounds and projects

Saving the Logic project stores the complete parameter state for each plugin
instance, including all inactive layers/operators, filters, effects, and Output
gain. Reopening the project doesn't depend on a `.synth` file still being present.
You can also use Logic's plugin settings menu to save/recall a setting for reuse
in other projects.

**Import .synth** reads settings saved by the SDL app or libcsynth. Version 1
files load with filters off; version 2 includes filter cutoffs. Values beyond
this plugin's parameter ranges are clamped to the supported range.

**Export .synth** writes the synth patch to a new file. Its name is derived from
the filename using up to 32 ASCII letters/numbers, spaces, hyphens, and
underscores. It refuses to overwrite an existing setting, so choose a new
filename for each revision. This format contains the libcsynth patch; the
plugin-only Output gain is saved by Logic rather than in `.synth` files.

## Automation and MIDI behavior

Press **A** in Logic to show track automation, then select a CSynth parameter
from the track's automation menu. Master controls have names such as Low-pass
cutoff. Layer controls have names such as Layer 1 OP2 FM depth. The editor's
layer/operator selection isn't itself a sound parameter: choosing another slot
doesn't retarget an automation lane you've already recorded.

Parameter changes reach the engine at audio-block boundaries. MIDI note and
sustain events split the block at their sample offsets, so notes aren't all
rounded to the start of a buffer. Filter, effects, velocity, and Output gain
changes are smoothed. Not every waveform/chain change can be made without a
click; those edits change the underlying waveform/structure directly.

All 16 MIDI channels use the current patch, including channel 10. Sustain is
handled per channel, and overlapping channel holds of the same pitch don't
cut each other off. Repeated note-ons at the same pitch are legato, following
libcsynth's one-voice-per-pitch model. Pitch bend, MPE, aftertouch, modulation-wheel
mapping, MIDI-clock sync, and general MIDI program-change messages aren't
implemented in this first version. Note-off, sustain, reset controllers, and
all-notes/all-sound-off messages release their holds; amplitude/effects tails
follow the patch. This isn't a General MIDI instrument bank.

Large layered patches across many notes use more CPU. If playback crackles,
reduce active layers/operators or raise Logic's audio buffer size. Some very
high FM ratios/depths and raw saw/pulse/square waveforms can alias. Lower Output
gain if chords are too loud; the core also has its own final clipping stage.

## Try the standalone app

Open:

```sh
open build/CSynth_artefacts/Release/Standalone/CSynth.app
```

Choose an audio output and MIDI input in its Options/settings panel. Play a MIDI
keyboard, or click the piano on the Keyboard tab. This is a convenient way to check the
synth without waiting for Logic's plugin scan. The app doesn't need SDL.

## Development and tests

```sh
cmake --build build --config Release --parallel 4
ctest --test-dir build -C Release --output-on-failure
```

These checks need a normal macOS process: a restricted sandbox may prevent
`AudioComponentRegister` from registering the test component. Run CTest outside
such a sandbox. The tests do not install the plugin.

The processor test covers MIDI sample offsets, sustain, overlapping channels,
state restoration (including inactive operators), independent instances, every
factory preset, and constructing the editor. The Audio Unit test loads the built
component directly, creates an instrument instance, renders MIDI, and round-trips
its AU state. It doesn't install the plugin into your Library.

That direct-load test isn't a substitute for Logic's own validation and a play
check in a real project. After installing a changed build, run `auval` and rescan
in Logic. Test saving/reopening a project and a bounce before relying on a build
for a session.

The Audio Unit identity is in `CMakeLists.txt`: bundle ID
`com.ricardicus.csynth`, manufacturer `Rica`, subtype `Csyn`, and export prefix
`CSynthAU`. Keep these stable when updating a plugin that existing Logic projects
use. Parameter IDs are also stable state/automation identifiers; changing them
can break old settings and automation.

`Source/Parameters.cpp` maps libcsynth fields to host parameters.
`Source/PluginProcessor.cpp` owns the engine and handles audio/MIDI/state.
`Source/PluginEditor.cpp` builds the UI. During processing, engine mutations happen on the audio
thread; the editor and host communicate through parameter values.

To move this project into a separate repo, copy this folder **without** its
`build*` directories or `libcsynth/` checkout, then initialize the new repository:

```sh
git init
git submodule add https://github.com/Ricardicus/libcsynth.git libcsynth
git -C libcsynth checkout 4f33f0c3dafa001595b5b4d14b421c5913aa709c
git add .gitmodules libcsynth
```

Commit the project files along with `.gitmodules` and the submodule pointer.
The registration currently lives in the parent repository’s `.gitmodules`,
so copying files alone does not preserve it. Future clones of the new repo can
use `git clone --recurse-submodules <your-repository-url>`. The sibling SDL app
and its build aren't required.

## Distribution

This is a source/development project, not a signed public installer. JUCE is
dual-licensed; choose a suitable licence for how you distribute the plugin.
See [the pinned JUCE version's licence](https://github.com/juce-framework/JUCE/blob/8.0.12/LICENSE.md)
and the libcsynth repository's terms. Code signing/notarization and any release
licensing decisions belong in the distribution process.

### Checked build

The native arm64 Release build passed both processor and Audio Unit tests.
The AU test loaded the built component, rendered offset MIDI, and restored its
state. The AU and standalone bundles also passed `codesign --verify --deep
--strict`. The editor screenshot above comes from that build. Logic itself
still needs the installation and manual check described above; the automated
test does not replace Logic’s scan or `auval`.

The plugin build defines `CSYNTH_ECHO_MAX_DELAY_MS=30000` for libcsynth and
its clients. This uses about 5.8 MB for the echo buffer at 48 kHz, allocated
once when the engine starts. The submodule contains the small capacity-option
change required for this; commit/publish those library changes before sharing
an updated submodule pointer with another checkout. Ordinary library builds
still default to 2,000 ms.
