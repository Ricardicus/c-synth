#include "fm.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#define CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition); \
        exit(1); \
    } \
} while (0)

#define TAU 6.28318530717958647692

static void advance(FmSynth *synth, int samples)
{
    for (int i = 0; i < samples; ++i)
        CHECK(isfinite(fmNextSample(synth, 10.0)));
}

static void testFm(void)
{
    FmConfig config = fmDefaultConfig();
    FmSynth synth;
    config.operators[0].rm = 0;
    CHECK(fmInit(&synth, 48000, &config) == 0);
    fmNoteOn(&synth, true);
    for (int i = 0; i < 48000; ++i)
        CHECK(fabs(fmNextSample(&synth, 440) - sin(TAU * 440 * i / 48000)) < 1e-6);

    config.operators[0].rm = 4;
    config.operators[0].ratio = 2;
    CHECK(fmInit(&synth, 48000, &config) == 0);
    fmNoteOn(&synth, true);
    double carrierPhase = 0;
    bool wentNegative = false;
    /* Independent discrete FM reference: integrate signed frequency, not
     * phase-offset modulation. Also verifies pitch-relative OP1 frequency. */
    for (int i = 0; i < 4800; ++i) {
        double instantaneousHz = 440 + 4 * 880 * sin(TAU * 880 * i / 48000);
        wentNegative |= instantaneousHz < 0;
        float sample = fmNextSample(&synth, 440);
        CHECK(fabs(sample - sin(carrierPhase)) < 2e-5);
        CHECK(sample >= -1 && sample <= 1);
        carrierPhase = fmod(carrierPhase + TAU * instantaneousHz / 48000, TAU);
    }
    CHECK(wentNegative);
    CHECK(fabs(synth.operators[0].oscillator.phase - fmod(TAU * 880 * 4800 / 48000, TAU)) < 1e-9 ||
          fabs(synth.operators[0].oscillator.phase - TAU) < 1e-9);

    FmSynth other;
    CHECK(fmInit(&other, 48000, &config) == 0);
    fmNoteOn(&other, true);
    CHECK(fmNextSample(&other, 440) == 0);
    CHECK(other.operators[0].oscillator.phase != synth.operators[0].oscillator.phase);
    double phase = synth.operators[1].oscillator.phase;
    CHECK(fmNextSample(&synth, NAN) == 0);
    CHECK(fmNextSample(&synth, -440) == 0);
    CHECK(synth.operators[1].oscillator.phase == phase);
}

static void testDecay(void)
{
    FmConfig config = fmDefaultConfig();
    config.operators[0].indexMode = FM_INDEX_DECAY;
    config.operators[0].decayRate = 2;
    FmSynth synth;
    CHECK(fmInit(&synth, 1000, &config) == 0);
    fmNoteOn(&synth, true);
    advance(&synth, 1000);
    CHECK(fabs(synth.operators[0].indexEnvelope - exp(-2)) < 1e-12);
    fmNoteOff(&synth);
    fmNoteOn(&synth, false);
    CHECK(synth.operators[0].indexEnvelope == 1);
    config.operators[0].decayRate = 0;
    CHECK(fmInit(&synth, 1000, &config) == 0);
    fmNoteOn(&synth, true);
    advance(&synth, 1000);
    CHECK(synth.operators[0].indexEnvelope == 1);
}

static void testAdsr(void)
{
    FmConfig config = fmDefaultConfig();
    config.operators[0].indexMode = FM_INDEX_ADSR;
    config.operators[0].attackMs = 10;
    config.operators[0].decayMs = 20;
    config.operators[0].sustainPercent = 25;
    config.operators[0].releaseMs = 30;
    FmSynth synth;
    CHECK(fmInit(&synth, 1000, &config) == 0);
    fmNoteOn(&synth, true);
    CHECK(synth.operators[0].indexEnvelope == 0);
    advance(&synth, 5);
    CHECK(fabs(synth.operators[0].indexEnvelope - 0.5) < 1e-12);
    fmNoteOn(&synth, true); /* Repeated registration must not retrigger. */
    CHECK(fabs(synth.operators[0].indexEnvelope - 0.5) < 1e-12);
    advance(&synth, 5);
    CHECK(synth.operators[0].indexEnvelope == 1 && synth.operators[0].stage == FM_ENV_DECAY);
    advance(&synth, 20);
    CHECK(synth.operators[0].indexEnvelope == 0.25 && synth.operators[0].stage == FM_ENV_SUSTAIN);
    advance(&synth, 100);
    CHECK(synth.operators[0].indexEnvelope == 0.25);
    fmNoteOff(&synth);
    advance(&synth, 15);
    CHECK(fabs(synth.operators[0].indexEnvelope - 0.125) < 1e-12);
    fmNoteOff(&synth); /* Repeated release must not restart the tail. */
    advance(&synth, 15);
    CHECK(synth.operators[0].indexEnvelope == 0 && synth.operators[0].stage == FM_ENV_IDLE);

    fmNoteOn(&synth, true);
    advance(&synth, 5);
    fmNoteOff(&synth); /* Release begins at the current attack level. */
    advance(&synth, 15);
    CHECK(fabs(synth.operators[0].indexEnvelope - 0.25) < 1e-12);
    double phase = synth.operators[1].oscillator.phase;
    fmNoteOn(&synth, false);
    CHECK(synth.operators[1].oscillator.phase == phase);
    CHECK(fabs(synth.operators[0].indexEnvelope - 0.25) < 1e-12);
    advance(&synth, 10);
    CHECK(synth.operators[0].indexEnvelope == 1);

    config.operators[0].attackMs = config.operators[0].decayMs = config.operators[0].releaseMs = 0;
    CHECK(fmInit(&synth, 1000, &config) == 0);
    fmNoteOn(&synth, true);
    CHECK(synth.operators[0].indexEnvelope == 0.25 && synth.operators[0].stage == FM_ENV_SUSTAIN);
    fmNoteOff(&synth);
    CHECK(synth.operators[0].indexEnvelope == 0 && synth.operators[0].stage == FM_ENV_IDLE);
}

static void testOscillator(void)
{
    Oscillator oscillator;
    oscillatorInit(&oscillator, 4);
    CHECK(oscillatorNextSample(&oscillator, 1) == 0);
    CHECK(fabs(oscillatorNextSample(&oscillator, 0) - 1) < 1e-6);
    CHECK(fabs(oscillatorNextSample(&oscillator, -1) - 1) < 1e-6);
    CHECK(oscillatorNextSample(&oscillator, -1) == 0);
    CHECK(fabs(oscillatorNextSample(&oscillator, 1) + 1) < 1e-6);
    oscillatorInit(&oscillator, 0);
    CHECK(oscillatorNextSample(&oscillator, 1) == 0);
}

static void testWaveforms(void)
{
    const float expected[][4] = {
        {0, 1, 0, -1},       /* sine */
        {1, 1, -1, -1},      /* square */
        {0, 1, 0, -1},       /* triangle */
        {-1, -0.5, 0, 0.5},  /* sawtooth */
        {1, -1, -1, -1}      /* 25% pulse */
    };
    for (int wave = WAVE_SINE; wave <= WAVE_PULSE; ++wave) {
        Oscillator oscillator;
        oscillatorInit(&oscillator, 4);
        CHECK(oscillatorSetWaveform(&oscillator, (Waveform)wave) == 0);
        for (int i = 0; i < 8; ++i)
            CHECK(fabs(oscillatorNextSample(&oscillator, 1) - expected[wave][i % 4]) < 1e-6);
        for (int i = 0; i < 8; ++i) {
            float sample = oscillatorNextSample(&oscillator, -1);
            CHECK(fabs(sample - expected[wave][(4 - i % 4) % 4]) < 1e-6);
        }

        FmConfig config = fmDefaultConfig();
        config.operatorCount = 1;
        config.operators[0].waveform = (Waveform)wave;
        FmSynth synth;
        CHECK(fmInit(&synth, 4, &config) == 0);
        fmNoteOn(&synth, true);
        for (int i = 0; i < 4; ++i)
            CHECK(fabs(fmNextSample(&synth, 1) - expected[wave][i]) < 1e-6);
        fmNoteOff(&synth);
        fmNoteOn(&synth, true);
        CHECK(synth.operators[0].oscillator.waveform == (Waveform)wave);
        CHECK(fabs(fmNextSample(&synth, 1) - expected[wave][0]) < 1e-6);
    }

    Oscillator oscillator;
    oscillatorInit(&oscillator, 4);
    CHECK(oscillatorSetWaveform(&oscillator, WAVE_PULSE) == 0);
    CHECK(oscillatorSetPulseWidth(&oscillator, 0.5) == 0);
    for (int i = 0; i < 4; ++i)
        CHECK(oscillatorNextSample(&oscillator, 1) == expected[WAVE_SQUARE][i]);
    oscillatorNextSample(&oscillator, 1);
    double phase = oscillator.phase;
    CHECK(oscillatorSetWaveform(&oscillator, WAVE_TRIANGLE) == 0);
    CHECK(oscillator.phase == phase);
    CHECK(oscillatorSetWaveform(&oscillator, (Waveform)99) == -1);
    CHECK(oscillator.waveform == WAVE_TRIANGLE);
    CHECK(oscillatorSetPulseWidth(&oscillator, 0) == -1);
    CHECK(oscillatorSetPulseWidth(&oscillator, 1) == -1);
    CHECK(oscillatorSetPulseWidth(&oscillator, NAN) == -1);
    CHECK(oscillator.pulseWidth == 0.5);

    /* Square OP1 drives signed instantaneous frequency of sawtooth OP2.
     * At 8 Hz sample rate it advances 2 Hz for four samples, then holds. */
    FmConfig config = fmDefaultConfig();
    config.operators[0].waveform = WAVE_SQUARE;
    config.operators[0].rm = 1;
    config.operators[1].waveform = WAVE_SAWTOOTH;
    FmSynth synth;
    CHECK(fmInit(&synth, 8, &config) == 0);
    fmNoteOn(&synth, true);
    const float mixed[] = {-1, -0.5, 0, 0.5, -1, -1, -1, -1};
    for (int i = 0; i < 8; ++i)
        CHECK(fabs(fmNextSample(&synth, 1) - mixed[i]) < 1e-6);
    fmNoteOff(&synth);
    fmNoteOn(&synth, true);
    CHECK(synth.operators[0].oscillator.waveform == WAVE_SQUARE);
    CHECK(synth.operators[1].oscillator.waveform == WAVE_SAWTOOTH);

    config.operators[0].waveform = WAVE_PULSE;
    config.operators[0].pulseWidth = 0.75;
    CHECK(fmInit(&synth, 8, &config) == 0);
    fmNoteOn(&synth, true);
    CHECK(synth.operators[0].oscillator.pulseWidth == 0.75);
    fmNoteOff(&synth);
    fmNoteOn(&synth, true);
    CHECK(synth.operators[0].oscillator.pulseWidth == 0.75);
    config.operators[0].pulseWidth = 0;
    CHECK(fmInit(&synth, 8, &config) == -1);
    config.operators[0].pulseWidth = 0.5;
    config.operators[0].waveform = (Waveform)99;
    CHECK(fmInit(&synth, 8, &config) == -1);
}

static void testVibrato(void)
{
    Oscillator oscillator;
    oscillatorInit(&oscillator, 1000);
    CHECK(oscillatorSetVibrato(&oscillator, 5, 30) == 0);
    double phase = 0;
    for (int i = 0; i < 2000; ++i) {
        CHECK(fabs(oscillatorNextSample(&oscillator, 100) - sin(phase)) < 1e-5);
        double hz = 100 * exp2(30 * sin(TAU * 5 * i / 1000) / 1200);
        phase = fmod(phase + TAU * hz / 1000, TAU);
    }
    CHECK(oscillatorSetVibrato(&oscillator, -1, 30) == -1);
    CHECK(oscillatorSetVibrato(&oscillator, NAN, 30) == -1);
    CHECK(oscillatorSetVibrato(&oscillator, 5, -1) == -1);
    CHECK(oscillatorSetVibrato(&oscillator, 5, 1201) == -1);
    CHECK(oscillator.vibratoRateHz == 5 && oscillator.vibratoDepthCents == 30);

    FmConfig config = fmDefaultConfig();
    config.operatorCount = 1;
    config.operators[0].vibratoRateHz = 7;
    config.operators[0].vibratoDepthCents = 20;
    FmSynth synth;
    CHECK(fmInit(&synth, 1000, &config) == 0);
    fmNoteOn(&synth, true);
    advance(&synth, 10);
    double vibratoPhase = synth.operators[0].oscillator.vibratoPhase;
    CHECK(vibratoPhase > 0);
    fmNoteOff(&synth);
    fmNoteOn(&synth, false);
    CHECK(synth.operators[0].oscillator.vibratoPhase == vibratoPhase);
    fmNoteOff(&synth);
    fmNoteOn(&synth, true);
    CHECK(synth.operators[0].oscillator.vibratoPhase == 0);
    CHECK(synth.operators[0].oscillator.vibratoRateHz == 7);
    CHECK(synth.operators[0].oscillator.vibratoDepthCents == 20);
}

static void testChains(void)
{
    for (int count = 1; count <= FM_MAX_OPERATORS; ++count) {
        FmConfig config = fmDefaultConfig();
        config.operatorCount = count;
        for (int i = 0; i < count; ++i) {
            config.operators[i].ratio = 0.5 + i * 0.25;
            config.operators[i].rm = 1.5 + i * 0.1;
        }
        FmSynth synth;
        CHECK(fmInit(&synth, 48000, &config) == 0);
        fmNoteOn(&synth, true);
        double phase[FM_MAX_OPERATORS] = {0};
        for (int sampleIndex = 0; sampleIndex < 3000; ++sampleIndex) {
            float previous = 0;
            for (int i = 0; i < count; ++i) {
                double hz = 220 * config.operators[i].ratio;
                if (i > 0)
                    hz += config.operators[i - 1].rm *
                          (220 * config.operators[i - 1].ratio) * previous;
                previous = (float)sin(phase[i]);
                phase[i] = fmod(phase[i] + TAU * hz / 48000, TAU);
            }
            CHECK(fabs(fmNextSample(&synth, 220) - previous) < 1e-5);
        }

        /* No signal can cross a zero-depth final modulation link. */
        if (count > 1)
            config.operators[count - 2].rm = 0;
        CHECK(fmInit(&synth, 48000, &config) == 0);
        fmNoteOn(&synth, true);
        for (int i = 0; i < 3000; ++i) {
            double carrierHz = 220 * config.operators[count - 1].ratio;
            CHECK(fabs(fmNextSample(&synth, 220) - sin(TAU * carrierHz * i / 48000)) < 1e-6);
        }
    }
}

static void testIndependentEnvelopes(void)
{
    FmConfig config = fmDefaultConfig();
    config.operatorCount = 3;
    config.operators[0].indexMode = FM_INDEX_DECAY;
    config.operators[0].decayRate = 2;
    config.operators[1].indexMode = FM_INDEX_ADSR;
    config.operators[1].attackMs = 10;
    config.operators[1].decayMs = 20;
    config.operators[1].sustainPercent = 25;
    config.operators[1].releaseMs = 30;
    FmSynth synth;
    CHECK(fmInit(&synth, 1000, &config) == 0);
    fmNoteOn(&synth, true);
    advance(&synth, 30);
    CHECK(fabs(synth.operators[0].indexEnvelope - exp(-0.06)) < 1e-12);
    CHECK(synth.operators[1].indexEnvelope == 0.25);
    CHECK(fmReleaseDurationMs(&synth) == 30);
    fmNoteOff(&synth);
    advance(&synth, 30);
    CHECK(fabs(synth.operators[0].indexEnvelope - exp(-0.12)) < 1e-12);
    CHECK(synth.operators[1].indexEnvelope == 0);

    config.operators[0].indexMode = FM_INDEX_ADSR;
    config.operators[0].releaseMs = 120;
    config.operators[2].indexMode = FM_INDEX_ADSR;
    config.operators[2].releaseMs = 999; /* Carrier's index is unused. */
    CHECK(fmInit(&synth, 1000, &config) == 0);
    CHECK(fmReleaseDurationMs(&synth) == 120);
    fmNoteOn(&synth, true);
    advance(&synth, 40);
    fmNoteOff(&synth);
    advance(&synth, 30);
    CHECK(synth.operators[0].indexEnvelope > 0);
    CHECK(synth.operators[1].indexEnvelope == 0);
    advance(&synth, 90);
    CHECK(synth.operators[0].indexEnvelope == 0);
}

static void testNoise(void)
{
    Oscillator a, b;
    oscillatorInit(&a, 48000); oscillatorInit(&b, 48000);
    CHECK(oscillatorSetWaveform(&a, WAVE_NOISE) == 0);
    CHECK(oscillatorSetWaveform(&b, WAVE_NOISE) == 0);
    b.noiseState += 1;
    double sum = 0, energy = 0, cross = 0;
    for (int i = 0; i < 48000; ++i) {
        double x = oscillatorNextSample(&a, 440), y = oscillatorNextSample(&b, 440);
        CHECK(isfinite(x) && x >= -1 && x <= 1);
        sum += x; energy += x*x; cross += x*y;
    }
    CHECK(fabs(sum / 48000) < .015);
    CHECK(energy / 48000 > .31 && energy / 48000 < .35);
    CHECK(fabs(cross / 48000) < .02);
    Waveform wave;
    CHECK(oscillatorParseWaveform("noise", &wave) && wave == WAVE_NOISE);
}

int main(void)
{
    testNoise();
    testFm();
    testDecay();
    testAdsr();
    testOscillator();
    testWaveforms();
    testVibrato();
    testChains();
    testIndependentEnvelopes();
    FmConfig config = fmDefaultConfig();
    FmSynth synth;
    CHECK(fmInit(&synth, 0, &config) == -1);
    CHECK(fmInit(&synth, 48000, NULL) == -1);
    config.operators[0].rm = NAN;
    CHECK(fmInit(&synth, 48000, &config) == -1);
    config = fmDefaultConfig();
    config.operatorCount = 0;
    CHECK(fmInit(&synth, 48000, &config) == -1);
    config.operatorCount = FM_MAX_OPERATORS + 1;
    CHECK(fmInit(&synth, 48000, &config) == -1);
    config.operatorCount = FM_MAX_OPERATORS;
    config.operators[FM_MAX_OPERATORS - 1].ratio = 0;
    CHECK(fmInit(&synth, 48000, &config) == -1);
    puts("FM, signed phase integration, envelope timing, and voice-state checks passed.");
    return 0;
}
