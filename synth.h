#ifndef SYNTH_H
#define SYNTH_H

#include "synth_config.h"
#include <stdint.h>

#define SYNTH_ANALYSIS_SAMPLES 2048

/* Call after SDL_Init(SDL_INIT_AUDIO). Returns 0 on success, -1 on failure;
 * SDL_GetError() describes the failure. Call synthShutdown before SDL_Quit.
 * All public functions are intended for the main thread. */
int synthInit(void);
/* Start with custom FM settings; use synthInit() for defaults. */
int synthInitWithConfig(const FmConfig *config);
int synthInitWithLayers(const SynthConfig *config);
void synthShutdown(void);
/* Apply settings to sounding and future notes without reopening audio. */
int synthConfigure(const SynthConfig *config);

/* Copy the latest mixed output in chronological order. Returns sample rate,
 * or 0 when stopped. samplePosition counts samples produced since startup.
 * The caller supplies SYNTH_ANALYSIS_SAMPLES floats. Main thread only. */
int synthAudioSnapshot(float *samples, uint64_t *samplePosition);

/* MIDI note numbers 0..127; middle C is 60. Invalid notes are ignored.
 * Registering an already held note is harmless. Multiple notes can play. */
void registerNote(int note);
void deregisterNote(int note);
/* Independent MIDI ownership keeps manual and file-played notes from cutting each other off. */
void registerMidiNote(int note);
void registerMidiNoteWithVelocity(int note, int velocity);
void deregisterMidiNote(int note);

#endif
