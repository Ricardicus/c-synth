#ifndef SYNTH_H
#define SYNTH_H

#include "synth_config.h"
#include <stddef.h>
#include <stdint.h>

#define SYNTH_ANALYSIS_SAMPLES 2048

typedef struct Synth Synth;

/* SDL-free engine. Rate: 1..384000 Hz. NULL config selects defaults.
 * Returns NULL for invalid settings or allocation failure. */
Synth *synthCreate(int sampleRate, const SynthConfig *config);
void synthDestroy(Synth *engine); /* NULL is harmless. */

/* Caller supplies count mono float samples. No allocations or device I/O.
 * NULL engine produces silence; NULL output is ignored.
 * Each instance must have a single owner, or external synchronization. */
void synthRender(Synth *engine, float *samples, size_t count);

/* Apply copied settings to active and future notes, preserving phases.
 * Return 0 on success, -1 for invalid pointers/settings. Failed changes
 * leave the previous patch intact. These calls do not allocate memory. */
int synthConfigure(Synth *engine, const SynthConfig *config);
int synthGetConfig(const Synth *engine, SynthConfig *config);

/* Note numbers 0..127, middle C=60. Invalid notes/NULL engines are ignored.
 * Repeated note-ons do not retrigger a held note; holds aren't counted.
 * Manual and MIDI ownership are independent. MIDI velocity clamps to 0..127;
 * zero is a silent hold, so use synthMidiNoteOff for a note-off. */
void synthNoteOn(Synth *engine, int note);
void synthNoteOff(Synth *engine, int note);
void synthMidiNoteOn(Synth *engine, int note, int velocity);
void synthMidiNoteOff(Synth *engine, int note);

/* Latest rendered output, oldest first. Caller supplies 2048 floats and a
 * valid position pointer. Returns sample rate, or 0 for invalid pointers.
 * Position counts samples rendered since creation. Not a recording queue. */
int synthAudioSnapshot(const Synth *engine, float *samples, uint64_t *samplePosition);

#endif
