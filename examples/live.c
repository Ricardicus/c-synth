#include "synth.h"
#include <stdint.h>
#include <stdio.h>

static void u16(FILE *file, unsigned value)
{
    fputc((int)(value & 255),file); fputc((int)((value >> 8) & 255),file);
}
static void u32(FILE *file, uint32_t value)
{
    u16(file,value & 65535); u16(file,value >> 16);
}
static void render(FILE *file, Synth *engine, size_t frames)
{
    float samples[256];
    while (frames) {
        size_t count=frames>256 ? 256 : frames;
        synthRender(engine,samples,count);
        for (size_t i=0;i<count;++i) {
            int16_t sample=(int16_t)(samples[i]*32767);
            u16(file,(uint16_t)sample);
        }
        frames-=count;
    }
}
int main(int argc, char **argv)
{
    const char *path=argc>1 ? argv[1] : "csynth-live.wav";
    const unsigned rate=48000, frames=rate*3;
    SynthConfig sound=synthPresetConfig(5);
    Synth *engine=synthCreate((int)rate,&sound);
    if (!engine) { fprintf(stderr,"Couldn't create the synth.\n"); return 1; }
    FILE *file=fopen(path,"wb");
    if (!file) { perror(path); synthDestroy(engine); return 1; }
    fwrite("RIFF",1,4,file); u32(file,36+frames*2); fwrite("WAVEfmt ",1,8,file);
    u32(file,16); u16(file,1); u16(file,1); u32(file,rate); u32(file,rate*2);
    u16(file,2); u16(file,16); fwrite("data",1,4,file); u32(file,frames*2);
    synthNoteOn(engine,60); synthNoteOn(engine,64); synthNoteOn(engine,67);
    render(file,engine,rate);
    sound.layers[0].fm.operators[0].rm=.7;
    sound.outputEnvelope.sustainPercent=65;
    sound.outputEnvelope.releaseMs=600;
    sound.effects.echoMix=.2;
    int result=synthConfigure(engine,&sound);
    render(file,engine,rate);
    synthNoteOff(engine,60); synthNoteOff(engine,64); synthNoteOff(engine,67);
    render(file,engine,rate);
    if (ferror(file)) result=-1;
    if (fclose(file)) result=-1;
    synthDestroy(engine);
    if (!result) printf("Wrote %s (chord, live edit, release).\n",path);
    return result==0 ? 0 : 1;
}
