#ifndef SPECTROGRAM_H
#define SPECTROGRAM_H

#include <SDL.h>
#include "synth_config.h"

typedef struct Spectrogram Spectrogram;

Spectrogram *spectrogramCreate(SDL_Window *window, int layerCount);
/* Snapshot mixed audio, analyze new samples, draw; 0 success, -1 SDL error. */
int spectrogramDraw(Spectrogram *view);
/* Update the numeric equations to the most recently played note. */
void spectrogramNote(Spectrogram *view, int note);
void spectrogramSetConfig(Spectrogram *view, const SynthConfig *config);
/* 1 consumed, 0 unhandled, -1 error. SDL logical mouse coordinates. */
int spectrogramEvent(Spectrogram *view, const SDL_Event *event);
void spectrogramDestroy(Spectrogram *view);

#endif
