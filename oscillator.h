#ifndef OSCILLATOR_H
#define OSCILLATOR_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    WAVE_SINE,
    WAVE_SQUARE,
    WAVE_TRIANGLE,
    WAVE_SAWTOOTH,
    WAVE_PULSE,
    WAVE_NOISE,
    WAVE_COUNT
} Waveform;

const char *oscillatorWaveformName(Waveform waveform);
bool oscillatorParseWaveform(const char *name, Waveform *waveform);

/* One instance per voice. No SDL dependency; owned by the rendering thread. */
typedef struct {
    uint32_t noiseState;
    double phase;
    double sampleRate;
    Waveform waveform;
    double pulseWidth; /* Fraction of cycle high; pulse only. */
    double vibratoPhase;
    double vibratoRateHz;
    double vibratoDepthCents;
} Oscillator;

/* Initialize/reset with the audio device's sample rate in Hz, sine waveform,
 * and 25% pulse width. Setters preserve phase; return 0 or -1 if invalid. */
void oscillatorInit(Oscillator *oscillator, double sampleRate);
int oscillatorSetWaveform(Oscillator *oscillator, Waveform waveform);
int oscillatorSetPulseWidth(Oscillator *oscillator, double pulseWidth);
/* Rate >= 0 Hz, depth 0..1200 cents. Defaults: 5 Hz, depth 0 (off). */
int oscillatorSetVibrato(Oscillator *oscillator, double rateHz, double depthCents);

/* Generate one sample in [-1, 1] and advance this oscillator's phase.
 * frequencyHz may change between calls without resetting phase.
 * Signed frequencies are supported for FM: negative values run phase
 * backwards, and zero holds phase. Nonfinite frequencies or invalid
 * (nonpositive/nonfinite) sample rates produce silence.
 * Call once per audio sample, from the thread that owns the instance. */
float oscillatorNextSample(Oscillator *oscillator, double frequencyHz);

#endif
