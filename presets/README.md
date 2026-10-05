# Factory sound guide

64 factory sounds are available in the preset dropdown. Choose a sound and play;
each includes its own layers, master ADSR, echo, and reverb. The first 13 retain
the original names and positions. Subsequent sounds are grouped by family.

These are synthesized interpretations of instruments. FM ratios and modulation
envelopes create their harmonics; noise adds breath, detuning adds movement,
and independent layers add overtones. Echo times are in milliseconds and are
not synchronized to MIDI tempo. The synth output is mono.

## Flutes and winds

Start around C4–C6 for flutes and reeds. Alto and Bass transpose down one and two
octaves; Piccolo transposes up one octave. Pan has a short, blown articulation;
Dream rewards longer notes. Breath is a quiet independent noise layer, so its
level can be adjusted with the layer gain knob.

| Sound | Character |
| --- | --- |
| Flute | Original soft FM flute with breath and gentle vibrato |
| Flute Concert | Clear fundamental, restrained breath, light room |
| Flute Alto | Lower, rounder tone with slower vibrato |
| Flute Bass | Two octaves down, soft attack and darker ambience |
| Flute Piccolo | Octave-up tone, quick articulation, subtle breath |
| Flute Bamboo | Woody upper harmonics and more audible breath |
| Flute Pan | Brief breathy attack that fades while held |
| Flute Dream | Detuned flute layers, lingering release, echo |
| Reed Clarinet | Hollow odd-harmonic FM tone |
| Reed Oboe | More insistent harmonics with a quiet second layer |
| Reed Bassoon | Low register with a rounded reed character |
| Brass | Original triangle-carrier brass with a swelling attack |
| Brass Mellow Horn | Gradual bloom and rounded sustained harmonics |
| Brass Bright Section | Three slightly detuned voices with brighter attacks |

## Keys and bells

Play short phrases or chords, allowing the release tails to ring. Most bells
and mallets decay to silence even with a key held. Low notes emphasize the
inharmonic beating of the bowls and larger bells.

| Sound | Character |
| --- | --- |
| Electric piano | Original bright attack that settles into a soft sustain |
| Keys Velvet EP | Warm fundamental with a mellow, fading FM attack |
| Keys Tine EP | Bright tine-like overtone over a softer body |
| Keys Worn EP | Rounded electric keys with slow pitch drift |
| Keys Digital Grand | Layered harmonic attacks and a sustained fundamental |
| Keys Toy Piano | Short, slightly inharmonic miniature piano |
| Keys Celesta | Octave-up chiming keys with a long, soft tail |
| Glass bell | Original glassy FM bell with light reverb |
| Metal chime | Original high-ratio metallic strike |
| Bell Tubular | Two inharmonic strikes with a long decay |
| Bell Singing Bowl | Soft onset and slowly beating noninteger harmonics |
| Bell Ice Crystal | High crystalline partials with distinct echoes |
| Bell Church | Low bell body, inharmonic overtones, spacious decay |
| Bell Music Box | Small, bright octave-up pluck |
| Soft organ | Fundamental, octave, and upper-octave sine layers |

## Basses and leads

Basses include octave-down layers: try C3–C4 on the keyboard first. Rubber FM,
Picked Wire, and Laser change brightness rapidly during their attack. Leads
with echo work well with gaps between notes.

| Sound | Character |
| --- | --- |
| Pulse bass | Original octave-down pulse, with a shaped amplitude envelope |
| Bass Rounded Sub | Strong sine foundation with a gentle harmonic attack |
| Bass Rubber FM | Snappy modulation that rapidly settles into a low body |
| Bass Electric Finger | Rounded fundamental and a decaying plucked edge |
| Bass Picked Wire | Fast, bright strike with a quiet higher-ratio transient |
| Bass Growl | Three-operator modulation chain plus a sine foundation |
| Bass Hollow Pulse | Pulse body supported by a lower sine layer |
| Saw lead | Original saw, with a smoother release and restrained ambience |
| Lead Liquid | Expressive FM attack, gentle vibrato, and echo |
| Lead Octave Shine | Detuned saws at two octaves with sine support |
| Lead Soft Square | Hollow sine FM blended with a triangle body |
| Lead Laser | Strong initial modulation that rapidly collapses |
| Lead Arcade | Square and upper-octave pulse with a short echo |

## Pads and plucks

Hold pad chords for at least two seconds to hear their evolution. Release them
to hear the tails. Fifth Horizon includes a fifth above the played note, so
simple intervals can work better than dense chords.

| Sound | Character |
| --- | --- |
| Wide pad | Original three-layer detuned FM pad with warm reverb |
| Pad Aurora | Slowly changing FM brightness and gentle pitch movement |
| Pad Velvet Strings | Detuned triangles with a gradual FM layer |
| Pad Glass Ocean | Slowly blooming, inharmonic glass texture |
| Pad Dark Orbit | Low, slowly drifting FM voices and dark reverb |
| Pad Choir Haze | Detuned harmonic clusters over a sine foundation |
| Pad Fifth Horizon | Fundamental, fifth, and lower octave with echo |
| Pluck Nylon | Soft harmonic attack with a short body |
| Pluck Steel | Brighter attack with slightly offset upper harmonics |
| Pluck Kalimba | Compact tine-like strike with an octave accent |
| Pluck Marimba | Brief bright transient that settles into a rounded body |
| Pluck Echo Harp | Clear layered pluck followed by repeating echoes |

## Experimental and starting points

| Sound | Character |
| --- | --- |
| Classic FM | Original two-operator FM starting point |
| Pure sine | Single unprocessed sine |
| Warm triangle | Single unprocessed triangle |
| Space wobble | Original large vibrato and strong FM, now with echo and reverb |
| FX Cosmic Transmission | Nested noninteger FM with an unstable lower layer |
| FX Metallic Rain | Short inharmonic strikes and closely spaced echoes |
| FX Ghost Whistle | Two high, slowly wavering tones in a long ambience |
| FX Rusted Machine | Low nested FM with a quiet noise layer |
| FX Ocean Breath | Slow noise swell over a low pitched tone |
| FX Starfall | High nested modulation with a bright upper layer and echoes |

## Files and saved sounds

`factory/` contains all 64 shipped `.synth` files. Built-in versions provide a
fallback if those files are unavailable. `user/` contains sounds saved with
**Save setting**; changes to the factory bank do not change those files.

The version 1 format preserves all eight layer slots and all eight operator
slots per layer, including inactive settings. `layers` and each `layer` row's
operator count determine which slots sound. Repeated inactive rows are expected.
Values retain full precision when saved; the on-screen controls round them.
