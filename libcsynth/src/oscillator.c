#include "oscillator.h"

#include <math.h>
#include <string.h>

#define TAU 6.28318530717958647692

const char *oscillatorWaveformName(Waveform waveform)
{
    switch (waveform) {
    case WAVE_SINE: return "sine";
    case WAVE_SQUARE: return "square";
    case WAVE_TRIANGLE: return "triangle";
    case WAVE_SAWTOOTH: return "sawtooth";
    case WAVE_PULSE: return "pulse";
    case WAVE_NOISE: return "noise";
    default: return "unknown";
    }
}

bool oscillatorParseWaveform(const char *name, Waveform *waveform)
{
    if (strcmp(name, "saw") == 0) {
        *waveform = WAVE_SAWTOOTH;
        return true;
    }
    for (int i = WAVE_SINE; i < WAVE_COUNT; ++i) {
        if (strcmp(name, oscillatorWaveformName((Waveform)i)) == 0) {
            *waveform = (Waveform)i;
            return true;
        }
    }
    return false;
}

void oscillatorInit(Oscillator *oscillator, double sampleRate)
{
    oscillator->noiseState = 0x6d2b79f5u;
    oscillator->phase = 0.0;
    oscillator->sampleRate = sampleRate;
    oscillator->waveform = WAVE_SINE;
    oscillator->pulseWidth = 0.25;
    oscillator->vibratoPhase = 0.0;
    oscillator->vibratoRateHz = 5.0;
    oscillator->vibratoDepthCents = 0.0;
}

int oscillatorSetVibrato(Oscillator *oscillator, double rateHz, double depthCents)
{
    if (!isfinite(rateHz) || rateHz < 0.0 || !isfinite(depthCents) ||
        depthCents < 0.0 || depthCents > 1200.0)
        return -1;
    oscillator->vibratoRateHz = rateHz;
    oscillator->vibratoDepthCents = depthCents;
    return 0;
}

int oscillatorSetWaveform(Oscillator *oscillator, Waveform waveform)
{
    if (waveform < WAVE_SINE || waveform >= WAVE_COUNT)
        return -1;
    oscillator->waveform = waveform;
    return 0;
}

int oscillatorSetPulseWidth(Oscillator *oscillator, double pulseWidth)
{
    if (!isfinite(pulseWidth) || pulseWidth <= 0.0 || pulseWidth >= 1.0)
        return -1;
    oscillator->pulseWidth = pulseWidth;
    return 0;
}

float oscillatorNextSample(Oscillator *oscillator, double frequencyHz)
{
    if (!isfinite(frequencyHz) ||
        !isfinite(oscillator->sampleRate) || oscillator->sampleRate <= 0.0)
        return 0.0f;

    if (oscillator->vibratoDepthCents != 0.0)
        frequencyHz *= exp2(oscillator->vibratoDepthCents *
                            sin(oscillator->vibratoPhase) / 1200.0);
    if (!isfinite(frequencyHz))
        return 0.0f;

    const double cycle = oscillator->phase / TAU;
    float sample;
    switch (oscillator->waveform) {
    case WAVE_SINE:
        sample = (float)sin(oscillator->phase);
        break;
    case WAVE_SQUARE:
        sample = cycle < 0.5 ? 1.0f : -1.0f;
        break;
    case WAVE_TRIANGLE:
        sample = (float)(1.0 - 4.0 * fabs(fmod(cycle + 0.25, 1.0) - 0.5));
        break;
    case WAVE_SAWTOOTH:
        sample = (float)(2.0 * cycle - 1.0);
        break;
    case WAVE_PULSE:
        sample = cycle < oscillator->pulseWidth ? 1.0f : -1.0f;
        break;
    case WAVE_NOISE: {
        uint32_t state = oscillator->noiseState ? oscillator->noiseState : 0x6d2b79f5u;
        state ^= state << 13; state ^= state >> 17; state ^= state << 5;
        oscillator->noiseState = state;
        sample = (float)((state >> 8) / 8388607.5 - 1.0);
        break;
    }
    default:
        return 0.0f;
    }
    double increment = TAU * (fmod(frequencyHz, oscillator->sampleRate) /
                              oscillator->sampleRate);
    oscillator->phase += increment;
    if (oscillator->phase >= TAU)
        oscillator->phase -= TAU;
    else if (oscillator->phase < 0.0)
        oscillator->phase += TAU;
    oscillator->vibratoPhase += TAU *
        (fmod(oscillator->vibratoRateHz, oscillator->sampleRate) / oscillator->sampleRate);
    if (oscillator->vibratoPhase >= TAU)
        oscillator->vibratoPhase -= TAU;
    return sample;
}
