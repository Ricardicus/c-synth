#include "envelope.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#define CHECK(c) do { if (!(c)) { fprintf(stderr, "%d: %s\n", __LINE__, #c); exit(1); } } while (0)
static void steps(OutputEnvelope *e, int count) { while (count--) outputEnvelopeNext(e, 1000); }
int main(void)
{
    OutputEnvelope e = {0};
    outputEnvelopeConfigure(&e, (SynthEnvelopeConfig){100, 100, 25, 100});
    outputEnvelopeOn(&e);
    steps(&e, 50); CHECK(fabs(e.level - .5) < 1e-9);
    steps(&e, 50); CHECK(e.stage == FM_ENV_DECAY && e.level == 1);
    steps(&e, 50); CHECK(fabs(e.level - .625) < 1e-9);
    steps(&e, 50); CHECK(e.stage == FM_ENV_SUSTAIN && e.level == .25);
    outputEnvelopeOff(&e);
    steps(&e, 50); CHECK(fabs(e.level - .125) < 1e-9);
    outputEnvelopeOn(&e);
    CHECK(e.level == .125); /* Retrigger the tail without resetting amplitude. */
    steps(&e, 100); CHECK(e.level == 1);
    outputEnvelopeOff(&e);
    steps(&e, 20); CHECK(fabs(e.level - .8) < 1e-9);
    outputEnvelopeConfigure(&e, (SynthEnvelopeConfig){100, 100, 25, 40});
    CHECK(fabs(e.level - .8) < 1e-9);
    steps(&e, 40); CHECK(e.level == 0 && e.stage == FM_ENV_IDLE);
    outputEnvelopeConfigure(&e, (SynthEnvelopeConfig){0, 0, 100, 0});
    outputEnvelopeOn(&e);
    CHECK(outputEnvelopeNext(&e, 1000) == 1 && e.stage == FM_ENV_SUSTAIN);
    outputEnvelopeConfigure(&e, (SynthEnvelopeConfig){0, 0, 0, 0});
    steps(&e, 5); CHECK(e.level == 0);
    outputEnvelopeOff(&e); CHECK(outputEnvelopeNext(&e, 1000) == 0 && e.stage == FM_ENV_IDLE);
    outputEnvelopeOn(&e); CHECK(outputEnvelopeNext(&e, 1000) == 0);
    puts("Output ADSR stages, zero times, retrigger, and live edits passed.");
    return 0;
}
