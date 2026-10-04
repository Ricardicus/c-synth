#include "fm.h"

#include <math.h>
#include <stddef.h>

FmConfig fmDefaultConfig(void)
{
    FmConfig config = {.operatorCount = 2};
    for (int i = 0; i < FM_MAX_OPERATORS; ++i) {
        config.operators[i] = (FmOperatorConfig){
            .waveform = WAVE_SINE, .pulseWidth = 0.25,
            .vibratoRateHz = 5.0, .vibratoDepthCents = 0.0,
            .rm = 2.0, .ratio = 1.0, .indexMode = FM_INDEX_SUSTAIN,
            .decayRate = 2.0, .attackMs = 10, .decayMs = 200,
            .sustainPercent = 50, .releaseMs = 300
        };
    }
    return config;
}

static bool operatorConfigValid(const FmOperatorConfig *config)
{
    return config->waveform >= WAVE_SINE && config->waveform < WAVE_COUNT &&
           isfinite(config->pulseWidth) && config->pulseWidth > 0.0 && config->pulseWidth < 1.0 &&
           isfinite(config->vibratoRateHz) && config->vibratoRateHz >= 0.0 &&
           isfinite(config->vibratoDepthCents) && config->vibratoDepthCents >= 0.0 &&
           config->vibratoDepthCents <= 1200.0 &&
           isfinite(config->rm) && config->rm >= 0.0 &&
           isfinite(config->ratio) && config->ratio > 0.0 &&
           config->indexMode >= FM_INDEX_SUSTAIN &&
           config->indexMode <= FM_INDEX_ADSR &&
           isfinite(config->decayRate) && config->decayRate >= 0.0 &&
           config->attackMs >= 0 && config->decayMs >= 0 &&
           config->sustainPercent >= 0 && config->sustainPercent <= 100 &&
           config->releaseMs >= 0;
}

bool fmConfigValid(const FmConfig *config)
{
    if (config == NULL || config->operatorCount < 1 ||
        config->operatorCount > FM_MAX_OPERATORS)
        return false;
    for (int i = 0; i < config->operatorCount; ++i)
        if (!operatorConfigValid(&config->operators[i]))
            return false;
    return true;
}

int fmInit(FmSynth *synth, double sampleRate, const FmConfig *config)
{
    if (!fmConfigValid(config) || !isfinite(sampleRate) || sampleRate <= 0.0)
        return -1;
    *synth = (FmSynth){.operatorCount = config->operatorCount};
    for (int i = 0; i < synth->operatorCount; ++i) {
        FmOperator *op = &synth->operators[i];
        op->config = config->operators[i];
        op->decayMultiplier = exp(-op->config.decayRate / sampleRate);
        oscillatorInit(&op->oscillator, sampleRate);
        oscillatorSetWaveform(&op->oscillator, op->config.waveform);
        oscillatorSetPulseWidth(&op->oscillator, op->config.pulseWidth);
        oscillatorSetVibrato(&op->oscillator, op->config.vibratoRateHz,
                            op->config.vibratoDepthCents);
    }
    return 0;
}

static void startStage(FmOperator *op, FmEnvelopeStage stage)
{
    op->stage = stage;
    op->stageStart = op->indexEnvelope;
    op->stagePosition = 0.0;
}

/* Resolve zero-duration ADSR stages without spending a sample in them. */
static void skipInstantStages(FmOperator *op)
{
    if (op->stage == FM_ENV_ATTACK && op->config.attackMs == 0) {
        op->indexEnvelope = 1.0;
        startStage(op, FM_ENV_DECAY);
    }
    if (op->stage == FM_ENV_DECAY && op->config.decayMs == 0) {
        op->indexEnvelope = op->config.sustainPercent / 100.0;
        startStage(op, FM_ENV_SUSTAIN);
    }
    if (op->stage == FM_ENV_RELEASE && op->config.releaseMs == 0) {
        op->indexEnvelope = 0.0;
        startStage(op, FM_ENV_IDLE);
    }
}

void fmNoteOn(FmSynth *synth, bool resetPhases)
{
    if (synth->held)
        return;
    synth->held = true;
    for (int i = 0; i < synth->operatorCount - 1; ++i) {
        FmOperator *op = &synth->operators[i];
        if (resetPhases)
            op->indexEnvelope = 0.0;
        if (op->config.indexMode == FM_INDEX_ADSR) {
            startStage(op, FM_ENV_ATTACK);
            skipInstantStages(op);
        } else {
            op->indexEnvelope = 1.0;
            startStage(op, FM_ENV_SUSTAIN);
        }
    }
    if (resetPhases) {
        for (int i = 0; i < synth->operatorCount; ++i) {
            synth->operators[i].oscillator.phase = 0.0;
            synth->operators[i].oscillator.vibratoPhase = 0.0;
        }
    }
}

void fmNoteOff(FmSynth *synth)
{
    if (!synth->held)
        return;
    synth->held = false;
    for (int i = 0; i < synth->operatorCount - 1; ++i) {
        FmOperator *op = &synth->operators[i];
        if (op->config.indexMode == FM_INDEX_ADSR) {
            startStage(op, FM_ENV_RELEASE);
            skipInstantStages(op);
        }
    }
}

int fmReleaseDurationMs(const FmSynth *synth)
{
    int duration = 0;
    for (int i = 0; i < synth->operatorCount - 1; ++i) {
        const FmOperatorConfig *config = &synth->operators[i].config;
        if (config->indexMode == FM_INDEX_ADSR && config->releaseMs > duration)
            duration = config->releaseMs;
    }
    return duration;
}

static void advanceIndexEnvelope(FmOperator *op)
{
    if (op->stage == FM_ENV_IDLE)
        return;
    if (op->config.indexMode == FM_INDEX_DECAY) {
        op->indexEnvelope *= op->decayMultiplier;
        if (op->indexEnvelope < 1e-12)
            op->indexEnvelope = 0.0;
        return;
    }
    if (op->config.indexMode != FM_INDEX_ADSR)
        return;

    int durationMs;
    double target;
    FmEnvelopeStage next;
    switch (op->stage) {
    case FM_ENV_ATTACK:
        durationMs = op->config.attackMs;
        target = 1.0;
        next = FM_ENV_DECAY;
        break;
    case FM_ENV_DECAY:
        durationMs = op->config.decayMs;
        target = op->config.sustainPercent / 100.0;
        next = FM_ENV_SUSTAIN;
        break;
    case FM_ENV_RELEASE:
        durationMs = op->config.releaseMs;
        target = 0.0;
        next = FM_ENV_IDLE;
        break;
    default:
        return;
    }
    double durationSamples = fmax(1.0, ceil(durationMs * op->oscillator.sampleRate / 1000.0));
    double progress = fmin(1.0, ++op->stagePosition / durationSamples);
    op->indexEnvelope = op->stageStart + (target - op->stageStart) * progress;
    if (progress == 1.0) {
        startStage(op, next);
        skipInstantStages(op);
    }
}

float fmNextSample(FmSynth *synth, double baseFrequencyHz)
{
    if (!isfinite(baseFrequencyHz) || baseFrequencyHz <= 0.0)
        return 0.0f;

    /* Validate all frequencies before advancing any operator's state. */
    double frequencies[FM_MAX_OPERATORS];
    double deviations[FM_MAX_OPERATORS];
    for (int i = 0; i < synth->operatorCount; ++i) {
        const FmOperator *op = &synth->operators[i];
        frequencies[i] = baseFrequencyHz * op->config.ratio;
        deviations[i] = i < synth->operatorCount - 1
                      ? op->config.rm * op->indexEnvelope * frequencies[i] : 0.0;
        if (!isfinite(frequencies[i]) || !isfinite(deviations[i]))
            return 0.0f;
        if (i > 0 && !isfinite(frequencies[i] + deviations[i - 1]))
            return 0.0f;
    }

    float sample = 0.0f;
    for (int i = 0; i < synth->operatorCount; ++i) {
        FmOperator *op = &synth->operators[i];
        double instantaneousHz = frequencies[i];
        if (i > 0)
            instantaneousHz += deviations[i - 1] * sample;
        /* Each operator integrates signed frequency into its own phase.
         * Its current output modulates the next operator in this sample. */
        sample = oscillatorNextSample(&op->oscillator, instantaneousHz);
        if (i < synth->operatorCount - 1)
            advanceIndexEnvelope(op);
    }
    return sample;
}
