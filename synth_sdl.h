#ifndef SYNTH_SDL_H
#define SYNTH_SDL_H
#include "synth.h"

/* Optional singleton SDL audio adapter. Initialize SDL audio first and stop
 * this adapter before SDL_Quit. Calls belong to one main/control thread.
 * Functions returning int use 0/-1 and SDL_GetError, except snapshot which
 * returns the sample rate or 0 when stopped. */
int sdlSynthInit(void);
int sdlSynthInitWithConfig(const FmConfig *config);
int sdlSynthInitWithLayers(const SynthConfig *config);
void sdlSynthShutdown(void);
int sdlSynthConfigure(const SynthConfig *config);
int sdlSynthApplySampleBank(SynthSampleBank *bank);
int sdlSynthSetSourceMode(SynthSourceMode mode);
SynthSourceMode sdlSynthGetSourceMode(void);
int sdlSynthAudioSnapshot(float *samples, uint64_t *samplePosition);
void sdlSynthRegisterNote(int note);
void sdlSynthDeregisterNote(int note);
void sdlSynthRegisterMidiNote(int note);
void sdlSynthRegisterMidiNoteWithVelocity(int note, int velocity);
void sdlSynthDeregisterMidiNote(int note);
#endif
