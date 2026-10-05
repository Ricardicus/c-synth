#include "presets.h"
#include "envelope.h"
#include "effects.h"
#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(c) do { if (!(c)) { fprintf(stderr,"%s:%d: %s (preset %s)\n",__FILE__,__LINE__,#c,name); exit(1); } } while(0)

int main(void)
{
    const int rate=24000, held=rate*2, total=rate*5;
    double lowest=1, highest=0;
    for (int p=0;p<SYNTH_PRESET_COUNT;++p) {
        const char *name=synthPresetName(p);
        SynthConfig expected=synthPresetConfig(p), c;
        CHECK(synthConfigValid(&expected));
        for (int previous=0;previous<p;++previous) CHECK(strcmp(name,synthPresetName(previous)));
        char slug[64],path[1024],loaded[PRESET_NAME_MAX+1],error[200];
        size_t n=strlen(name); CHECK(n<=PRESET_NAME_MAX);
        for (size_t i=0;i<n;++i) slug[i]=name[i]==' ' ? '-' : (char)tolower((unsigned char)name[i]);
        slug[n]=0;
        snprintf(path,sizeof(path),"%s/factory/%s.synth",FACTORY_TEST_DIR,slug);
        CHECK(!presetRead(path,loaded,&c,error,sizeof(error)));
        CHECK(!strcmp(name,loaded));
        CHECK(c.layerCount==expected.layerCount);
#define SAME(field) CHECK(c.field==expected.field)
        SAME(outputEnvelope.attackMs); SAME(outputEnvelope.decayMs);
        SAME(outputEnvelope.sustainPercent); SAME(outputEnvelope.releaseMs);
        SAME(effects.echoMix); SAME(effects.echoDelayMs); SAME(effects.echoFeedback);
        SAME(effects.reverbMix); SAME(effects.reverbRoom); SAME(effects.reverbDamping);
        for (int l=0;l<SYNTH_MAX_LAYERS;++l) {
            SAME(layers[l].gain); SAME(layers[l].detuneCents); SAME(layers[l].fm.operatorCount);
            for (int o=0;o<FM_MAX_OPERATORS;++o) {
                SAME(layers[l].fm.operators[o].waveform); SAME(layers[l].fm.operators[o].pulseWidth);
                SAME(layers[l].fm.operators[o].ratio); SAME(layers[l].fm.operators[o].rm);
                SAME(layers[l].fm.operators[o].indexMode); SAME(layers[l].fm.operators[o].decayRate);
                SAME(layers[l].fm.operators[o].attackMs); SAME(layers[l].fm.operators[o].decayMs);
                SAME(layers[l].fm.operators[o].sustainPercent); SAME(layers[l].fm.operators[o].releaseMs);
                SAME(layers[l].fm.operators[o].vibratoRateHz); SAME(layers[l].fm.operators[o].vibratoDepthCents);
            }
        }
#undef SAME
        /* Isolated low/middle/high notes, including master ADSR and effect tails.
         * Use a full two-second hold so slow pads reach their intended level. */
        const double frequencies[]={110,440,880};
        for (int pitch=0;pitch<3;++pitch) {
            FmSynth layers[SYNTH_MAX_LAYERS]; double hz[SYNTH_MAX_LAYERS];
            OutputEnvelope envelope={0}; SynthEffects effects;
            outputEnvelopeConfigure(&envelope,c.outputEnvelope); outputEnvelopeOn(&envelope);
            CHECK(!effectsInit(&effects,rate,c.effects));
            for (int l=0;l<c.layerCount;++l) {
                CHECK(!fmInit(&layers[l],rate,&c.layers[l].fm)); fmNoteOn(&layers[l],true);
                hz[l]=frequencies[pitch]*pow(2,c.layers[l].detuneCents/1200);
            }
            double energy=0, tail=0;
            for (int i=0;i<total;++i) {
                if (i==held) {
                    outputEnvelopeOff(&envelope);
                    for (int l=0;l<c.layerCount;++l) fmNoteOff(&layers[l]);
                }
                double dry=0, amp=outputEnvelopeNext(&envelope,rate);
                for (int l=0;l<c.layerCount;++l)
                    dry+=.1*amp*c.layers[l].gain/c.layerCount*fmNextSample(&layers[l],hz[l]);
                double sample=effectsNext(&effects,(float)dry);
                CHECK(isfinite(sample) && fabs(sample)<.2);
                if (i<held) energy+=sample*sample;
                if (i>total-rate/4) tail+=sample*sample;
            }
            double rms=sqrt(energy/held);
            CHECK(rms>.003 && rms<.1);
            CHECK(envelope.stage==FM_ENV_IDLE);
            CHECK(tail<energy*.15);
            lowest=fmin(lowest,rms); highest=fmax(highest,rms);
            effectsDestroy(&effects);
        }
    }
    printf("All %d factory files match compiled patches; 3-register renders passed (RMS %.4f..%.4f).\n",SYNTH_PRESET_COUNT,lowest,highest);
    return 0;
}
