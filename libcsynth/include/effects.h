#ifndef EFFECTS_H
#define EFFECTS_H
#include "synth_config.h"
typedef struct { float *buffer; int length, cursor; double filtered; } EffectDelay;
typedef struct {
    EffectDelay echo, combs[6], allpasses[2];
    SynthEffectsConfig current, target;
    double sampleRate, smoothing;
} SynthEffects;
int effectsInit(SynthEffects *effects, double sampleRate, SynthEffectsConfig config);
void effectsDestroy(SynthEffects *effects);
void effectsConfigure(SynthEffects *effects, SynthEffectsConfig config);
float effectsNext(SynthEffects *effects, float input);
#endif
