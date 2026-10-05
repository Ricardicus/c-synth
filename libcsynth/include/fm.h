#ifndef FM_H
#define FM_H

#include "oscillator.h"
#include <stdbool.h>

#define FM_MAX_OPERATORS 8

typedef enum {
    FM_INDEX_SUSTAIN,
    FM_INDEX_DECAY,
    FM_INDEX_ADSR
} FmIndexMode;

typedef struct {
    Waveform waveform;
    double pulseWidth;      /* Pulse duty cycle, strictly between 0 and 1. */
    double vibratoRateHz;
    double vibratoDepthCents;
    double rm;             /* Depth into next OP: deviation / nominal Hz. */
    double ratio;          /* Operator's nominal Hz = base Hz * ratio. */
    FmIndexMode indexMode;
    double decayRate;      /* Exponential index decay rate, per second. */
    int attackMs;
    int decayMs;
    int sustainPercent;
    int releaseMs;
} FmOperatorConfig;

typedef struct {
    int operatorCount; /* OP1 -> OP2 -> ... -> OPn (the output carrier). */
    /* Ratio/waveform/pulse width apply to the carrier; its index is unused. */
    FmOperatorConfig operators[FM_MAX_OPERATORS];
} FmConfig;

typedef enum {
    FM_ENV_IDLE,
    FM_ENV_ATTACK,
    FM_ENV_DECAY,
    FM_ENV_SUSTAIN,
    FM_ENV_RELEASE
} FmEnvelopeStage;

/* One instance per voice. All instance operations belong to its rendering
 * thread, or must run while that thread is locked. No SDL dependency. */
typedef struct {
    Oscillator oscillator;
    FmOperatorConfig config;
    FmEnvelopeStage stage;
    double indexEnvelope;
    double stageStart;
    double stagePosition;
    double decayMultiplier;
} FmOperator;

typedef struct {
    int operatorCount;
    FmOperator operators[FM_MAX_OPERATORS];
    bool held;
} FmSynth;

FmConfig fmDefaultConfig(void);
bool fmConfigValid(const FmConfig *config);
/* Returns 0 on success, -1 for invalid configuration or sample rate. */
int fmInit(FmSynth *synth, double sampleRate, const FmConfig *config);
/* Reset phases for a fresh voice; retain them when retriggering a tail. */
void fmNoteOn(FmSynth *synth, bool resetPhases);
void fmNoteOff(FmSynth *synth);
/* Longest modulator ADSR release; caller keeps the amplitude tail audible. */
int fmReleaseDurationMs(const FmSynth *synth);
/* The sound-generation entry point: base Hz in, one sample in [-1,1] out.
 * Call once per sample after fmNoteOn. Index envelopes change timbre only;
 * the caller applies the output amplitude envelope and mixes voices. */
float fmNextSample(FmSynth *synth, double baseFrequencyHz);

#endif
