#include "synth.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(c) do { if (!(c)) { fprintf(stderr,"%d: %s\n",__LINE__,#c); exit(1); } } while(0)

static double energy(const float *samples, size_t count)
{
    double sum=0;
    for (size_t i=0;i<count;++i) {
        CHECK(isfinite(samples[i]) && fabs(samples[i])<=1);
        sum+=samples[i]*samples[i];
    }
    return sum;
}

int main(void)
{
    SynthConfig patch=synthPresetConfig(1);
    patch.outputEnvelope=(SynthEnvelopeConfig){0,0,100,100};
    CHECK(!synthCreate(0,&patch)); CHECK(!synthCreate(384001,&patch));
    SynthConfig invalid=patch; invalid.layerCount=0;
    CHECK(!synthCreate(48000,&invalid));
    Synth *a=synthCreate(48000,&patch), *b=synthCreate(48000,&patch);
    CHECK(a && b);
    float x[4800], y[4800], history[SYNTH_ANALYSIS_SAMPLES]; uint64_t position;
    synthRender(NULL,x,4800); CHECK(energy(x,4800)==0);
    synthRender(a,x,4800); CHECK(energy(x,4800)==0);
    synthNoteOn(a,69);
    synthRender(a,x,4800); CHECK(energy(x,4800)>10);
    synthRender(b,y,4800); CHECK(energy(y,4800)==0); /* No shared voices/effects. */
    CHECK(synthAudioSnapshot(a,history,&position)==48000 && position==9600);
    CHECK(!memcmp(history,x+4800-SYNTH_ANALYSIS_SAMPLES,sizeof(history)));
    SynthConfig saved; CHECK(!synthGetConfig(a,&saved));
    CHECK(saved.layers[0].fm.operators[0].ratio==1);
    CHECK(synthConfigure(a,&invalid)==-1);
    CHECK(!synthGetConfig(a,&saved) && saved.layerCount==1);
    patch.layers[0].fm.operators[0].ratio=2;
    patch.outputEnvelope.sustainPercent=25;
    CHECK(!synthConfigure(a,&patch));
    synthRender(a,x,4800); synthRender(a,x,4800);
    double quietEnergy=energy(x,4800); CHECK(quietEnergy>.8 && quietEnergy<2);
    CHECK(!synthGetConfig(a,&saved) && saved.layers[0].fm.operators[0].ratio==2);
    synthMidiNoteOn(a,69,32); synthNoteOff(a,69);
    synthRender(a,x,4800); synthRender(a,x,4800);
    CHECK(energy(x,4800)<quietEnergy*.08); /* MIDI velocity survives manual release. */
    synthMidiNoteOff(a,69);
    synthRender(a,x,4800); CHECK(energy(x,4800)>0);
    synthRender(a,x,4800); CHECK(energy(x,4800)==0);
    synthDestroy(a); synthDestroy(b); synthDestroy(NULL);
    /* Rendering in arbitrary block sizes must not change the stream. */
    patch=synthPresetConfig(12);
    a=synthCreate(44100,&patch); b=synthCreate(44100,&patch); CHECK(a && b);
    synthNoteOn(a,72); synthNoteOn(b,72);
    synthRender(a,x,4800);
    for (size_t offset=0;offset<4800;) {
        size_t count=offset%97+1; if (count>4800-offset) count=4800-offset;
        synthRender(b,y+offset,count); offset+=count;
    }
    CHECK(!memcmp(x,y,sizeof(x)));
    synthDestroy(a); synthDestroy(b);
    /* All factory patches through the full engine, including independent tails. */
    for (int preset=0;preset<SYNTH_PRESET_COUNT;++preset) {
        patch=synthPresetConfig(preset); a=synthCreate(24000,&patch); CHECK(a);
        synthNoteOn(a,60); synthNoteOn(a,64); synthNoteOn(a,67);
        double total=0;
        for (int block=0;block<10;++block) { synthRender(a,x,4800); total+=energy(x,4800); }
        CHECK(total>1);
        synthNoteOff(a,60); synthNoteOff(a,64); synthNoteOff(a,67);
        for (int block=0;block<15;++block) { synthRender(a,x,4800); energy(x,4800); }
        synthDestroy(a);
    }
    puts("SDL-free instances, live edits, ownership, snapshot ordering, block sizes and 64 factory chords passed.");
    return 0;
}
