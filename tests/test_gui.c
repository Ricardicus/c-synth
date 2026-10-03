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
        click(v, 100, 620);
        CHECK(v->presetOpen);
        click(v, 100, PRESET_LIST_Y + 35);
        CHECK(v->preset == 1 && !v->presetOpen);
        CHECK(v->config.layers[0].fm.operatorCount == 1);
    }
    /* Wheel scroll reaches the final preset; selection replaces the sound. */
    click(v, 100, 620);
    SDL_Event e = {0};
    e.type = SDL_MOUSEWHEEL; e.wheel.y = -1;
    for (int i = 0; i < 10; ++i) CHECK(spectrogramEvent(v, &e) == 1);
    CHECK(v->presetScroll == SYNTH_PRESET_COUNT - PRESET_VISIBLE);
    click(v, 100, PRESET_LIST_Y + 7 * PRESET_ITEM_H + 10);
    CHECK(v->preset == 11 && !v->presetOpen);
    /* Upward dragging increases gain; release ends the drag. */
    v->config.layers[0].gain = .5;
    CHECK(synthConfigure(&v->config) == 0);
    click(v, 1112, 310);
    e.type = SDL_MOUSEMOTION; e.motion.x = 1112; e.motion.y = 270;
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
    CHECK(v->config.outputEnvelope.attackMs == before[0] + 100);
    CHECK(v->config.outputEnvelope.decayMs == before[1] + 250);
    CHECK(v->config.outputEnvelope.sustainPercent == 100); /* Clamped. */
    CHECK(v->config.outputEnvelope.releaseMs == before[3] + 250);
    /* Escape dismisses a dropdown, and arrows/Enter commit one selection. */
    click(v, 100, 620);
    e.type = SDL_KEYDOWN; e.key.keysym.sym = SDLK_ESCAPE;
    CHECK(spectrogramEvent(v, &e) == 1 && !v->presetOpen);
    click(v, 100, 620);
    e.key.keysym.sym = SDLK_END;
    CHECK(spectrogramEvent(v, &e) == 1);
    e.key.keysym.sym = SDLK_RETURN;
    CHECK(spectrogramEvent(v, &e) == 1 && v->preset == 11);
    CHECK(spectrogramDraw(v) == 0);
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
    click(v, 570, 620);
    CHECK(v->equationPage == 1);
    CHECK(spectrogramDraw(v) == 0);
    click(v, 620, 35);
    CHECK(v->equationPage == 2);
    CHECK(spectrogramDraw(v) == 0);
    e.type = SDL_KEYDOWN; e.key.keysym.sym = SDLK_ESCAPE;
    CHECK(spectrogramEvent(v, &e) == 1 && v->equationPage == 0);
    CHECK(spectrogramDraw(v) == 0);
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
    puts("Resized clicks, dropdown scrolling and selection, and knob drag checks passed.");
    return 0;
}
