#include "effects.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

static int delayInit(EffectDelay *delay, int length)
{
    delay->length = length;
    delay->buffer = calloc((size_t)length, sizeof(float));
    return delay->buffer ? 0 : -1;
}
void effectsDestroy(SynthEffects *e)
{
    free(e->echo.buffer);
    for (int i = 0; i < 6; ++i) free(e->combs[i].buffer);
    for (int i = 0; i < 2; ++i) free(e->allpasses[i].buffer);
    memset(e, 0, sizeof(*e));
}
int effectsInit(SynthEffects *e, double rate, SynthEffectsConfig config)
{
    if (!isfinite(rate) || rate < 1000 || rate > 384000 || !synthEffectsConfigValid(&config)) return -1;
    *e = (SynthEffects){.sampleRate = rate, .current = config, .target = config,
                       .smoothing = 1 - exp(-1 / (.02 * rate))};
    if (delayInit(&e->echo, (int)ceil(2 * rate) + 2)) goto fail;
    static const double times[6] = {.0297,.0371,.0411,.0437,.0479,.0531};
    for (int i = 0; i < 6; ++i)
        if (delayInit(&e->combs[i], (int)ceil(times[i] * rate))) goto fail;
    for (int i = 0; i < 2; ++i)
        if (delayInit(&e->allpasses[i], (int)ceil((i ? .0017 : .005) * rate))) goto fail;
    return 0;
fail:
    effectsDestroy(e);
    return -1;
}
void effectsConfigure(SynthEffects *e, SynthEffectsConfig config) { e->target = config; }
static void writeDelay(EffectDelay *d, double value)
{
    d->buffer[d->cursor] = (float)value;
    d->cursor = (d->cursor + 1) % d->length;
}
float effectsNext(SynthEffects *e, float input)
{
#define SMOOTH(field) e->current.field += e->smoothing * (e->target.field - e->current.field)
    SMOOTH(echoMix); SMOOTH(echoDelayMs); SMOOTH(echoFeedback);
    SMOOTH(reverbMix); SMOOTH(reverbRoom); SMOOTH(reverbDamping);
#undef SMOOTH
    double position = e->echo.cursor - e->current.echoDelayMs * e->sampleRate / 1000;
    if (position < 0) position += e->echo.length;
    int first = (int)position, second = (first + 1) % e->echo.length;
    double blend = position - first;
    double echo = e->echo.buffer[first] * (1 - blend) + e->echo.buffer[second] * blend;
    writeDelay(&e->echo, input + echo * e->current.echoFeedback);
    double echoOutput = input + e->current.echoMix * echo;
    double reverb = 0;
    for (int i = 0; i < 6; ++i) {
        EffectDelay *d = &e->combs[i];
        double delayed = d->buffer[d->cursor];
        /* Damping never fully freezes the feedback filter. */
        double damping = .95 * e->current.reverbDamping;
        d->filtered = delayed * (1 - damping) + d->filtered * damping;
        writeDelay(d, echoOutput + d->filtered * e->current.reverbRoom);
        reverb += delayed / 6;
    }
    for (int i = 0; i < 2; ++i) {
        EffectDelay *d = &e->allpasses[i];
        double delayed = d->buffer[d->cursor];
        double output = delayed - .5 * reverb;
        writeDelay(d, reverb + .5 * output);
        reverb = output;
    }
    return (float)(echoOutput + e->current.reverbMix * reverb);
}
