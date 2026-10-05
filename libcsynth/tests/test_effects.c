#include "effects.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#define CHECK(c) do { if (!(c)) { fprintf(stderr, "%d: %s\n", __LINE__, #c); exit(1); } } while (0)
int main(void)
{
    SynthEffects e;
    SynthConfig config = synthDefaultConfig();
    CHECK(effectsInit(&e, 1000, config.effects) == 0);
    for (int i = 0; i < 100; ++i) {
        float input = (float)sin(i * .2);
        CHECK(effectsNext(&e, input) == input); /* Exact dry bypass. */
    }
    effectsDestroy(&e);
    config.effects.echoMix = 1; config.effects.echoDelayMs = 10; config.effects.echoFeedback = .5;
    CHECK(effectsInit(&e, 1000, config.effects) == 0);
    for (int i = 0; i <= 30; ++i) {
        float output = effectsNext(&e, i == 0 ? 1 : 0);
        double expected = i == 0 || i == 10 ? 1 : i == 20 ? .5 : i == 30 ? .25 : 0;
        CHECK(fabs(output - expected) < 1e-6);
    }
    effectsDestroy(&e);
    config.effects.echoMix = 0; config.effects.reverbMix = .6;
    CHECK(effectsInit(&e, 48000, config.effects) == 0);
    double early = 0, late = 0;
    for (int i = 0; i < 240000; ++i) {
        double output = effectsNext(&e, i == 0 ? 1 : 0);
        CHECK(isfinite(output) && fabs(output) <= 1.1);
        if (i > 1000 && i < 24000) early += output * output;
        if (i > 216000) late += output * output;
    }
    CHECK(early > .001 && late < early * .001);
    config.effects.echoMix = 1; config.effects.echoFeedback = .95;
    config.effects.reverbMix = 1; config.effects.reverbRoom = .95; config.effects.reverbDamping = 1;
    effectsConfigure(&e, config.effects);
    for (int i = 0; i < 100000; ++i) {
        if (i % 5000 == 0) {
            config.effects.echoDelayMs = i % 10000 ? 1 : 2000;
            effectsConfigure(&e, config.effects);
        }
        CHECK(isfinite(effectsNext(&e, i == 0 ? .5f : 0)));
    }
    effectsDestroy(&e);
    config.effects.echoFeedback = 1;
    CHECK(effectsInit(&e, 48000, config.effects) == -1);
    puts("Dry bypass, echo timing/feedback, reverb decay, and live effect edits passed.");
    return 0;
}
