#include "spectrogram.h"
#include "spectrum.h"
#include "synth.h"
#include "presets.h"
#include "midi.h"
#include "file_browser.h"

#include <ft2build.h>
#include FT_FREETYPE_H
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PLOT_X 80
#define PLOT_Y 166
#define PLOT_W 900
#define PLOT_H 329
#define MIN_HZ 40.0
#ifndef SYNTH_DEFAULT_PRESET_DIR
#define SYNTH_DEFAULT_PRESET_DIR "presets"
#endif

_Static_assert(SPECTRUM_SIZE == SYNTH_ANALYSIS_SAMPLES, "Snapshot must fit FFT");

struct Spectrogram {
    SDL_Window *window;
    SDL_Texture *font;
    int equationPage;
    int equationScroll;
    double equationFrequency;
    MidiPlayer midi;
    FileBrowser browser;
    bool browserOpen;
    char midiPath[BROWSER_PATH_MAX];
    char midiError[128];
    PresetLibrary library;
    int presetDialog; /* 1 save, 2 delete */
    char presetName[PRESET_NAME_MAX + 1];
    char presetError[128];
    SynthConfig config;
    int selectedLayer;
    int selectedOperator;
    int preset;
    bool presetOpen;
    int presetScroll;
    int presetHighlight;
    int dragRow;
    int dragY;
    double dragRemainder;
    SDL_Renderer *renderer;
    SDL_Texture *texture;
    Spectrum spectrum;
    float samples[SPECTRUM_SIZE];
    int cursor;
    int layerCount;
    uint64_t lastPosition;
};

static const char *presetName(const Spectrogram *view, int index)
{
    return index >= 0 && index < view->library.count ? view->library.items[index].name : "Custom";
}

static void midiNote(void *context, int note, bool on, int velocity)
{
    Spectrogram *view = context;
    if (on) { registerMidiNoteWithVelocity(note, velocity); spectrogramNote(view, note); }
    else deregisterMidiNote(note);
}
static uint64_t midiNow(void) { return SDL_GetTicks64() * 1000; }

/* Rasterize at twice the displayed size for smooth text on HiDPI displays. */
static SDL_Texture *createFont(SDL_Renderer *renderer)
{
    FT_Library library;
    FT_Face face = NULL;
    if (FT_Init_FreeType(&library)) { SDL_SetError("Cannot initialize FreeType"); return NULL; }
    const char *paths[] = {
        getenv("SYNTH_FONT"), "/System/Library/Fonts/Menlo.ttc",
        "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf",
        "/usr/share/fonts/truetype/liberation2/LiberationMono-Regular.ttf",
        "C:/Windows/Fonts/consola.ttf"
    };
    for (unsigned int i = 0; i < sizeof(paths) / sizeof(paths[0]); ++i)
        if (paths[i] && FT_New_Face(library, paths[i], 0, &face) == 0) break;
    if (!face) { FT_Done_FreeType(library); SDL_SetError("No monospace font found; set SYNTH_FONT to a TTF file"); return NULL; }
    SDL_Surface *atlas = SDL_CreateRGBSurfaceWithFormat(0, 384, 288, 32, SDL_PIXELFORMAT_ARGB8888);
    if (!atlas) { FT_Done_Face(face); FT_Done_FreeType(library); return NULL; }
    SDL_FillRect(atlas, NULL, 0);
    FT_Set_Pixel_Sizes(face, 0, 40);
    for (int ch = 32; ch < 127; ++ch) {
        if (FT_Load_Char(face, (FT_ULong)ch, FT_LOAD_RENDER)) continue;
        FT_GlyphSlot g = face->glyph;
        int ax = ((ch - 32) % 16) * 24, ay = ((ch - 32) / 16) * 48;
        for (unsigned int y = 0; y < g->bitmap.rows; ++y) {
            for (unsigned int x = 0; x < g->bitmap.width; ++x) {
                int dx = g->bitmap_left + (int)x, dy = 38 - g->bitmap_top + (int)y;
                if (dx < 0 || dx >= 24 || dy < 0 || dy >= 48) continue;
                Uint8 alpha = g->bitmap.buffer[(int)y * g->bitmap.pitch + x];
                Uint32 *row = (Uint32 *)((Uint8 *)atlas->pixels + (ay + dy) * atlas->pitch);
                row[ax + dx] = ((Uint32)alpha << 24) | 0x00ffffff;
            }
        }
    }
    SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, atlas);
    if (texture) {
        SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
        SDL_SetTextureScaleMode(texture, SDL_ScaleModeLinear);
    }
    SDL_FreeSurface(atlas);
    FT_Done_Face(face);
    FT_Done_FreeType(library);
    return texture;
}

static void textScaled(Spectrogram *view, int x, int y, const char *value, int scale)
{
    Uint8 red, green, blue, alpha;
    SDL_GetRenderDrawColor(view->renderer, &red, &green, &blue, &alpha);
    SDL_SetTextureColorMod(view->font, red, green, blue);
    SDL_SetTextureAlphaMod(view->font, alpha);
    int width = scale == 3 ? 18 : 12, height = scale == 3 ? 36 : 24;
    for (; *value; ++value, x += width) {
        unsigned char ch = (unsigned char)*value;
        if (ch < 32 || ch > 126) ch = '?';
        SDL_Rect src = {((ch - 32) % 16) * 24, ((ch - 32) / 16) * 48, 24, 48};
        SDL_Rect dst = {x, y, width, height};
        SDL_RenderCopy(view->renderer, view->font, &src, &dst);
    }
}

static void text(Spectrogram *view, int x, int y, const char *value)
{
    textScaled(view, x, y, value, 2);
}

static Uint32 color(double db)
{
    static const int stops[6][3] = {
        {2,4,12}, {23,17,70}, {91,32,151}, {20,173,192}, {248,159,44}, {255,249,204}
    };
    double position = fmax(0.0, fmin(1.0, (db + 80.0) / 80.0)) * 5.0;
    int first = (int)position;
    int next = first < 5 ? first + 1 : 5;
    double blend = position - first;
    Uint32 packed = 0xff000000u;
    for (int channel = 0; channel < 3; ++channel) {
        Uint32 value = (Uint32)(stops[first][channel] +
                       (stops[next][channel] - stops[first][channel]) * blend);
        packed |= value << (16 - channel * 8);
    }
    return packed;
}

Spectrogram *spectrogramCreate(SDL_Window *window, int layerCount)
{
    Spectrogram *view = calloc(1, sizeof(*view));
    if (view == NULL) {
        SDL_OutOfMemory();
        return NULL;
    }
    midiPlayerInit(&view->midi, midiNote, view);
    view->window = window;
    view->config = synthDefaultConfig();
    view->preset = -1;
    view->dragRow = -1;
    view->equationFrequency = 440.0;
    view->layerCount = layerCount;
    const char *folder = getenv("SYNTH_PRESET_DIR");
    char error[128];
    if (presetLibraryInit(&view->library, folder ? folder : SYNTH_DEFAULT_PRESET_DIR, error, sizeof(error)) != 0) {
        SDL_SetError("%s", error); free(view); return NULL;
    }
    if (view->library.skipped) fprintf(stderr, "Skipped %d invalid preset files.\n", view->library.skipped);
    view->renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (view->renderer == NULL)
        view->renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
    if (view->renderer == NULL || SDL_RenderSetLogicalSize(view->renderer, 1640, 780) != 0)
        goto failure;
    view->font = createFont(view->renderer);
    if (!view->font) goto failure;
    view->texture = SDL_CreateTexture(view->renderer, SDL_PIXELFORMAT_ARGB8888,
                                     SDL_TEXTUREACCESS_STREAMING, PLOT_W, PLOT_H);
    if (view->texture == NULL)
        goto failure;
    void *pixels;
    int pitch;
    if (SDL_LockTexture(view->texture, NULL, &pixels, &pitch) != 0)
        goto failure;
    for (int y = 0; y < PLOT_H; ++y) {
        Uint32 *row = (Uint32 *)((Uint8 *)pixels + y * pitch);
        for (int x = 0; x < PLOT_W; ++x)
            row[x] = color(-90.0);
    }
    SDL_UnlockTexture(view->texture);
    spectrumInit(&view->spectrum);
    return view;
failure:
    spectrogramDestroy(view);
    return NULL;
}

static void drawControls(Spectrogram *view);
static void drawEquations(Spectrogram *view);
static void drawPresetDialog(Spectrogram *view);
static void drawFileBrowser(Spectrogram *view);

int spectrogramDraw(Spectrogram *view)
{
    midiPlayerTick(&view->midi, midiNow());
    uint64_t position = 0;
    int sampleRate = synthAudioSnapshot(view->samples, &position);
    if (sampleRate == 0)
        return SDL_SetError("Audio stopped during spectrogram rendering");
    double maxHz = fmin(20000.0, sampleRate / 2.0);
    if (position >= SPECTRUM_SIZE && position - view->lastPosition >= SPECTRUM_SIZE / 2) {
        spectrumCompute(&view->spectrum, view->samples);
        SDL_Rect column = {view->cursor, 0, 1, PLOT_H};
        void *pixels;
        int pitch;
        if (SDL_LockTexture(view->texture, &column, &pixels, &pitch) != 0)
            return -1;
        for (int y = 0; y < PLOT_H; ++y) {
            double hz = MIN_HZ * pow(maxHz / MIN_HZ, 1.0 - (double)y / (PLOT_H - 1));
            double bin = fmin(SPECTRUM_BINS - 1, hz * SPECTRUM_SIZE / sampleRate);
            int lower = (int)bin;
            int upper = lower < SPECTRUM_BINS - 1 ? lower + 1 : lower;
            double db = view->spectrum.db[lower] + (bin - lower) *
                        (view->spectrum.db[upper] - view->spectrum.db[lower]);
            /* At high frequencies one display row spans several FFT bins.
             * Keep the strongest one so narrow harmonics stay visible. */
            double step = pow(maxHz / MIN_HZ, 0.5 / (PLOT_H - 1));
            int first = (int)ceil((hz / step) * SPECTRUM_SIZE / sampleRate);
            int last = (int)floor(fmin(maxHz, hz * step) * SPECTRUM_SIZE / sampleRate);
            if (first <= last) {
                db = -90.0;
                for (int i = first; i <= last && i < SPECTRUM_BINS; ++i)
                    db = fmax(db, view->spectrum.db[i]);
            }
            *(Uint32 *)((Uint8 *)pixels + y * pitch) = color(db);
        }
        SDL_UnlockTexture(view->texture);
        view->cursor = (view->cursor + 1) % PLOT_W;
        view->lastPosition = position;
    }

    SDL_Renderer *renderer = view->renderer;
    SDL_SetRenderDrawColor(renderer, 12, 16, 26, 255);
    if (SDL_RenderClear(renderer) != 0)
        return -1;
    if (view->equationPage) {
        drawEquations(view);
        if (view->browserOpen) drawFileBrowser(view);
        SDL_RenderPresent(renderer);
        return 0;
    }
    SDL_Rect source = {view->cursor, 0, PLOT_W - view->cursor, PLOT_H};
    SDL_Rect target = {PLOT_X, PLOT_Y, source.w, PLOT_H};
    if (SDL_RenderCopy(renderer, view->texture, &source, &target) != 0)
        return -1;
    if (view->cursor != 0) {
        source = (SDL_Rect){0, 0, view->cursor, PLOT_H};
        target = (SDL_Rect){PLOT_X + PLOT_W - view->cursor, PLOT_Y, source.w, PLOT_H};
        if (SDL_RenderCopy(renderer, view->texture, &source, &target) != 0)
            return -1;
    }
    SDL_SetRenderDrawColor(renderer, 190, 204, 222, 255);
    text(view, PLOT_X, 110, "LIVE OUTPUT SPECTROGRAM");
    char status[80];
    snprintf(status, sizeof(status), "%d LAYERS   %d HZ   FREQUENCY UP", view->layerCount, sampleRate);
    text(view, PLOT_X, 136, status);
    const int ticks[] = {40,100,200,500,1000,2000,5000,10000,20000};
    for (unsigned int i = 0; i < sizeof(ticks) / sizeof(ticks[0]); ++i) {
        if (ticks[i] > maxHz)
            continue;
        int y = PLOT_Y + (int)((1.0 - log(ticks[i] / MIN_HZ) / log(maxHz / MIN_HZ)) * (PLOT_H - 1));
        char label[16];
        snprintf(label, sizeof(label), "%d", ticks[i]);
        SDL_SetRenderDrawColor(renderer, 145, 159, 182, 255);
        text(view, 8, y - 12, label);
        SDL_RenderDrawLine(renderer, PLOT_X - 5, y, PLOT_X - 1, y);
    }
    SDL_SetRenderDrawColor(renderer, 190, 204, 222, 255);
    text(view, PLOT_X, 508, "TIME   OLDER > NEWER");
    for (int i = 0; i < 220; ++i) {
        Uint32 rgb = color(-80.0 + 80.0 * i / 219);
        SDL_SetRenderDrawColor(renderer, (Uint8)(rgb >> 16), (Uint8)(rgb >> 8), (Uint8)rgb, 255);
        SDL_RenderDrawLine(renderer, 700 + i, 510, 700 + i, 524);
    }
    SDL_SetRenderDrawColor(renderer, 190, 204, 222, 255);
    text(view, 620, 510, "-80 DB");
    text(view, 932, 510, "0 DB");
    text(view, PLOT_X, 550, "Z-M A-L Q-P PLAY - ESC QUITS");
    drawControls(view);
    if (view->presetDialog) drawPresetDialog(view);
    SDL_RenderPresent(renderer);
    return 0;
}

void spectrogramDestroy(Spectrogram *view)
{
    if (view == NULL)
        return;
    if (view->texture != NULL)
        SDL_DestroyTexture(view->texture);
    if (view->font != NULL)
        SDL_DestroyTexture(view->font);
    if (view->renderer != NULL)
        SDL_DestroyRenderer(view->renderer);
    if (view->presetDialog == 1) SDL_StopTextInput();
    midiPlayerDestroy(&view->midi);
    presetLibraryDestroy(&view->library);
    free(view);
}

/* Controls share their geometry between drawing and hit testing. */
#define PANEL_X 1020
#define CONTROL_ROWS 28
#define SOUND_KNOBS 16
#define KNOB_COUNT 22
#define PRESET_VISIBLE 8
#define PRESET_ITEM_H 30
#define PRESET_LIST_Y 48

static const SDL_Rect presetBox = {80, 10, 450, 38};
static const SDL_Rect previousPresetButton = {30, 10, 38, 38};
static const SDL_Rect nextPresetButton = {540, 10, 38, 38};
static const int knobRows[KNOB_COUNT] = {2, 3, 7, 8, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27};
static const int selectorRows[6] = {0, 1, 4, 5, 6, 9};

static const char *rowNames[CONTROL_ROWS] = {
    "Layers", "Selected layer", "Layer gain", "Detune cents",
    "Operators", "Selected operator", "Waveform", "Ratio", "FM depth",
    "Index envelope", "Index decay rate", "Attack ms", "Decay ms",
    "Sustain percent", "Release ms", "Pulse width", "Vibrato Hz",
    "Vibrato cents", "Attack ms", "Decay ms", "Sustain percent", "Release ms",
    "Echo mix", "Echo delay ms", "Echo feedback", "Reverb mix", "Room size", "Damping"
};

void spectrogramSetConfig(Spectrogram *view, const SynthConfig *config)
{
    view->config = *config;
    view->layerCount = config->layerCount;
    view->selectedLayer = 0;
    view->selectedOperator = 0;
    view->preset = -1;
}

static void rowValue(Spectrogram *view, int row, char *value, size_t size)
{
    SynthLayerConfig *layer = &view->config.layers[view->selectedLayer];
    FmOperatorConfig *op = &layer->fm.operators[view->selectedOperator];
    double number = 0;
    switch (row) {
    case 0: number = view->config.layerCount; break;
    case 1: number = view->selectedLayer + 1; break;
    case 2: number = layer->gain; break;
    case 3: number = layer->detuneCents; break;
    case 4: number = layer->fm.operatorCount; break;
    case 5: number = view->selectedOperator + 1; break;
    case 6: snprintf(value, size, "%s", oscillatorWaveformName(op->waveform)); return;
    case 7: number = op->ratio; break;
    case 8: number = op->rm; break;
    case 9: snprintf(value, size, "%s", op->indexMode == FM_INDEX_ADSR ? "ADSR" :
                      op->indexMode == FM_INDEX_DECAY ? "DECAY" : "SUSTAIN"); return;
    case 10: number = op->decayRate; break;
    case 11: number = op->attackMs; break;
    case 12: number = op->decayMs; break;
    case 13: number = op->sustainPercent; break;
    case 14: number = op->releaseMs; break;
    case 15: number = op->pulseWidth; break;
    case 16: number = op->vibratoRateHz; break;
    case 17: number = op->vibratoDepthCents; break;
    case 18: number = view->config.outputEnvelope.attackMs; break;
    case 19: number = view->config.outputEnvelope.decayMs; break;
    case 20: number = view->config.outputEnvelope.sustainPercent; break;
    case 21: number = view->config.outputEnvelope.releaseMs; break;
    case 22: number = view->config.effects.echoMix; break;
    case 23: number = view->config.effects.echoDelayMs; break;
    case 24: number = view->config.effects.echoFeedback; break;
    case 25: number = view->config.effects.reverbMix; break;
    case 26: number = view->config.effects.reverbRoom; break;
    case 27: number = view->config.effects.reverbDamping; break;
    default: value[0] = 0; return;
    }
    snprintf(value, size, "%.2f", number);
}

static double adjust(double value, double step, double direction, double low, double high)
{
    return fmax(low, fmin(high, value + step * direction));
}

static bool envelopeTimeKnob(int row)
{
    return row == 11 || row == 12 || row == 14 || row == 18 || row == 19 || row == 21;
}

static bool integerKnob(int row)
{
    return row == 11 || row == 12 || row == 13 || row == 14 || (row >= 18 && row <= 21);
}

static int changeRow(Spectrogram *view, int row, double direction)
{
    SynthConfig old = view->config;
    SynthLayerConfig *layer = &view->config.layers[view->selectedLayer];
    FmOperatorConfig *op = &layer->fm.operators[view->selectedOperator];
    switch (row) {
    case 0: view->config.layerCount = (int)adjust(view->config.layerCount, 1, direction, 1, SYNTH_MAX_LAYERS); break;
    case 1: view->selectedLayer = (int)adjust(view->selectedLayer, 1, direction, 0, view->config.layerCount - 1); break;
    case 2: layer->gain = adjust(layer->gain, .01, direction, 0, 1); break;
    case 3: layer->detuneCents = adjust(layer->detuneCents, .1, direction, -4800, 4800); break;
    case 4: layer->fm.operatorCount = (int)adjust(layer->fm.operatorCount, 1, direction, 1, FM_MAX_OPERATORS); break;
    case 5: view->selectedOperator = (int)adjust(view->selectedOperator, 1, direction, 0, layer->fm.operatorCount - 1); break;
    case 6: op->waveform = (Waveform)((op->waveform + (int)direction + WAVE_COUNT) % WAVE_COUNT); break;
    case 7: op->ratio = adjust(op->ratio, .01, direction, .01, 32); break;
    case 8: op->rm = adjust(op->rm, .01, direction, 0, 32); break;
    case 9: op->indexMode = (FmIndexMode)((op->indexMode + (int)direction + 3) % 3); break;
    case 10: op->decayRate = adjust(op->decayRate, .01, direction, 0, 100); break;
    case 11: op->attackMs = (int)adjust(op->attackMs, 1, direction, 0, 10000); break;
    case 12: op->decayMs = (int)adjust(op->decayMs, 1, direction, 0, 10000); break;
    case 13: op->sustainPercent = (int)adjust(op->sustainPercent, 1, direction, 0, 100); break;
    case 14: op->releaseMs = (int)adjust(op->releaseMs, 1, direction, 0, 10000); break;
    case 15: op->pulseWidth = adjust(op->pulseWidth, .01, direction, .05, .95); break;
    case 16: op->vibratoRateHz = adjust(op->vibratoRateHz, .01, direction, 0, 50); break;
    case 17: op->vibratoDepthCents = adjust(op->vibratoDepthCents, .1, direction, 0, 1200); break;
    case 18: view->config.outputEnvelope.attackMs = (int)adjust(view->config.outputEnvelope.attackMs, 1, direction, 0, 10000); break;
    case 19: view->config.outputEnvelope.decayMs = (int)adjust(view->config.outputEnvelope.decayMs, 1, direction, 0, 10000); break;
    case 20: view->config.outputEnvelope.sustainPercent = (int)adjust(view->config.outputEnvelope.sustainPercent, 1, direction, 0, 100); break;
    case 21: view->config.outputEnvelope.releaseMs = (int)adjust(view->config.outputEnvelope.releaseMs, 1, direction, 0, 10000); break;
    case 22: view->config.effects.echoMix = adjust(view->config.effects.echoMix, .01, direction, 0, 1); break;
    case 23: view->config.effects.echoDelayMs = adjust(view->config.effects.echoDelayMs, 1, direction, 1, 2000); break;
    case 24: view->config.effects.echoFeedback = adjust(view->config.effects.echoFeedback, .01, direction, 0, .95); break;
    case 25: view->config.effects.reverbMix = adjust(view->config.effects.reverbMix, .01, direction, 0, 1); break;
    case 26: view->config.effects.reverbRoom = adjust(view->config.effects.reverbRoom, .01, direction, 0, .95); break;
    case 27: view->config.effects.reverbDamping = adjust(view->config.effects.reverbDamping, .01, direction, 0, 1); break;
    default: return 0;
    }
    view->selectedLayer = (int)fmin(view->selectedLayer, view->config.layerCount - 1);
    view->selectedOperator = (int)fmin(view->selectedOperator,
        view->config.layers[view->selectedLayer].fm.operatorCount - 1);
    if (row == 1 || row == 5) return 0;
    if (synthConfigure(&view->config) != 0) { view->config = old; return -1; }
    view->layerCount = view->config.layerCount;
    view->preset = -1;
    return 0;
}

static void button(Spectrogram *view, SDL_Rect rect, const char *label, bool active)
{
    SDL_Renderer *r = view->renderer;
    SDL_SetRenderDrawColor(r, active ? 39 : 30, active ? 104 : 42, active ? 126 : 60, 255);
    SDL_RenderFillRect(r, &rect);
    SDL_SetRenderDrawColor(r, 220, 232, 245, 255);
    text(view, rect.x + 8, rect.y + 1, label);
}

static void disabledButton(Spectrogram *view, SDL_Rect rect, const char *label)
{
    SDL_SetRenderDrawColor(view->renderer,23,31,44,255);
    SDL_RenderFillRect(view->renderer,&rect);
    SDL_SetRenderDrawColor(view->renderer,100,115,134,255);
    text(view,rect.x+8,rect.y+1,label);
}

static SDL_Rect knobRect(int index)
{
    if (index >= SOUND_KNOBS) return (SDL_Rect){220 + ((index - SOUND_KNOBS) % 3) * 400, 180 + ((index - SOUND_KNOBS) / 3) * 180, 240, 104};
    if (index >= 12) return (SDL_Rect){80 + (index - 12) * 225, 670, 210, 104};
    if (index < 2) return (SDL_Rect){PANEL_X + index * 300, 142, 280, 104};
    int operatorIndex = index - 2;
    return (SDL_Rect){PANEL_X + (operatorIndex % 3) * 195,
                      358 + (operatorIndex / 3) * 104, 185, 104};
}

static SDL_Rect selectorRect(int index)
{
    int y = index < 2 ? 86 : 252 + ((index - 2) / 2) * 54;
    return (SDL_Rect){PANEL_X + (index % 2) * 300, y, 280, 50};
}

static bool inside(int x, int y, SDL_Rect rect)
{
    return x >= rect.x && x < rect.x + rect.w && y >= rect.y && y < rect.y + rect.h;
}

static void circle(SDL_Renderer *r, int x, int y, int radius)
{
    for (int dy = -radius; dy <= radius; ++dy) {
        int dx = (int)sqrt(radius * radius - dy * dy);
        SDL_RenderDrawLine(r, x - dx, y + dy, x + dx, y + dy);
    }
}

static void drawKnob(Spectrogram *view, int index)
{
    static const double lows[KNOB_COUNT] = {0, -4800, .01, 0, 0, 0, 0, 0, 0, .05, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0};
    static const double highs[KNOB_COUNT] = {1, 4800, 32, 32, 100, 10000, 10000, 100, 10000, .95, 50, 1200, 10000, 10000, 100, 10000, 1, 2000, .95, 1, .95, 1};
    int row = knobRows[index];
    SDL_Rect rect = knobRect(index);
    SDL_Renderer *r = view->renderer;
    char value[32];
    rowValue(view, row, value, sizeof(value));
    double number = strtod(value, NULL);
    double position = fmax(0, fmin(1, (number - lows[index]) / (highs[index] - lows[index])));
    /* Compress large ranges so useful low values occupy more of the dial. */
    if (row == 7 || row == 8 || row == 10 || row == 11 || row == 12 || row == 14 || row == 16 || row == 18 || row == 19 || row == 21)
        position = log1p(position * 99) / log(100);
    int cx = rect.x + rect.w / 2, cy = rect.y + 52;
    SDL_SetRenderDrawColor(r, 180, 199, 219, 255);
    text(view, rect.x + (rect.w - (int)strlen(rowNames[row]) * 12) / 2, rect.y, rowNames[row]);
    SDL_SetRenderDrawColor(r, 44, 61, 80, 255);
    circle(r, cx, cy, 29);
    SDL_SetRenderDrawColor(r, view->dragRow == row ? 57 : 29, view->dragRow == row ? 160 : 99, 126, 255);
    circle(r, cx, cy, 24);
    double angle = (135 + position * 270) * 3.141592653589793 / 180;
    SDL_SetRenderDrawColor(r, 231, 241, 252, 255);
    SDL_RenderDrawLine(r, cx, cy, cx + (int)(cos(angle) * 21), cy + (int)(sin(angle) * 21));
    text(view, rect.x + (rect.w - (int)strlen(value) * 12) / 2, rect.y + 84, value);
}

static const SDL_Rect savePresetButton = {80, 58, 215, 38};
static const SDL_Rect deletePresetButton = {315, 58, 215, 38};
static const SDL_Rect effectsPageButton = {598, 10, 155, 38};
static const SDL_Rect midiPageButton = {773, 10, 207, 38};
static const SDL_Rect midiBrowseButton = {100, 170, 235, 42};
static const SDL_Rect midiPlayButton = {100, 290, 160, 42};
static const SDL_Rect midiStopButton = {290, 290, 160, 42};
static const SDL_Rect midiRestartButton = {480, 290, 160, 42};
static const SDL_Rect soundPageButton = {80, 20, 225, 42};

typedef struct {
    int count;
    char lines[256][512];
} EquationLines;

static void addEquation(EquationLines *lines, const char *value)
{
    /* Wrap at operators rather than reducing the font size. */
    while (*value && lines->count < 256) {
        size_t length = strlen(value), take = length;
        if (take > 118) {
            take = 118;
            while (take > 60 && value[take] != ' ') --take;
        }
        snprintf(lines->lines[lines->count++], 512, "%.*s", (int)take, value);
        value += take;
        while (*value == ' ') ++value;
    }
}

static void waveformFunction(const FmOperatorConfig *op, int layer, int index,
                             char *value, size_t size)
{
    const char *name = op->waveform == WAVE_SINE ? "sin" : oscillatorWaveformName(op->waveform);
    if (op->waveform == WAVE_NOISE)
        snprintf(value, size, "n%d_%d(t)", layer, index);
    else if (op->waveform == WAVE_PULSE)
        snprintf(value, size, "pulse(p%d_%d(t), %.2f)", layer, index, op->pulseWidth);
    else
        snprintf(value, size, "%s(p%d_%d(t))", name, layer, index);
}

static void fmFunctions(const Spectrogram *view, EquationLines *lines)
{
    char line[512], wave[80], previousWave[80], index[120], vibrato[120];
    snprintf(line, sizeof(line), "x(t) = (0.10 / %.2f) * (", (double)view->config.layerCount);
    addEquation(lines, line);
    for (int l = 0; l < view->config.layerCount; ++l) {
        const SynthLayerConfig *layer = &view->config.layers[l];
        waveformFunction(&layer->fm.operators[layer->fm.operatorCount - 1], l + 1,
                         layer->fm.operatorCount, wave, sizeof(wave));
        snprintf(line, sizeof(line), "    %s%.2f * %s", l ? "+ " : "", layer->gain, wave);
        addEquation(lines, line);
    }
    addEquation(lines, ")");
    for (int l = 0; l < view->config.layerCount; ++l) {
        const SynthLayerConfig *layer = &view->config.layers[l];
        double base = view->equationFrequency * exp2(layer->detuneCents / 1200.0);
        addEquation(lines, " ");
        for (int k = 0; k < layer->fm.operatorCount; ++k) {
            const FmOperatorConfig *op = &layer->fm.operators[k];
            if (op->waveform == WAVE_NOISE) {
                snprintf(line, sizeof(line), "n%d_%d(t) = uniform(-1.00, 1.00)", l + 1, k + 1);
                addEquation(lines, line);
                continue;
            }
            double frequency = base * op->ratio;
            if (k == 0 || layer->fm.operators[k - 1].rm == 0) {
                snprintf(line, sizeof(line), "p%d_%d'(t) = 2.00*pi * (%.2f)", l + 1, k + 1, frequency);
            } else {
                const FmOperatorConfig *prev = &layer->fm.operators[k - 1];
                waveformFunction(prev, l + 1, k, previousWave, sizeof(previousWave));
                if (prev->indexMode == FM_INDEX_DECAY)
                    snprintf(index, sizeof(index), "exp(-%.2f*t)", prev->decayRate);
                else if (prev->indexMode == FM_INDEX_ADSR)
                    snprintf(index, sizeof(index), "adsr(1000.00*t, %.2f, %.2f, %.2f, %.2f)",
                             (double)prev->attackMs, (double)prev->decayMs,
                             prev->sustainPercent / 100.0, (double)prev->releaseMs);
                else
                    snprintf(index, sizeof(index), "1.00");
                snprintf(line, sizeof(line), "p%d_%d'(t) = 2.00*pi * (%.2f + %.2f * %s * %s)",
                         l + 1, k + 1, frequency, prev->rm * base * prev->ratio, index, previousWave);
            }
            if (op->vibratoDepthCents > 0) {
                snprintf(vibrato, sizeof(vibrato), " * 2.00^(%.2f*sin(2.00*pi*%.2f*t)/1200.00)",
                         op->vibratoDepthCents, op->vibratoRateHz);
                size_t used = strlen(line);
                snprintf(line + used, sizeof(line) - used, "%s", vibrato);
            }
            addEquation(lines, line);
        }
    }
}

static void outputFunctions(const Spectrogram *view, EquationLines *lines)
{
    const SynthEnvelopeConfig *env = &view->config.outputEnvelope;
    double a = env->attackMs, d = env->decayMs, s = env->sustainPercent / 100.0, r = env->releaseMs;
    char line[512];
    addEquation(lines, "y(t) = e(1000.00*t) * x(t)");
    addEquation(lines, " ");
    addEquation(lines, "e(t) = {");
    if (a > 0) {
        snprintf(line, sizeof(line), "    t / %.2f,                         0.00 <= t < %.2f, t < t_r", a, a);
        addEquation(lines, line);
    }
    if (d > 0) {
        snprintf(line, sizeof(line), "    1.00 - %.2f*(t-%.2f)/%.2f,       %.2f <= t < %.2f, t < t_r",
                 1 - s, a, d, a, a + d);
        addEquation(lines, line);
    }
    snprintf(line, sizeof(line), "    %.2f,                             %.2f <= t < t_r", s, a + d);
    addEquation(lines, line);
    if (r > 0) {
        snprintf(line, sizeof(line), "    e(t_r-)*(1.00-(t-t_r)/%.2f),       t_r <= t < t_r+%.2f", r, r);
        addEquation(lines, line);
    }
    addEquation(lines, "    0.00,                             otherwise");
    addEquation(lines, "}");
}

static void drawEquations(Spectrogram *view)
{
    button(view, soundPageButton, "Back to sound", false);
    button(view, (SDL_Rect){340, 20, 225, 42}, "FM equations", view->equationPage == 1);
    button(view, (SDL_Rect){600, 20, 225, 42}, "Output ADSR", view->equationPage == 2);
    button(view, (SDL_Rect){860,20,225,42}, "Effects", view->equationPage == 3);
    button(view, (SDL_Rect){1120,20,225,42}, "MIDI player", view->equationPage == 4);
    if (view->equationPage == 4) {
        SDL_SetRenderDrawColor(view->renderer, 101,218,233,255);
        textScaled(view,100,95,"MIDI files",3);
        button(view,midiBrowseButton,"Browse MIDI file",false);
        SDL_SetRenderDrawColor(view->renderer,232,239,249,255);
        const char *name = strrchr(view->midiPath,'/');
        name = name ? name+1 : view->midiPath;
        char label[128]; snprintf(label,sizeof(label),"%.110s",*name ? name : "No MIDI file loaded");
        text(view,100,235,label);
        if (view->midi.song.count) {
            button(view,midiPlayButton,"Play",view->midi.playing);
            button(view,midiStopButton,"Stop",false);
            button(view,midiRestartButton,"Restart",false);
        } else {
            disabledButton(view,midiPlayButton,"Play"); disabledButton(view,midiStopButton,"Stop");
            disabledButton(view,midiRestartButton,"Restart");
        }
        double seconds=view->midi.positionUs/1000000.0, total=view->midi.song.durationUs/1000000.0;
        snprintf(label,sizeof(label),"%s   %.2f / %.2f seconds",view->midi.playing ? "Playing" : "Stopped",seconds,total);
        text(view,100,380,label);
        SDL_Rect track={100,430,1340,12};
        SDL_SetRenderDrawColor(view->renderer,38,56,74,255); SDL_RenderFillRect(view->renderer,&track);
        if (total>0) {
            SDL_Rect progress=track; progress.w=(int)(track.w*seconds/total);
            SDL_SetRenderDrawColor(view->renderer,101,218,233,255); SDL_RenderFillRect(view->renderer,&progress);
        }
        SDL_SetRenderDrawColor(view->renderer,255,175,150,255); text(view,100,485,view->midiError);
        SDL_SetRenderDrawColor(view->renderer,210,225,240,255);
        text(view,100,545,"Play resumes. Stop pauses. Restart plays from the beginning.");
        text(view,100,585,"The file plays through the current synth settings and effects.");
        return;
    }
    if (view->equationPage == 3) {
        SDL_SetRenderDrawColor(view->renderer, 101, 218, 233, 255);
        textScaled(view, 100, 95, "Echo and reverb", 3);
        for (int i = SOUND_KNOBS; i < KNOB_COUNT; ++i) drawKnob(view, i);
        SDL_SetRenderDrawColor(view->renderer, 210, 225, 240, 255);
        text(view, 220, 540, "Dry signal -> echo -> reverb -> output");
        text(view, 220, 585, "Mix 0.00 bypasses that effect.");
        text(view, 220, 630, "Hold Shift for fine control.");
        return;
    }
    EquationLines lines = {0};
    if (view->equationPage == 2) outputFunctions(view, &lines);
    else fmFunctions(view, &lines);
    int maximum = lines.count > 16 ? lines.count - 16 : 0;
    view->equationScroll = (int)fmin(view->equationScroll, maximum);
    SDL_SetRenderDrawColor(view->renderer, 232, 239, 249, 255);
    for (int i = 0; i < 16 && i + view->equationScroll < lines.count; ++i)
        text(view, 100, 105 + i * 38, lines.lines[i + view->equationScroll]);
    if (maximum > 0) {
        SDL_Rect track = {1580, 105, 8, 608};
        SDL_SetRenderDrawColor(view->renderer, 36, 52, 69, 255);
        SDL_RenderFillRect(view->renderer, &track);
        SDL_Rect thumb = {1580, 105 + view->equationScroll * 608 / lines.count, 8, 16 * 608 / lines.count};
        SDL_SetRenderDrawColor(view->renderer, 101, 218, 233, 255);
        SDL_RenderFillRect(view->renderer, &thumb);
    }
}

void spectrogramNote(Spectrogram *view, int note)
{
    if (note >= 0 && note < 128)
        view->equationFrequency = 440.0 * exp2((note - 69) / 12.0);
}

static void drawControls(Spectrogram *view)
{
    SDL_Renderer *r = view->renderer;
    SDL_SetRenderDrawColor(r, 220, 232, 245, 255);
    text(view, PANEL_X, 20, "LIVE SOUND CONTROLS");
    text(view, PANEL_X, 48, presetName(view, view->preset));
    for (int i = 0; i < 6; ++i) {
        SDL_Rect rect = selectorRect(i);
        char value[32];
        rowValue(view, selectorRows[i], value, sizeof(value));
        SDL_SetRenderDrawColor(r, 180, 199, 219, 255);
        text(view, rect.x, rect.y, rowNames[selectorRows[i]]);
        button(view, (SDL_Rect){rect.x, rect.y + 24, 36, 25}, "-", false);
        button(view, (SDL_Rect){rect.x + 244, rect.y + 24, 36, 25}, "+", false);
        text(view, rect.x + 48, rect.y + 24, value);
    }
    for (int i = 0; i < SOUND_KNOBS; ++i) drawKnob(view, i);
    SDL_SetRenderDrawColor(r, 145, 165, 186, 255);
    text(view, PANEL_X + 215, 746, "SHIFT FOR FINE CONTROL");
    button(view, presetBox, presetName(view, view->preset), view->presetOpen);
    button(view, previousPresetButton, "<", false);
    button(view, nextPresetButton, ">", false);
    /* Dropdown indicator. */
    for (int i = 0; i < 7; ++i)
        SDL_RenderDrawLine(r, 500 + i, 24 + i, 514 - i, 24 + i);
    text(view, 80, 650, "MASTER OUTPUT ADSR");
    button(view, savePresetButton, "Save setting", false);
    if (view->preset>=0 && !view->library.items[view->preset].builtin)
        button(view,deletePresetButton,"Delete setting",false);
    else disabledButton(view,deletePresetButton,"Delete setting");
    button(view, effectsPageButton, "Effects", false);
    button(view, midiPageButton, "MIDI player", false);
    if (view->presetOpen) {
        for (int i = 0; i < PRESET_VISIBLE && view->presetScroll + i < view->library.count; ++i) {
            int preset = view->presetScroll + i;
            button(view, (SDL_Rect){80, PRESET_LIST_Y + i * PRESET_ITEM_H, 450, PRESET_ITEM_H},
                   presetName(view, preset), preset == view->presetHighlight);
        }
        SDL_Rect track = {518, PRESET_LIST_Y, 10, PRESET_VISIBLE * PRESET_ITEM_H};
        SDL_SetRenderDrawColor(r, 59, 78, 97, 255);
        SDL_RenderFillRect(r, &track);
        SDL_Rect thumb = {518, PRESET_LIST_Y + view->presetScroll * track.h / view->library.count,
                          10, track.h * PRESET_VISIBLE / view->library.count};
        SDL_SetRenderDrawColor(r, 94, 182, 202, 255);
        SDL_RenderFillRect(r, &thumb);
    }
}

static const SDL_Rect dialogOK = {610, 422, 170, 38};
static const SDL_Rect dialogCancel = {810, 422, 170, 38};

static void drawPresetDialog(Spectrogram *view)
{
    SDL_Renderer *r = view->renderer;
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(r, 0, 0, 0, 180);
    SDL_Rect backdrop = {0,0,1640,780}; SDL_RenderFillRect(r, &backdrop);
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
    SDL_SetRenderDrawColor(r, 27,40,56,255);
    SDL_Rect panel = {390,240,860,245}; SDL_RenderFillRect(r, &panel);
    SDL_SetRenderDrawColor(r, 232,239,249,255);
    text(view, 425,260, view->presetDialog == 1 ? "Save current sound" : "Delete saved setting?");
    if (view->presetDialog == 1) {
        SDL_Rect input = {425,302,790,40};
        SDL_SetRenderDrawColor(r, 12,20,30,255); SDL_RenderFillRect(r, &input);
        SDL_SetRenderDrawColor(r, 232,239,249,255);
        text(view, 437,308, view->presetName);
        int x = 437 + (int)strlen(view->presetName) * 12;
        SDL_RenderDrawLine(r,x,308,x,332);
        text(view,425,354,"Name: letters, numbers, spaces, - or _ (max 32)");
    } else text(view,425,310,presetName(view,view->preset));
    SDL_SetRenderDrawColor(r,255,175,150,255);
    text(view,425,390,view->presetError);
    button(view,dialogOK,view->presetDialog == 1 ? "Save" : "Delete",false);
    button(view,dialogCancel,"Cancel",false);
}

static void closePresetDialog(Spectrogram *view)
{
    if (view->presetDialog == 1) SDL_StopTextInput();
    view->presetDialog = 0;
}

static void submitPresetDialog(Spectrogram *view)
{
    if (view->presetDialog == 1) {
        int index = presetLibrarySave(&view->library, view->presetName, &view->config,
                                      view->presetError, sizeof(view->presetError));
        if (index < 0) return;
        view->preset = index;
    } else {
        if (presetLibraryDelete(&view->library, view->preset, view->presetError, sizeof(view->presetError))) return;
        view->preset = -1;
        view->presetHighlight = 0;
        view->presetScroll = 0;
    }
    closePresetDialog(view);
}

static int presetDialogEvent(Spectrogram *view, const SDL_Event *event)
{
    if (event->type == SDL_TEXTINPUT && view->presetDialog == 1) {
        size_t n = strlen(view->presetName);
        for (const unsigned char *p = (const unsigned char *)event->text.text; *p && n < PRESET_NAME_MAX; ++p)
            if (*p >= 32 && *p < 127) view->presetName[n++] = (char)*p;
        view->presetName[n] = 0; view->presetError[0] = 0;
    } else if (event->type == SDL_KEYDOWN) {
        SDL_Keycode key = event->key.keysym.sym;
        if (key == SDLK_ESCAPE) closePresetDialog(view);
        else if (key == SDLK_RETURN || key == SDLK_KP_ENTER) submitPresetDialog(view);
        else if (view->presetDialog == 1 && key == SDLK_BACKSPACE) {
            size_t n = strlen(view->presetName); if (n) view->presetName[n-1] = 0;
            view->presetError[0] = 0;
        } else if (view->presetDialog == 1 && key == SDLK_a && (event->key.keysym.mod & KMOD_CTRL)) view->presetName[0] = 0;
    } else if (event->type == SDL_MOUSEBUTTONDOWN && event->button.button == SDL_BUTTON_LEFT) {
        if (inside(event->button.x,event->button.y,dialogOK)) submitPresetDialog(view);
        else if (inside(event->button.x,event->button.y,dialogCancel)) closePresetDialog(view);
    }
    return event->type == SDL_KEYUP || event->type == SDL_QUIT || event->type == SDL_WINDOWEVENT ? 0 : 1;
}

#define BROWSER_VISIBLE 10
static const SDL_Rect browserUp = {210,140,125,36};
static const SDL_Rect browserCancel = {1240,140,180,36};
static const SDL_Rect browserList = {210,225,1210,BROWSER_VISIBLE*38};
static void drawFileBrowser(Spectrogram *view)
{
    SDL_Renderer *r=view->renderer;
    SDL_SetRenderDrawBlendMode(r,SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(r,0,0,0,190); SDL_Rect backdrop={0,0,1640,780}; SDL_RenderFillRect(r,&backdrop);
    SDL_SetRenderDrawBlendMode(r,SDL_BLENDMODE_NONE);
    SDL_SetRenderDrawColor(r,27,40,56,255); SDL_Rect panel={180,110,1280,590}; SDL_RenderFillRect(r,&panel);
    button(view,browserUp,"Up",false); button(view,browserCancel,"Cancel",false);
    SDL_SetRenderDrawColor(r,232,239,249,255);
    char label[128]; snprintf(label,sizeof(label),"%.100s",view->browser.directory); text(view,210,188,label);
    for (int i=0;i<BROWSER_VISIBLE && view->browser.scroll+i<view->browser.count;++i) {
        int index=view->browser.scroll+i;
        const BrowserEntry *entry=&view->browser.entries[index];
        snprintf(label,sizeof(label),"%s%.95s",entry->directory ? "[Folder] " : "",entry->name);
        button(view,(SDL_Rect){210,225+i*38,1210,36},label,index==view->browser.selected);
    }
    if (!view->browser.count) text(view,210,260,"No folders or MIDI files here.");
    SDL_SetRenderDrawColor(r,255,175,150,255); text(view,210,627,view->midiError);
    SDL_SetRenderDrawColor(r,210,225,240,255); text(view,210,662,"Click a folder or .mid/.midi file. Scroll to browse.");
}
static void chooseMidiFile(Spectrogram *view,int index)
{
    char path[BROWSER_PATH_MAX];
    int chosen=fileBrowserChoose(&view->browser,index,path,sizeof(path),view->midiError,sizeof(view->midiError));
    if (chosen!=1) return;
    if (midiPlayerLoad(&view->midi,path,view->midiError,sizeof(view->midiError))) return;
    strcpy(view->midiPath,path); view->midiError[0]=0; view->browserOpen=false;
}
static int fileBrowserEvent(Spectrogram *view,const SDL_Event *event)
{
    if (event->type==SDL_KEYDOWN) {
        SDL_Keycode key=event->key.keysym.sym;
        if (key==SDLK_ESCAPE) view->browserOpen=false;
        else if (key==SDLK_BACKSPACE) chooseMidiFile(view,-1);
        else if (key==SDLK_RETURN || key==SDLK_KP_ENTER) chooseMidiFile(view,view->browser.selected);
        else if (key==SDLK_DOWN || key==SDLK_UP) {
            view->browser.selected=(int)fmax(0,fmin(view->browser.count-1,view->browser.selected+(key==SDLK_DOWN ? 1 : -1)));
            if (view->browser.selected<view->browser.scroll) view->browser.scroll=view->browser.selected;
            if (view->browser.selected>=view->browser.scroll+BROWSER_VISIBLE) view->browser.scroll=view->browser.selected-BROWSER_VISIBLE+1;
        }
    } else if (event->type==SDL_MOUSEWHEEL) {
        int direction=event->wheel.y; if (event->wheel.direction==SDL_MOUSEWHEEL_FLIPPED) direction=-direction;
        view->browser.scroll=(int)fmax(0,fmin(fmax(0,view->browser.count-BROWSER_VISIBLE),view->browser.scroll-direction));
    } else if (event->type==SDL_MOUSEBUTTONDOWN && event->button.button==SDL_BUTTON_LEFT) {
        int x=event->button.x,y=event->button.y;
        if (inside(x,y,browserUp)) chooseMidiFile(view,-1);
        else if (inside(x,y,browserCancel)) view->browserOpen=false;
        else if (inside(x,y,browserList)) chooseMidiFile(view,view->browser.scroll+(y-browserList.y)/38);
    }
    return event->type==SDL_KEYUP || event->type==SDL_QUIT || event->type==SDL_WINDOWEVENT ? 0 : 1;
}

static void revealPreset(Spectrogram *view)
{
    if (view->presetHighlight < view->presetScroll) view->presetScroll = view->presetHighlight;
    if (view->presetHighlight >= view->presetScroll + PRESET_VISIBLE)
        view->presetScroll = view->presetHighlight - PRESET_VISIBLE + 1;
}

static int loadPreset(Spectrogram *view, int preset)
{
    if (preset < 0 || preset >= view->library.count) return 0;
    SynthConfig config = view->library.items[preset].config;
    if (synthConfigure(&config) != 0) return -1;
    spectrogramSetConfig(view, &config);
    view->preset = preset;
    view->presetOpen = false;
    return 1;
}

int spectrogramEvent(Spectrogram *view, const SDL_Event *event)
{
    if (view->presetDialog) return presetDialogEvent(view,event);
    if (view->browserOpen) return fileBrowserEvent(view,event);
    if (event->type == SDL_WINDOWEVENT && event->window.event == SDL_WINDOWEVENT_FOCUS_LOST) {
        view->dragRow = -1;
        view->presetOpen = false;
        SDL_CaptureMouse(SDL_FALSE);
        return 0;
    }
    if (event->type == SDL_KEYDOWN && view->equationPage && event->key.keysym.sym == SDLK_ESCAPE) {
        view->equationPage = 0;
        return 1;
    }
    if (event->type == SDL_KEYDOWN && view->presetOpen) {
        switch (event->key.keysym.sym) {
        case SDLK_ESCAPE: view->presetOpen = false; return 1;
        case SDLK_RETURN: case SDLK_KP_ENTER: return loadPreset(view, view->presetHighlight);
        case SDLK_UP: view->presetHighlight = (int)fmax(0, view->presetHighlight - 1); break;
        case SDLK_DOWN: view->presetHighlight = (int)fmin(view->library.count - 1, view->presetHighlight + 1); break;
        case SDLK_HOME: view->presetHighlight = 0; break;
        case SDLK_END: view->presetHighlight = view->library.count - 1; break;
        default: return 0;
        }
        revealPreset(view);
        return 1;
    }
    if (event->type == SDL_MOUSEBUTTONUP && event->button.button == SDL_BUTTON_LEFT) {
        bool dragging = view->dragRow >= 0;
        view->dragRow = -1;
        SDL_CaptureMouse(SDL_FALSE);
        return dragging ? 1 : 0;
    }
    if (event->type == SDL_MOUSEMOTION && view->dragRow >= 0) {
        double delta = (view->dragY - event->motion.y) / 4.0;
        bool fine = (SDL_GetModState() & KMOD_SHIFT) != 0;
        if (envelopeTimeKnob(view->dragRow)) delta *= fine ? 1 : 25;
        else if (fine) delta *= .1;
        view->dragY = event->motion.y;
        if (!integerKnob(view->dragRow)) {
            if (delta == 0) return 1;
            return changeRow(view, view->dragRow, delta) < 0 ? -1 : 1;
        }
        /* Milliseconds and whole percentages accumulate fractional motion. */
        view->dragRemainder += delta;
        int steps = (int)view->dragRemainder;
        if (!steps) return 1;
        view->dragRemainder -= steps;
        return changeRow(view, view->dragRow, steps) < 0 ? -1 : 1;
    }
    int x, y, direction = 0;
    double wheelDelta = 0;
    bool click = event->type == SDL_MOUSEBUTTONDOWN && event->button.button == SDL_BUTTON_LEFT;
    if (click) {
        /* SDL_RenderSetLogicalSize already transforms button and motion events. */
        x = event->button.x; y = event->button.y;
    } else if (event->type == SDL_MOUSEMOTION && view->presetOpen) {
        x = event->motion.x; y = event->motion.y;
    } else if (event->type == SDL_MOUSEWHEEL) {
        /* GetMouseState returns window coordinates, unlike button events. */
        int wx, wy;
        float lx, ly;
        SDL_GetMouseState(&wx, &wy);
        SDL_RenderWindowToLogical(view->renderer, wx, wy, &lx, &ly);
        x = (int)lx; y = (int)ly;
        wheelDelta = event->wheel.preciseY != 0 ? event->wheel.preciseY : event->wheel.y;
        if (event->wheel.direction == SDL_MOUSEWHEEL_FLIPPED) wheelDelta = -wheelDelta;
        if (!wheelDelta) return 0;
        direction = wheelDelta > 0 ? 1 : -1;
    } else return 0;
    if (view->equationPage) {
        if (click && inside(x, y, soundPageButton)) { view->equationPage = 0; return 1; }
        if (click && inside(x, y, (SDL_Rect){340,20,225,42})) { view->equationPage = 1; view->equationScroll = 0; return 1; }
        if (click && inside(x, y, (SDL_Rect){600,20,225,42})) { view->equationPage = 2; view->equationScroll = 0; return 1; }
        if (click && inside(x, y, (SDL_Rect){860,20,225,42})) { view->equationPage = 3; return 1; }
        if (click && inside(x,y,(SDL_Rect){1120,20,225,42})) { view->equationPage=4; return 1; }
        if (view->equationPage == 4) {
            if (click && inside(x,y,midiBrowseButton)) {
                const char *dir=view->browser.directory[0] ? view->browser.directory : NULL;
                if (fileBrowserOpen(&view->browser,dir,view->midiError,sizeof(view->midiError))==0) {
                    view->midiError[0]=0; view->browserOpen=true;
                }
            } else if (click && inside(x,y,midiPlayButton)) midiPlayerPlay(&view->midi,midiNow());
            else if (click && inside(x,y,midiStopButton)) midiPlayerStop(&view->midi,midiNow());
            else if (click && inside(x,y,midiRestartButton)) midiPlayerRestart(&view->midi,midiNow());
            return 1;
        }
        if (view->equationPage != 3) {
            if (direction) view->equationScroll = (int)fmax(0, view->equationScroll - direction * 3);
            return 1;
        }
    }
    if (!view->equationPage && click &&
        (inside(x,y,previousPresetButton) || inside(x,y,nextPresetButton))) {
        int step = inside(x,y,nextPresetButton) ? 1 : -1;
        int preset = view->preset < 0 ? (step > 0 ? 0 : view->library.count - 1) :
                     (view->preset + step + view->library.count) % view->library.count;
        return loadPreset(view, preset);
    }
    if (!view->equationPage && !view->presetOpen && click && inside(x,y,midiPageButton)) {
        view->equationPage=4; return 1;
    }
    if (!view->equationPage && !view->presetOpen && click &&
        (inside(x,y,savePresetButton) || inside(x,y,deletePresetButton))) {
        bool save = inside(x,y,savePresetButton);
        if (!save && (view->preset < 0 || view->library.items[view->preset].builtin)) return 1;
        view->presetDialog = save ? 1 : 2;
        view->presetName[0] = 0; view->presetError[0] = 0;
        if (save) {
            for (int note = 0; note < 128; ++note) deregisterNote(note);
            SDL_StartTextInput();
        }
        return 1;
    }
    if (!view->equationPage && !view->presetOpen && click &&
        inside(x, y, effectsPageButton)) {
        view->equationPage = 3;
        view->equationScroll = 0;
        return 1;
    }
    SDL_Rect list = {80, PRESET_LIST_Y, 450, PRESET_VISIBLE * PRESET_ITEM_H};
    if (view->presetOpen) {
        if (direction) {
            view->presetScroll = (int)adjust(view->presetScroll, 1, -direction, 0, fmax(0, view->library.count - PRESET_VISIBLE));
            return 1;
        }
        if (inside(x, y, list)) {
            view->presetHighlight = view->presetScroll + (y - PRESET_LIST_Y) / PRESET_ITEM_H;
            if (click) return loadPreset(view, view->presetHighlight);
            return 1;
        }
        if (click) { view->presetOpen = false; return 1; }
        return 0;
    }
    if (!view->equationPage && click && inside(x, y, presetBox)) {
        view->presetOpen = true;
        view->presetHighlight = view->preset >= 0 ? view->preset : 0;
        revealPreset(view);
        return 1;
    }
    for (int i = 0; !view->equationPage && i < 6; ++i) {
        SDL_Rect rect = selectorRect(i);
        if (!inside(x, y, rect)) continue;
        if (!direction) {
            if (y < rect.y + 24) return 0;
            if (x < rect.x + 36) direction = -1;
            else if (x >= rect.x + 244) direction = 1;
            else return 0;
        }
        return changeRow(view, selectorRows[i], direction) < 0 ? -1 : 1;
    }
    for (int i = view->equationPage == 3 ? SOUND_KNOBS : 0;
         i < (view->equationPage == 3 ? KNOB_COUNT : SOUND_KNOBS); ++i) {
        if (!inside(x, y, knobRect(i))) continue;
        if (direction) {
            double amount = integerKnob(knobRows[i]) ? direction : wheelDelta;
            bool fine = (SDL_GetModState() & KMOD_SHIFT) != 0;
            if (envelopeTimeKnob(knobRows[i])) amount *= fine ? 1 : 25;
            else if (!integerKnob(knobRows[i]) && fine) amount *= .1;
            return changeRow(view, knobRows[i], amount) < 0 ? -1 : 1;
        }
        if (click) {
            view->dragRow = knobRows[i];
            view->dragY = y;
            view->dragRemainder = 0;
            SDL_CaptureMouse(SDL_TRUE);
            return 1;
        }
    }
    return 0;
}
