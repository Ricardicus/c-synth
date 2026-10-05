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
    CHECK(synthInit() == 0);
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
    for (int i = 0; i < 10; ++i) CHECK(spectrogramEvent(v, &e) == 1);
    CHECK(v->presetScroll == SYNTH_PRESET_COUNT - PRESET_VISIBLE);
    click(v, 100, PRESET_LIST_Y + 7 * PRESET_ITEM_H + 10);
    CHECK(v->preset == 12 && !v->presetOpen);
    CHECK(v->config.layers[1].fm.operators[0].waveform == WAVE_NOISE);
    SynthConfig baseline = synthPresetConfig(11);
    CHECK(synthConfigure(&baseline) == 0);
    spectrogramSetConfig(v, &baseline);
    /* Upward dragging increases gain; release ends the drag. */
    v->config.layers[0].gain = .5;
    CHECK(synthConfigure(&v->config) == 0);
    click(v, 1160, 194);
    e.type = SDL_MOUSEMOTION; e.motion.x = 1160; e.motion.y = 154;
    CHECK(spectrogramEvent(v, &e) == 1);
    CHECK(fabs(v->config.layers[0].gain - .6) < .00001 && v->preset == -1);
    e.type = SDL_MOUSEBUTTONUP; e.button.button = SDL_BUTTON_LEFT;
    CHECK(spectrogramEvent(v, &e) == 1 && v->dragRow == -1);
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
    CHECK(spectrogramEvent(v, &e) == 1 && v->preset == 12);
    CHECK(spectrogramDraw(v) == 0);
    baseline = synthPresetConfig(11);
    CHECK(synthConfigure(&baseline) == 0);
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
    /* Equation views are readable pages, and Escape returns without quitting. */
    click(v, 765, 25);
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
    click(v, 570, 25);
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
    click(v,765,25); CHECK(v->equationPage==4);
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
    if (getenv("GUI_CAPTURE")) {
        SDL_SetWindowSize(w, 1640, 780);
        SDL_PumpEvents();
        v->equationPage = atoi(getenv("GUI_CAPTURE"));
        CHECK(spectrogramDraw(v) == 0);
        SDL_Surface *surface = SDL_CreateRGBSurfaceWithFormat(0, 1640, 780, 32, SDL_PIXELFORMAT_ARGB8888);
        CHECK(surface != NULL);
        CHECK(SDL_RenderReadPixels(v->renderer, NULL, surface->format->format, surface->pixels, surface->pitch) == 0);
        CHECK(SDL_SaveBMP(surface, "/tmp/synth-gui.bmp") == 0);
        SDL_FreeSurface(surface);
    }
    spectrogramDestroy(v);
    synthShutdown();
    SDL_DestroyWindow(w);
    SDL_Quit();
    char userFolder[1024]; snprintf(userFolder,sizeof(userFolder),"%s/user",presetFolder);
    CHECK(TEST_RMDIR(userFolder)==0 && TEST_RMDIR(presetFolder)==0);
    puts("Resized clicks, dropdown scrolling and selection, and knob drag checks passed.");
    return 0;
}
