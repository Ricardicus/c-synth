#ifndef ENVELOPE_H
#define ENVELOPE_H
#include "synth_config.h"

typedef struct {
    SynthEnvelopeConfig config;
    FmEnvelopeStage stage;
    double level;
    double start;
    double position;
} OutputEnvelope;

void outputEnvelopeConfigure(OutputEnvelope *env, SynthEnvelopeConfig config);
void outputEnvelopeOn(OutputEnvelope *env);
void outputEnvelopeOff(OutputEnvelope *env);
double outputEnvelopeNext(OutputEnvelope *env, double sampleRate);
#endif
