#include "test_temp.h"
/* Inspect UI state as well as dispatch results: ignored clicks must fail tests. */
#include "../spectrogram.c"

#define CHECK(c) do { if (!(c)) { fprintf(stderr, "%d: %s (%s)\n", __LINE__, #c, SDL_GetError()); exit(1); } } while (0)

static void click(Spectrogram *view, int x, int y)
{
    SDL_Event e = {0};
    e.type = SDL_MOUSEBUTTONDOWN;
    e.button.button = SDL_BUTTON_LEFT;
    e.button.x = x; e.button.y = y;
    CHECK(spectrogramEvent(view, &e) == 1);
}

int main(void)
{
    char presetFolder[1024]; CHECK(testTempDirectory(presetFolder,sizeof(presetFolder),"gui"));
    CHECK(SDL_setenv("SYNTH_PRESET_DIR",presetFolder,1)==0);
    SDL_SetMainReady();
    SDL_setenv("SDL_VIDEODRIVER", "dummy", 1);
    SDL_setenv("SDL_AUDIODRIVER", "dummy", 1);
    CHECK(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) == 0);
    CHECK(sdlSynthInit() == 0);
    SDL_Window *w = SDL_CreateWindow("UI test", 0, 0, 1640, 780, SDL_WINDOW_RESIZABLE);
    CHECK(w != NULL);
    Spectrogram *v = spectrogramCreate(w, 1);
    CHECK(v != NULL);
    /* SDL delivers logical coordinates even after resizing and letterboxing. */
    const int sizes[][2] = {{1640,780}, {820,390}, {1100,900}};
    for (int i = 0; i < 3; ++i) {
        SDL_SetWindowSize(w, sizes[i][0], sizes[i][1]);
        SDL_PumpEvents();
        CHECK(spectrogramDraw(v) == 0);
        /* Verify the raw-coordinate conversion used for wheel hit testing. */
        int wx, wy;
        float lx, ly;
        SDL_RenderLogicalToWindow(v->renderer, 1275, 120, &wx, &wy);
        SDL_RenderWindowToLogical(v->renderer, wx, wy, &lx, &ly);
        CHECK(fabs(lx - 1275) <= 2 && fabs(ly - 120) <= 2);
        /* Synthetic pushes bypass native mouse transformation; dispatch the
         * logical coordinates that SDL documents for real button events. */
        int previous = v->config.layerCount;
        click(v, 1275, 120);
        CHECK(v->config.layerCount == previous + 1);
        click(v, 100, 25);
        CHECK(v->presetOpen);
        click(v, 100, PRESET_LIST_Y + 35);
        CHECK(v->preset == 1 && !v->presetOpen);
        CHECK(v->config.layers[0].fm.operatorCount == 1);
    }
    /* Wheel scroll reaches the final preset; selection replaces the sound. */
    click(v, 100, 25);
    SDL_Event e = {0};
    e.type = SDL_MOUSEWHEEL; e.wheel.y = -1;
    for (int i = 0; i < SYNTH_PRESET_COUNT; ++i) CHECK(spectrogramEvent(v, &e) == 1);
    CHECK(v->presetScroll == SYNTH_PRESET_COUNT - PRESET_VISIBLE);
    click(v, 100, PRESET_LIST_Y + 7 * PRESET_ITEM_H + 10);
    CHECK(v->preset == SYNTH_PRESET_COUNT - 1 && !v->presetOpen);
    CHECK(!strcmp(presetName(v, v->preset), "Graph Orbit Texture"));
    CHECK(v->config.layers[0].fm.operatorCount == 8);
    /* Adjacent arrows step through the same library and wrap at either end. */
    click(v, 555, 25);
    CHECK(v->preset == 0);
    click(v, 45, 25);
    CHECK(v->preset == SYNTH_PRESET_COUNT - 1);
    click(v, 45, 25);
    CHECK(v->preset == SYNTH_PRESET_COUNT - 2);
    click(v, 555, 25);
    CHECK(v->preset == SYNTH_PRESET_COUNT - 1);
    SynthConfig baseline = synthPresetConfig(11);
    baseline.outputEnvelope.sustainPercent = 100; /* Exercise the knob's upper clamp. */
    CHECK(sdlSynthConfigure(&baseline) == 0);
    spectrogramSetConfig(v, &baseline);
    /* Upward dragging increases gain; release ends the drag. */
    v->config.layers[0].gain = .5;
    CHECK(sdlSynthConfigure(&v->config) == 0);
    click(v, 1160, 194);
    e.type = SDL_MOUSEMOTION; e.motion.x = 1160; e.motion.y = 154;
    CHECK(spectrogramEvent(v, &e) == 1);
    CHECK(fabs(v->config.layers[0].gain - .6) < .00001 && v->preset == -1);
    e.type = SDL_MOUSEBUTTONUP; e.button.button = SDL_BUTTON_LEFT;
    CHECK(spectrogramEvent(v, &e) == 1 && v->dragRow == -1);
    /* Main-view filter knobs enable, tune, and bypass through real dispatch. */
    click(v,185,594);
    e.type=SDL_MOUSEMOTION; e.motion.y=554;
    CHECK(spectrogramEvent(v,&e)==1 && v->config.filters.lowpassHz==20);
    e.motion.y=506;
    CHECK(spectrogramEvent(v,&e)==1 && fabs(v->config.filters.lowpassHz-40)<1e-8);
    e.type=SDL_MOUSEBUTTONUP; e.button.button=SDL_BUTTON_LEFT;
    CHECK(spectrogramEvent(v,&e)==1);
    click(v,410,594); e.type=SDL_MOUSEMOTION; e.motion.y=554;
    CHECK(spectrogramEvent(v,&e)==1 && v->config.filters.highpassHz==20);
    e.motion.y=598;
    CHECK(spectrogramEvent(v,&e)==1 && v->config.filters.highpassHz==0);
    e.type=SDL_MOUSEBUTTONUP; e.button.button=SDL_BUTTON_LEFT;
    CHECK(spectrogramEvent(v,&e)==1);
    /* The four master knobs edit shared output settings, not operator timbre. */
    int before[4] = {v->config.outputEnvelope.attackMs, v->config.outputEnvelope.decayMs,
                     v->config.outputEnvelope.sustainPercent, v->config.outputEnvelope.releaseMs};
    for (int i = 0; i < 4; ++i) {
        click(v, 180 + i * 225, 715);
        e.type = SDL_MOUSEMOTION; e.motion.y = i == 2 ? 711 : 675;
        CHECK(spectrogramEvent(v, &e) == 1);
        e.type = SDL_MOUSEBUTTONUP; e.button.button = SDL_BUTTON_LEFT;
        CHECK(spectrogramEvent(v, &e) == 1);
    }
    CHECK(v->config.outputEnvelope.attackMs == before[0] + 250);
    CHECK(v->config.outputEnvelope.decayMs == before[1] + 250);
    CHECK(v->config.outputEnvelope.sustainPercent == 100); /* Clamped. */
    CHECK(v->config.outputEnvelope.releaseMs == before[3] + 250);
    /* Escape dismisses a dropdown, and arrows/Enter commit one selection. */
    click(v, 100, 25);
    e.type = SDL_KEYDOWN; e.key.keysym.sym = SDLK_ESCAPE;
    CHECK(spectrogramEvent(v, &e) == 1 && !v->presetOpen);
    click(v, 100, 25);
    e.key.keysym.sym = SDLK_END;
    CHECK(spectrogramEvent(v, &e) == 1);
    e.key.keysym.sym = SDLK_RETURN;
    CHECK(spectrogramEvent(v, &e) == 1 && v->preset == SYNTH_PRESET_COUNT - 1);
    CHECK(spectrogramDraw(v) == 0);
    baseline = synthPresetConfig(11);
    CHECK(sdlSynthConfigure(&baseline) == 0);
    spectrogramSetConfig(v, &baseline);
    /* Envelope timing remains precise with Shift after speeding up normal drag. */
    int attackBefore = v->config.outputEnvelope.attackMs;
    click(v, 180, 715);
    SDL_SetModState(KMOD_SHIFT);
    e.type = SDL_MOUSEMOTION; e.motion.y = 711;
    CHECK(spectrogramEvent(v, &e) == 1);
    CHECK(v->config.outputEnvelope.attackMs == attackBefore + 1);
    SDL_SetModState(KMOD_NONE);
    e.motion.y = 715;
    CHECK(spectrogramEvent(v, &e) == 1);
    CHECK(v->config.outputEnvelope.attackMs == (int)fmax(0, attackBefore - 24));
    e.type = SDL_MOUSEBUTTONUP; e.button.button = SDL_BUTTON_LEFT;
    CHECK(spectrogramEvent(v, &e) == 1);
    /* Decimal knobs react to one pixel, including reversals and Shift precision. */
    v->config.layers[0].fm.operators[0].ratio = 1;
    click(v, 1112, 408);
    e.type = SDL_MOUSEMOTION; e.motion.y = 407;
    CHECK(spectrogramEvent(v, &e) == 1);
    CHECK(fabs(v->config.layers[0].fm.operators[0].ratio - 1.0025) < 1e-9);
    e.motion.y = 408;
    CHECK(spectrogramEvent(v, &e) == 1);
    CHECK(fabs(v->config.layers[0].fm.operators[0].ratio - 1) < 1e-9);
    SDL_SetModState(KMOD_SHIFT);
    e.motion.y = 404;
    CHECK(spectrogramEvent(v, &e) == 1);
    CHECK(fabs(v->config.layers[0].fm.operators[0].ratio - 1.001) < 1e-9);
    SDL_SetModState(KMOD_NONE);
    e.type = SDL_MOUSEBUTTONUP; e.button.button = SDL_BUTTON_LEFT;
    CHECK(spectrogramEvent(v, &e) == 1);
    CHECK(changeRow(v, 8, 1) == 0);
    CHECK(fabs(v->config.layers[0].fm.operators[0].rm - 5.01) < 1e-9);
    /* Raw functions use actual waveforms, numeric FM deviation, and live pitch. */
    SynthConfig raw = synthDefaultConfig();
    spectrogramSetConfig(v, &raw);
    spectrogramNote(v, 69);
    EquationLines functions = {0};
    fmFunctions(v, &functions);
    CHECK(strcmp(functions.lines[0], "x(t) = (0.10 / 1.00) * (") == 0);
    CHECK(strstr(functions.lines[1], "1.00 * sin(p1_2(t))") != NULL);
    CHECK(strstr(functions.lines[4], "440.00") != NULL);
    CHECK(strstr(functions.lines[5], "880.00 * 1.00 * sin(p1_1(t))") != NULL);
    raw.layers[0].fm.operators[0].waveform = WAVE_PULSE;
    raw.layers[0].fm.operators[0].indexMode = FM_INDEX_DECAY;
    raw.layers[0].fm.operators[0].decayRate = 2.25;
    spectrogramSetConfig(v, &raw);
    spectrogramNote(v, 81);
    functions = (EquationLines){0};
    fmFunctions(v, &functions);
    CHECK(strstr(functions.lines[4], "880.00") != NULL);
    CHECK(strstr(functions.lines[5], "exp(-2.25*t) * pulse(p1_1(t), 0.25)") != NULL);
    functions = (EquationLines){0};
    outputFunctions(v, &functions);
    CHECK(strstr(functions.lines[3], "t / 5.00") != NULL);
    raw.outputEnvelope = (SynthEnvelopeConfig){0,0,100,0};
    spectrogramSetConfig(v, &raw);
    functions = (EquationLines){0};
    outputFunctions(v, &functions);
    for (int i = 0; i < functions.count; ++i) CHECK(strstr(functions.lines[i], "/0.00") == NULL);
    SynthConfig graphPatch=synthPresetConfig(68);
    spectrogramSetConfig(v,&graphPatch); functions=(EquationLines){0}; fmFunctions(v,&functions);
    CHECK(strstr(functions.lines[0],"dt = 1.00 / sampleRate"));
    bool feedbackFunction=false;
    for (int i=0;i<functions.count;++i) feedbackFunction |= strstr(functions.lines[i],"y1_1[n-1]")!=NULL;
    CHECK(feedbackFunction);
    spectrogramSetConfig(v,&raw);
    /* Equation views are readable pages, and Escape returns without quitting. */
    click(v, 795, 25);
    CHECK(v->equationPage == 4);
    click(v, 360, 35);
    CHECK(v->equationPage == 1);
    CHECK(spectrogramDraw(v) == 0);
    click(v, 620, 35);
    CHECK(v->equationPage == 2);
    CHECK(spectrogramDraw(v) == 0);
    e.type = SDL_KEYDOWN; e.key.keysym.sym = SDLK_ESCAPE;
    CHECK(spectrogramEvent(v, &e) == 1 && v->equationPage == 0);
    CHECK(spectrogramDraw(v) == 0);
    click(v, 620, 25);
    CHECK(v->equationPage == 3);
    CHECK(spectrogramDraw(v) == 0);
    for (int i = SOUND_KNOBS; i < KNOB_COUNT; ++i) {
        SDL_Rect rect = knobRect(i);
        click(v, rect.x + 120, rect.y + 50);
        e.type = SDL_MOUSEMOTION; e.motion.y = rect.y + 46;
        CHECK(spectrogramEvent(v, &e) == 1);
        e.type = SDL_MOUSEBUTTONUP; e.button.button = SDL_BUTTON_LEFT;
        CHECK(spectrogramEvent(v, &e) == 1);
    }
    CHECK(fabs(v->config.effects.echoMix - .01) < 1e-9);
    CHECK(fabs(v->config.effects.echoDelayMs - 301) < 1e-9);
    CHECK(fabs(v->config.effects.echoFeedback - .36) < 1e-9);
    CHECK(fabs(v->config.effects.reverbMix - .01) < 1e-9);
    CHECK(fabs(v->config.effects.reverbRoom - .71) < 1e-9);
    CHECK(fabs(v->config.effects.reverbDamping - .41) < 1e-9);
    e.type = SDL_KEYDOWN; e.key.keysym.sym = SDLK_ESCAPE;
    CHECK(spectrogramEvent(v, &e) == 1 && v->equationPage == 0);
    /* Save named settings through the real text-input dialog, then reload/delete. */
    SynthConfig savedConfig=v->config;
    click(v,100,75); CHECK(v->presetDialog==1);
    e=(SDL_Event){0}; e.type=SDL_TEXTINPUT; strcpy(e.text.text,"GUI flute");
    CHECK(spectrogramEvent(v,&e)==1);
    e.type=SDL_KEYDOWN; e.key.keysym.sym=SDLK_RETURN;
    CHECK(spectrogramEvent(v,&e)==1 && !v->presetDialog && v->preset==SYNTH_PRESET_COUNT);
    CHECK(v->library.count==SYNTH_PRESET_COUNT+1);
    char savedPath[1024]; strcpy(savedPath,v->library.items[v->preset].path); CHECK(testFileExists(savedPath));
    PresetLibrary reloaded; char issue[128];
    CHECK(presetLibraryInit(&reloaded,presetFolder,issue,sizeof(issue))==0 && reloaded.count==SYNTH_PRESET_COUNT+1);
    presetLibraryDestroy(&reloaded);
    CHECK(changeRow(v,2,-1)==0);
    click(v,100,25);
    e.key.keysym.sym=SDLK_END; CHECK(spectrogramEvent(v,&e)==1);
    e.key.keysym.sym=SDLK_RETURN; CHECK(spectrogramEvent(v,&e)==1);
    CHECK(v->config.layers[0].gain==savedConfig.layers[0].gain && v->preset==SYNTH_PRESET_COUNT);
    click(v,100,75);
    e.type=SDL_TEXTINPUT; strcpy(e.text.text,"GUI flute"); CHECK(spectrogramEvent(v,&e)==1);
    e.type=SDL_KEYDOWN; e.key.keysym.sym=SDLK_RETURN;
    CHECK(spectrogramEvent(v,&e)==1 && v->presetDialog==1 && v->presetError[0]);
    e.key.keysym.sym=SDLK_ESCAPE; CHECK(spectrogramEvent(v,&e)==1 && !v->presetDialog);
    click(v,335,75); CHECK(v->presetDialog==2);
    e.key.keysym.sym=SDLK_ESCAPE; CHECK(spectrogramEvent(v,&e)==1 && testFileExists(savedPath));
    click(v,335,75); e.key.keysym.sym=SDLK_RETURN;
    CHECK(spectrogramEvent(v,&e)==1 && v->preset==-1 && !testFileExists(savedPath));
    CHECK(v->config.layers[0].gain==savedConfig.layers[0].gain);
    /* Browse/select a MIDI file and exercise Play, Stop and Restart buttons. */
    char songPath[1024]; snprintf(songPath,sizeof(songPath),"%s/song.mid",presetFolder);
    const unsigned char midiBytes[]={ 'M','T','h','d',0,0,0,6,0,0,0,1,1,0xe0,
        'M','T','r','k',0,0,0,13,0,0x90,60,100,0x83,0x60,0x80,60,0,0,0xff,0x2f,0 };
    FILE *song=fopen(songPath,"wb"); CHECK(song); CHECK(fwrite(midiBytes,1,sizeof(midiBytes),song)==sizeof(midiBytes)); CHECK(fclose(song)==0);
    CHECK(fileBrowserOpen(&v->browser,presetFolder,issue,sizeof(issue))==0);
    click(v,795,25); CHECK(v->equationPage==4);
    click(v,110,185); CHECK(v->browserOpen);
    int fileIndex=-1;
    for (int i=0;i<v->browser.count;++i) if (!strcmp(v->browser.entries[i].name,"song.mid")) fileIndex=i;
    CHECK(fileIndex>=0);
    click(v,220,235+fileIndex*38);
    CHECK(!v->browserOpen && v->midi.song.durationUs==500000);
    click(v,110,305); CHECK(v->midi.playing && v->midi.held[60]);
    click(v,300,305); CHECK(!v->midi.playing && !v->midi.held[60]);
    click(v,490,305); CHECK(v->midi.playing && v->midi.positionUs==0);
    CHECK(spectrogramDraw(v)==0);
    midiPlayerStop(&v->midi,midiNow());
    e.type=SDL_KEYDOWN; e.key.keysym.sym=SDLK_ESCAPE;
    CHECK(spectrogramEvent(v,&e)==1 && v->equationPage==0);
    CHECK(remove(songPath)==0);
    /* Recording browser, Hz editing, atomic apply, mode switching and map loading. */
    char wavePath[1024], mapPath[1024];
    snprintf(wavePath,sizeof(wavePath),"%s/tone.wav",presetFolder);
    snprintf(mapPath,sizeof(mapPath),"%s/tone.csamples",presetFolder);
    const unsigned char waveHeader[]={ 'R','I','F','F',40,0,0,0,'W','A','V','E',
        'f','m','t',' ',16,0,0,0,1,0,1,0,0x80,0xbb,0,0,0,0x77,1,0,2,0,16,0,
        'd','a','t','a',4,0,0,0,0,0x40,0,0x40 };
    FILE *wave=fopen(wavePath,"wb"); CHECK(wave);
    CHECK(fwrite(waveHeader,1,sizeof(waveHeader),wave)==sizeof(waveHeader)); CHECK(!fclose(wave));
    FILE *map=fopen(mapPath,"wb"); CHECK(map);
    CHECK(fputs("csynth-samples 1\nsample 220.00 1.00 0 2 \"tone.wav\"\n",map)>=0); CHECK(!fclose(map));
    click(v,650,75); CHECK(v->equationPage==5); CHECK(spectrogramDraw(v)==0);
    for (int n=0;n<2;++n) {
        click(v,120,160); CHECK(v->browserOpen && v->browser.filter==BROWSER_AUDIO);
        CHECK(v->browser.count==2); /* Only the user folder and WAV, never the map. */
        fileIndex=-1;
        for (int i=0;i<v->browser.count;++i) if (!strcmp(v->browser.entries[i].name,"tone.wav")) fileIndex=i;
        CHECK(fileIndex>=0); click(v,220,235+fileIndex*38);
        CHECK(!v->browserOpen && v->sampleCount==n+1 && v->sampleEdit==n);
        e=(SDL_Event){0}; e.type=SDL_KEYDOWN; e.key.keysym.sym=SDLK_a; e.key.keysym.mod=KMOD_CTRL;
        CHECK(spectrogramEvent(v,&e)==1);
        e.type=SDL_TEXTINPUT; strcpy(e.text.text,n ? "440.00" : "220.00"); CHECK(spectrogramEvent(v,&e)==1);
        e.type=SDL_KEYDOWN; e.key.keysym.sym=SDLK_RETURN; e.key.keysym.mod=0;
        CHECK(spectrogramEvent(v,&e)==1 && v->sampleEdit==-1);
    }
    click(v,560,160); CHECK(v->appliedSamples==2 && !v->sampleError[0]);
    CHECK(sdlSynthGetSourceMode()==SYNTH_SOURCE_SAMPLES);
    strcpy(v->sampleRows[0].hz,"0"); click(v,560,160);
    CHECK(v->sampleError[0] && v->appliedSamples==2 && sdlSynthGetSourceMode()==SYNTH_SOURCE_SAMPLES);
    strcpy(v->sampleRows[0].hz,"220.00");
    click(v,800,160); CHECK(sdlSynthGetSourceMode()==SYNTH_SOURCE_FM);
    click(v,1010,160); CHECK(sdlSynthGetSourceMode()==SYNTH_SOURCE_SAMPLES);
    click(v,120,250); click(v,120,625); CHECK(v->sampleCount==1 && v->appliedSamples==2);
    click(v,560,160); CHECK(v->appliedSamples==1);
    click(v,340,160); CHECK(v->browserOpen && v->browser.filter==BROWSER_SAMPLE_MAP);
    fileIndex=-1;
    for (int i=0;i<v->browser.count;++i) if (!strcmp(v->browser.entries[i].name,"tone.csamples")) fileIndex=i;
    CHECK(fileIndex>=0); click(v,220,235+fileIndex*38);
    CHECK(!v->browserOpen && v->appliedSamples==1 && !v->sampleError[0]);
    CHECK(strstr(v->sampleStatus,"tone.csamples"));
    /* Editing shared processing controls must preserve the sample source. */
    click(v,880,35); CHECK(v->equationPage==3);
    CHECK(changeRow(v,22,1)==0 && sdlSynthGetSourceMode()==SYNTH_SOURCE_SAMPLES);
    click(v,100,35); CHECK(v->equationPage==0);
    CHECK(changeRow(v,18,1)==0 && sdlSynthGetSourceMode()==SYNTH_SOURCE_SAMPLES);
    CHECK(changeRow(v,28,1)==0 && sdlSynthGetSourceMode()==SYNTH_SOURCE_SAMPLES);
    CHECK(changeRow(v,29,1)==0 && sdlSynthGetSourceMode()==SYNTH_SOURCE_SAMPLES);
    CHECK(remove(wavePath)==0 && remove(mapPath)==0);
    /* FM routing dispatch, sample-mode explanations, and saved graph settings. */
    SynthConfig routed=synthPresetConfig(64);
    spectrogramSetConfig(v,&routed); CHECK(sdlSynthConfigure(&routed)==0);
    click(v,820,75); CHECK(v->equationPage==6); CHECK(spectrogramDraw(v)==0);
    click(v,1100,255); CHECK(v->config.layers[0].fm.algorithm==FM_ALGORITHM_PAIRS);
    CHECK(strstr(routingReason(v,31),"Sample mode"));
    click(v,1120,145); CHECK(sdlSynthGetSourceMode()==SYNTH_SOURCE_FM);
    click(v,1100,255); CHECK(v->config.layers[0].fm.algorithm==FM_ALGORITHM_CUSTOM);
    click(v,920,145); click(v,920,145); CHECK(v->selectedOperator==2);
    const int routingIndices[]={2,1,0};
    for(int i=0;i<3;++i) {
        SDL_Rect knob=routingKnobRect(routingIndices[i]); int cx=knob.x+knob.w/2,cy=knob.y+52;
        click(v,cx,cy); CHECK(v->dragRow>=30);
        e=(SDL_Event){0}; e.type=SDL_MOUSEMOTION; e.motion.y=cy-40;
        CHECK(spectrogramEvent(v,&e)==1);
        e.type=SDL_MOUSEBUTTONUP; e.button.button=SDL_BUTTON_LEFT; CHECK(spectrogramEvent(v,&e)==1);
    }
    CHECK(fabs(v->config.layers[0].fm.routing[0][2]-.1)<1e-9);
    CHECK(fabs(v->config.layers[0].fm.operators[2].feedback-.1)<1e-9);
    CHECK(fabs(v->config.layers[0].fm.operators[2].outputLevel-.1)<1e-9);
    SDL_Rect inputKnob=routingKnobRect(2);
    click(v,inputKnob.x+inputKnob.w/2,inputKnob.y+52); SDL_SetModState(KMOD_SHIFT);
    e.type=SDL_MOUSEMOTION; e.motion.y=inputKnob.y+48; CHECK(spectrogramEvent(v,&e)==1);
    CHECK(fabs(v->config.layers[0].fm.routing[0][2]-.101)<1e-9);
    SDL_SetModState(KMOD_NONE); e.type=SDL_MOUSEBUTTONUP; e.button.button=SDL_BUTTON_LEFT;
    CHECK(spectrogramEvent(v,&e)==1);
    FmSynth diagram; CHECK(fmInit(&diagram,48000,&v->config.layers[0].fm)==0);
    SDL_Rect sourceNode=routingNodeRect(&diagram,0);
    click(v,sourceNode.x+20,sourceNode.y+10); CHECK(v->selectedOperator==0);
    click(v,inputKnob.x+inputKnob.w/2,inputKnob.y+52); CHECK(v->dragRow==-1 && v->config.layers[0].fm.routing[0][0]==0);
    SDL_Rect outputKnob=routingKnobRect(0),feedbackKnob=routingKnobRect(1);
    click(v,outputKnob.x+outputKnob.w/2,outputKnob.y+52); CHECK(v->dragRow==30);
    e.type=SDL_MOUSEBUTTONUP; e.button.button=SDL_BUTTON_LEFT; CHECK(spectrogramEvent(v,&e)==1);
    char graphPath[1024],graphName[PRESET_NAME_MAX+1]; SynthConfig loadedGraph;
    snprintf(graphPath,sizeof(graphPath),"%s/routing.synth",presetFolder);
    CHECK(presetWrite(graphPath,"Routing test",&v->config,issue,sizeof(issue))==0);
    CHECK(presetRead(graphPath,graphName,&loadedGraph,issue,sizeof(issue))==0);
    CHECK(loadedGraph.layers[0].fm.algorithm==FM_ALGORITHM_CUSTOM);
    CHECK(loadedGraph.layers[0].fm.routing[0][2]==v->config.layers[0].fm.routing[0][2]);
    CHECK(loadedGraph.layers[0].fm.operators[2].feedback==v->config.layers[0].fm.operators[2].feedback);
    CHECK(remove(graphPath)==0);
    for(int mode=0;mode<FM_ALGORITHM_COUNT;++mode) {
        SDL_Rect rect=algorithmRect(mode); click(v,rect.x+10,rect.y+10);
        CHECK(v->config.layers[0].fm.algorithm==(FmAlgorithm)mode);
        CHECK(spectrogramDraw(v)==0);
    }
    /* Every algorithm has clickable, non-overlapping operator nodes at all sizes. */
    for(int mode=0;mode<FM_ALGORITHM_COUNT;++mode) for(int count=1;count<=8;++count) {
        SynthConfig layout=synthDefaultConfig(); layout.layers[0].fm.algorithm=(FmAlgorithm)mode;
        layout.layers[0].fm.operatorCount=count; spectrogramSetConfig(v,&layout); CHECK(sdlSynthConfigure(&layout)==0);
        CHECK(fmInit(&diagram,48000,&layout.layers[0].fm)==0);
        for(int op=0;op<count;++op) {
            SDL_Rect node=routingNodeRect(&diagram,op); CHECK(node.x>=100 && node.x+node.w<=1040 && node.y>=300 && node.y+node.h<=585);
            click(v,node.x+20,node.y+10); CHECK(v->selectedOperator==op);
        }
        CHECK(spectrogramDraw(v)==0);
    }
    click(v,110,200); CHECK(v->config.layers[0].fm.algorithm==FM_ALGORITHM_CHAIN);
    click(v,outputKnob.x+outputKnob.w/2,outputKnob.y+52); CHECK(v->dragRow==-1); /* Legacy output is fixed. */
    click(v,feedbackKnob.x+feedbackKnob.w/2,feedbackKnob.y+52); CHECK(v->dragRow==31); /* Carrier feedback is still available. */
    e.type=SDL_MOUSEBUTTONUP; e.button.button=SDL_BUTTON_LEFT; CHECK(spectrogramEvent(v,&e)==1);
    e.type=SDL_KEYDOWN; e.key.keysym.sym=SDLK_ESCAPE; CHECK(spectrogramEvent(v,&e)==1 && v->equationPage==0);
    if (getenv("GUI_CAPTURE")) {
        SDL_SetWindowSize(w, 1640, 780);
        SDL_PumpEvents();
        v->equationPage = atoi(getenv("GUI_CAPTURE"));
        if(v->equationPage==6) {
            SynthConfig picture=synthPresetConfig(69); picture.layers[0].fm.operators[2].feedback=.3;
            spectrogramSetConfig(v,&picture); CHECK(sdlSynthConfigure(&picture)==0); v->selectedOperator=2;
        }
        CHECK(spectrogramDraw(v) == 0);
        SDL_Surface *surface = SDL_CreateRGBSurfaceWithFormat(0, 1640, 780, 32, SDL_PIXELFORMAT_ARGB8888);
        CHECK(surface != NULL);
        CHECK(SDL_RenderReadPixels(v->renderer, NULL, surface->format->format, surface->pixels, surface->pitch) == 0);
        CHECK(SDL_SaveBMP(surface, "/tmp/synth-gui.bmp") == 0);
        SDL_FreeSurface(surface);
    }
    spectrogramDestroy(v);
    sdlSynthShutdown();
    SDL_DestroyWindow(w);
    SDL_Quit();
    char userFolder[1024]; snprintf(userFolder,sizeof(userFolder),"%s/user",presetFolder);
    CHECK(TEST_RMDIR(userFolder)==0 && TEST_RMDIR(presetFolder)==0);
    puts("Resized clicks, dropdown scrolling and selection, and knob drag checks passed.");
    return 0;
}
