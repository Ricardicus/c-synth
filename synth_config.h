#ifndef SYNTH_CONFIG_H
#define SYNTH_CONFIG_H

#include "fm.h"

#define SYNTH_MAX_LAYERS 8

typedef struct {
    FmConfig fm;
    double gain;        /* 0..1, mixed with normalization by layer count. */
    double detuneCents; /* -4800..4800; shifts the whole layer's FM chain. */
} SynthLayerConfig;

typedef struct {
    int attackMs;
    int decayMs;
    int sustainPercent;
    int releaseMs;
} SynthEnvelopeConfig;

typedef struct {
    SynthEnvelopeConfig outputEnvelope; /* Shared amplitude ADSR for every note. */
    int layerCount;
    SynthLayerConfig layers[SYNTH_MAX_LAYERS];
} SynthConfig;

SynthConfig synthDefaultConfig(void);
#define SYNTH_PRESET_COUNT 12
const char *synthPresetName(int index);
SynthConfig synthPresetConfig(int index);
bool synthConfigValid(const SynthConfig *config);

#endif
