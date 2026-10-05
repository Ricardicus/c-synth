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
    double echoMix;       /* 0..1 */
    double echoDelayMs;   /* 1..2000 */
    double echoFeedback;  /* 0..0.95 */
    double reverbMix;     /* 0..1 */
    double reverbRoom;    /* 0..0.95, feedback amount */
    double reverbDamping; /* 0..1, high-frequency absorption */
} SynthEffectsConfig;

typedef struct {
    SynthEffectsConfig effects;
    SynthEnvelopeConfig outputEnvelope; /* Shared amplitude ADSR for every note. */
    int layerCount;
    SynthLayerConfig layers[SYNTH_MAX_LAYERS];
} SynthConfig;

SynthConfig synthDefaultConfig(void);
#define SYNTH_PRESET_COUNT 64
const char *synthPresetName(int index);
SynthConfig synthPresetConfig(int index);
bool synthEffectsConfigValid(const SynthEffectsConfig *config);
bool synthConfigValid(const SynthConfig *config);

#endif
