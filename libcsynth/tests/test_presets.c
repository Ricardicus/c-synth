#include "test_temp.h"
#include "presets.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(c) do { if (!(c)) { fprintf(stderr,"%d: %s\n",__LINE__,#c); exit(1); } } while (0)
int main(void)
{
    char folder[1024]; CHECK(testTempDirectory(folder,sizeof(folder),"presets"));
    PresetLibrary library; char error[128];
    CHECK(presetLibraryInit(&library,folder,error,sizeof(error))==0);
    CHECK(library.count==SYNTH_PRESET_COUNT);
    SynthConfig c=synthPresetConfig(12);
    c.layers[0].fm.operators[7].ratio=3.141592653589793;
    c.layers[6].detuneCents=-12.3456789;
    c.effects.echoMix=.123456789; c.effects.echoFeedback=.55;
    int saved=presetLibrarySave(&library,"My flute 1",&c,error,sizeof(error));
    CHECK(saved==SYNTH_PRESET_COUNT && library.count==SYNTH_PRESET_COUNT+1);
    char path[PRESET_PATH_MAX]; strcpy(path,library.items[saved].path);
    char name[PRESET_NAME_MAX+1]; SynthConfig restored;
    CHECK(presetRead(path,name,&restored,error,sizeof(error))==0);
    CHECK(!strcmp(name,"My flute 1") && restored.layerCount==2);
    CHECK(restored.layers[0].fm.operators[7].ratio==c.layers[0].fm.operators[7].ratio);
    CHECK(restored.layers[6].detuneCents==c.layers[6].detuneCents);
    CHECK(restored.effects.echoMix==c.effects.echoMix && restored.outputEnvelope.releaseMs==220);
    CHECK(presetLibrarySave(&library,"My flute 1",&c,error,sizeof(error))==-1);
    CHECK(presetLibrarySave(&library,"MY-FLUTE-1",&c,error,sizeof(error))==-1);
    CHECK(presetLibrarySave(&library,"../escape",&c,error,sizeof(error))==-1);
    CHECK(presetLibrarySave(&library,"",&c,error,sizeof(error))==-1);
    CHECK(presetLibraryDelete(&library,0,error,sizeof(error))==-1);
    char bad[PRESET_PATH_MAX]; snprintf(bad,sizeof(bad),"%s/user/bad.synth",folder);
    FILE *file=fopen(bad,"w"); CHECK(file); fputs("SYNTH_PRESET 99\nname Bad\n",file); fclose(file);
    presetLibraryDestroy(&library);
    CHECK(presetLibraryInit(&library,folder,error,sizeof(error))==0);
    CHECK(library.count==SYNTH_PRESET_COUNT+1 && library.skipped==1);
    CHECK(!strcmp(library.items[SYNTH_PRESET_COUNT].name,"My flute 1"));
    restored=synthDefaultConfig(); restored.layerCount=3;
    CHECK(presetRead(bad,name,&restored,error,sizeof(error))==-1 && restored.layerCount==3);
    CHECK(presetLibraryDelete(&library,SYNTH_PRESET_COUNT,error,sizeof(error))==0);
    CHECK(!testFileExists(path));
    CHECK(presetLibraryDelete(&library,SYNTH_PRESET_COUNT,error,sizeof(error))==-1);
    presetLibraryDestroy(&library);
    CHECK(presetLibraryInit(&library,folder,error,sizeof(error))==0 && library.count==SYNTH_PRESET_COUNT);
    presetLibraryDestroy(&library);
    CHECK(remove(bad)==0);
    char user[PRESET_PATH_MAX]; snprintf(user,sizeof(user),"%s/user",folder); CHECK(TEST_RMDIR(user)==0); CHECK(TEST_RMDIR(folder)==0);
    puts("Full-precision presets, inactive settings, reload, duplicates, malformed files and deletion passed.");
    return 0;
}
