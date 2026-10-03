#include "spectrogram.h"
#include "spectrum.h"
#include "synth.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PLOT_X 80
#define PLOT_Y 75
#define PLOT_W 900
#define PLOT_H 420
#define MIN_HZ 40.0

_Static_assert(SPECTRUM_SIZE == SYNTH_ANALYSIS_SAMPLES, "Snapshot must fit FFT");

struct Spectrogram {
    SDL_Window *window;
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

/* Tiny built-in uppercase font, keeping SDL_ttf out of the scaffold. */
static void text(SDL_Renderer *renderer, int x, int y, const char *value)
{
    static const Uint8 glyphs[36][5] = {
        {0x3e,0x51,0x49,0x45,0x3e},{0x00,0x42,0x7f,0x40,0x00},
        {0x42,0x61,0x51,0x49,0x46},{0x21,0x41,0x45,0x4b,0x31},
        {0x18,0x14,0x12,0x7f,0x10},{0x27,0x45,0x45,0x45,0x39},
        {0x3c,0x4a,0x49,0x49,0x30},{0x01,0x71,0x09,0x05,0x03},
        {0x36,0x49,0x49,0x49,0x36},{0x06,0x49,0x49,0x29,0x1e},
        {0x7e,0x11,0x11,0x11,0x7e},{0x7f,0x49,0x49,0x49,0x36},
        {0x3e,0x41,0x41,0x41,0x22},{0x7f,0x41,0x41,0x22,0x1c},
        {0x7f,0x49,0x49,0x49,0x41},{0x7f,0x09,0x09,0x09,0x01},
        {0x3e,0x41,0x49,0x49,0x7a},{0x7f,0x08,0x08,0x08,0x7f},
        {0x00,0x41,0x7f,0x41,0x00},{0x20,0x40,0x41,0x3f,0x01},
        {0x7f,0x08,0x14,0x22,0x41},{0x7f,0x40,0x40,0x40,0x40},
        {0x7f,0x02,0x0c,0x02,0x7f},{0x7f,0x04,0x08,0x10,0x7f},
        {0x3e,0x41,0x41,0x41,0x3e},{0x7f,0x09,0x09,0x09,0x06},
        {0x3e,0x41,0x51,0x21,0x5e},{0x7f,0x09,0x19,0x29,0x46},
        {0x46,0x49,0x49,0x49,0x31},{0x01,0x01,0x7f,0x01,0x01},
        {0x3f,0x40,0x40,0x40,0x3f},{0x1f,0x20,0x40,0x20,0x1f},
        {0x3f,0x40,0x38,0x40,0x3f},{0x63,0x14,0x08,0x14,0x63},
        {0x07,0x08,0x70,0x08,0x07},{0x61,0x51,0x49,0x45,0x43}
    };
    for (; *value; ++value, x += 12) {
        int ch = toupper((unsigned char)*value);
        int index = ch >= '0' && ch <= '9' ? ch - '0' :
                    ch >= 'A' && ch <= 'Z' ? ch - 'A' + 10 : -1;
        for (int col = 0; col < 5; ++col) {
            Uint8 bits = index >= 0 ? glyphs[index][col] :
                         ch == '.' ? (Uint8)(col == 2 ? 0x60 : 0) : ch == '+' ? (Uint8)(col == 2 ? 0x3e : 0x08) : ch == '-' ? 0x08 : ch == '>' ?
                         (Uint8)(col == 2 ? 0x08 : col == 1 ? 0x14 : col == 0 ? 0x22 : 0) : 0;
            for (int row = 0; row < 7; ++row) {
                if (bits & (1 << row)) {
                    SDL_Rect pixel = {x + col * 2, y + row * 2, 2, 2};
                    SDL_RenderFillRect(renderer, &pixel);
                }
            }
        }
    }
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
    view->window = window;
    view->config = synthDefaultConfig();
    view->preset = -1;
    view->dragRow = -1;
    view->layerCount = layerCount;
    view->renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (view->renderer == NULL)
        view->renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
    if (view->renderer == NULL || SDL_RenderSetLogicalSize(view->renderer, 1640, 780) != 0)
        goto failure;
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

int spectrogramDraw(Spectrogram *view)
{
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
    text(renderer, PLOT_X, 20, "LIVE OUTPUT SPECTROGRAM");
    char status[80];
    snprintf(status, sizeof(status), "%d LAYERS   %d HZ   FREQUENCY UP", view->layerCount, sampleRate);
    text(renderer, PLOT_X, 45, status);
    const int ticks[] = {40,100,200,500,1000,2000,5000,10000,20000};
    for (unsigned int i = 0; i < sizeof(ticks) / sizeof(ticks[0]); ++i) {
        if (ticks[i] > maxHz)
            continue;
        int y = PLOT_Y + (int)((1.0 - log(ticks[i] / MIN_HZ) / log(maxHz / MIN_HZ)) * (PLOT_H - 1));
        char label[16];
        snprintf(label, sizeof(label), "%d", ticks[i]);
        SDL_SetRenderDrawColor(renderer, 145, 159, 182, 255);
        text(renderer, 8, y - 7, label);
        SDL_RenderDrawLine(renderer, PLOT_X - 5, y, PLOT_X - 1, y);
    }
    SDL_SetRenderDrawColor(renderer, 190, 204, 222, 255);
    text(renderer, PLOT_X, 508, "TIME   OLDER > NEWER");
    for (int i = 0; i < 220; ++i) {
        Uint32 rgb = color(-80.0 + 80.0 * i / 219);
        SDL_SetRenderDrawColor(renderer, (Uint8)(rgb >> 16), (Uint8)(rgb >> 8), (Uint8)rgb, 255);
        SDL_RenderDrawLine(renderer, 700 + i, 510, 700 + i, 524);
    }
    SDL_SetRenderDrawColor(renderer, 190, 204, 222, 255);
    text(renderer, 620, 510, "-80 DB");
    text(renderer, 932, 510, "0 DB");
    text(renderer, PLOT_X, 550, "LOW Z-M   MID A-L   HIGH Q-P   ESC QUITS");
    drawControls(view);
    SDL_RenderPresent(renderer);
    return 0;
}

void spectrogramDestroy(Spectrogram *view)
{
    if (view == NULL)
        return;
    if (view->texture != NULL)
        SDL_DestroyTexture(view->texture);
    if (view->renderer != NULL)
        SDL_DestroyRenderer(view->renderer);
    free(view);
}

/* Controls share their geometry between drawing and hit testing. */
#define PANEL_X 1020
#define CONTROL_ROWS 22
#define KNOB_COUNT 16
#define PRESET_VISIBLE 8
#define PRESET_ITEM_H 30
#define PRESET_LIST_Y 365

static const SDL_Rect presetBox = {80, 605, 450, 38};
static const int knobRows[KNOB_COUNT] = {2, 3, 7, 8, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21};
static const int selectorRows[6] = {0, 1, 4, 5, 6, 9};

static const char *rowNames[CONTROL_ROWS] = {
    "Layers", "Selected layer", "Layer gain", "Detune cents",
    "Operators", "Selected operator", "Waveform", "Ratio", "FM depth",
    "Index envelope", "Index decay rate", "Attack ms", "Decay ms",
    "Sustain percent", "Release ms", "Pulse width", "Vibrato Hz",
    "Vibrato cents", "Attack ms", "Decay ms", "Sustain percent", "Release ms"
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
    default: value[0] = 0; return;
    }
    snprintf(value, size, "%.2f", number);
}

static double adjust(double value, double step, double direction, double low, double high)
{
    return fmax(low, fmin(high, value + step * direction));
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
    case 3: layer->detuneCents = adjust(layer->detuneCents, 5, direction, -4800, 4800); break;
    case 4: layer->fm.operatorCount = (int)adjust(layer->fm.operatorCount, 1, direction, 1, FM_MAX_OPERATORS); break;
    case 5: view->selectedOperator = (int)adjust(view->selectedOperator, 1, direction, 0, layer->fm.operatorCount - 1); break;
    case 6: op->waveform = (Waveform)((op->waveform + (int)direction + 5) % 5); break;
    case 7: op->ratio = adjust(op->ratio, .25, direction, .25, 32); break;
    case 8: op->rm = adjust(op->rm, .25, direction, 0, 32); break;
    case 9: op->indexMode = (FmIndexMode)((op->indexMode + (int)direction + 3) % 3); break;
    case 10: op->decayRate = adjust(op->decayRate, .25, direction, 0, 100); break;
    case 11: op->attackMs = (int)adjust(op->attackMs, 10, direction, 0, 10000); break;
    case 12: op->decayMs = (int)adjust(op->decayMs, 25, direction, 0, 10000); break;
    case 13: op->sustainPercent = (int)adjust(op->sustainPercent, 5, direction, 0, 100); break;
    case 14: op->releaseMs = (int)adjust(op->releaseMs, 25, direction, 0, 10000); break;
    case 15: op->pulseWidth = adjust(op->pulseWidth, .01, direction, .05, .95); break;
    case 16: op->vibratoRateHz = adjust(op->vibratoRateHz, .25, direction, 0, 50); break;
    case 17: op->vibratoDepthCents = adjust(op->vibratoDepthCents, 5, direction, 0, 1200); break;
    case 18: view->config.outputEnvelope.attackMs = (int)adjust(view->config.outputEnvelope.attackMs, 10, direction, 0, 10000); break;
    case 19: view->config.outputEnvelope.decayMs = (int)adjust(view->config.outputEnvelope.decayMs, 25, direction, 0, 10000); break;
    case 20: view->config.outputEnvelope.sustainPercent = (int)adjust(view->config.outputEnvelope.sustainPercent, 1, direction, 0, 100); break;
    case 21: view->config.outputEnvelope.releaseMs = (int)adjust(view->config.outputEnvelope.releaseMs, 25, direction, 0, 10000); break;
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

static void button(SDL_Renderer *r, SDL_Rect rect, const char *label, bool active)
{
    SDL_SetRenderDrawColor(r, active ? 39 : 30, active ? 104 : 42, active ? 126 : 60, 255);
    SDL_RenderFillRect(r, &rect);
    SDL_SetRenderDrawColor(r, 220, 232, 245, 255);
    text(r, rect.x + 8, rect.y + 6, label);
}

static SDL_Rect knobRect(int index)
{
    if (index >= 12) return (SDL_Rect){80 + (index - 12) * 225, 670, 210, 104};
    return (SDL_Rect){PANEL_X + (index % 3) * 195, 260 + (index / 3) * 112, 185, 104};
}

static SDL_Rect selectorRect(int index)
{
    return (SDL_Rect){PANEL_X + (index % 2) * 300, 92 + (index / 2) * 52, 280, 44};
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
    static const double lows[KNOB_COUNT] = {0, -4800, .25, 0, 0, 0, 0, 0, 0, .05, 0, 0, 0, 0, 0, 0};
    static const double highs[KNOB_COUNT] = {1, 4800, 32, 32, 100, 10000, 10000, 100, 10000, .95, 50, 1200, 10000, 10000, 100, 10000};
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
    text(r, rect.x + (rect.w - (int)strlen(rowNames[row]) * 12) / 2, rect.y, rowNames[row]);
    SDL_SetRenderDrawColor(r, 44, 61, 80, 255);
    circle(r, cx, cy, 29);
    SDL_SetRenderDrawColor(r, view->dragRow == row ? 57 : 29, view->dragRow == row ? 160 : 99, 126, 255);
    circle(r, cx, cy, 24);
    double angle = (135 + position * 270) * 3.141592653589793 / 180;
    SDL_SetRenderDrawColor(r, 231, 241, 252, 255);
    SDL_RenderDrawLine(r, cx, cy, cx + (int)(cos(angle) * 21), cy + (int)(sin(angle) * 21));
    text(r, rect.x + (rect.w - (int)strlen(value) * 12) / 2, rect.y + 88, value);
}

static void drawControls(Spectrogram *view)
{
    SDL_Renderer *r = view->renderer;
    SDL_SetRenderDrawColor(r, 220, 232, 245, 255);
    text(r, PANEL_X, 20, "LIVE SOUND CONTROLS");
    text(r, PANEL_X, 48, synthPresetName(view->preset));
    for (int i = 0; i < 6; ++i) {
        SDL_Rect rect = selectorRect(i);
        char value[32];
        rowValue(view, selectorRows[i], value, sizeof(value));
        SDL_SetRenderDrawColor(r, 180, 199, 219, 255);
        text(r, rect.x, rect.y, rowNames[selectorRows[i]]);
        button(r, (SDL_Rect){rect.x, rect.y + 18, 36, 25}, "-", false);
        button(r, (SDL_Rect){rect.x + 244, rect.y + 18, 36, 25}, "+", false);
        text(r, rect.x + 48, rect.y + 24, value);
    }
    for (int i = 0; i < KNOB_COUNT; ++i) drawKnob(view, i);
    SDL_SetRenderDrawColor(r, 145, 165, 186, 255);
    text(r, PANEL_X, 722, "DRAG UP DOWN OR SCROLL A KNOB");
    text(r, PANEL_X, 748, "SHIFT DRAG FOR FINE CONTROL");
    text(r, 80, 578, "PRESET");
    button(r, presetBox, synthPresetName(view->preset), view->presetOpen);
    /* Dropdown indicator. */
    for (int i = 0; i < 7; ++i)
        SDL_RenderDrawLine(r, 500 + i, 620 + i, 514 - i, 620 + i);
    text(r, 80, 650, "MASTER OUTPUT ADSR");
    if (view->presetOpen) {
        for (int i = 0; i < PRESET_VISIBLE; ++i) {
            int preset = view->presetScroll + i;
            button(r, (SDL_Rect){80, PRESET_LIST_Y + i * PRESET_ITEM_H, 450, PRESET_ITEM_H},
                   synthPresetName(preset), preset == view->presetHighlight);
        }
        SDL_Rect track = {518, PRESET_LIST_Y, 10, PRESET_VISIBLE * PRESET_ITEM_H};
        SDL_SetRenderDrawColor(r, 59, 78, 97, 255);
        SDL_RenderFillRect(r, &track);
        SDL_Rect thumb = {518, PRESET_LIST_Y + view->presetScroll * track.h / SYNTH_PRESET_COUNT,
                          10, track.h * PRESET_VISIBLE / SYNTH_PRESET_COUNT};
        SDL_SetRenderDrawColor(r, 94, 182, 202, 255);
        SDL_RenderFillRect(r, &thumb);
    }
}

static void revealPreset(Spectrogram *view)
{
    if (view->presetHighlight < view->presetScroll) view->presetScroll = view->presetHighlight;
    if (view->presetHighlight >= view->presetScroll + PRESET_VISIBLE)
        view->presetScroll = view->presetHighlight - PRESET_VISIBLE + 1;
}

static int loadPreset(Spectrogram *view, int preset)
{
    SynthConfig config = synthPresetConfig(preset);
    if (synthConfigure(&config) != 0) return -1;
    spectrogramSetConfig(view, &config);
    view->preset = preset;
    view->presetOpen = false;
    return 1;
}

int spectrogramEvent(Spectrogram *view, const SDL_Event *event)
{
    if (event->type == SDL_WINDOWEVENT && event->window.event == SDL_WINDOWEVENT_FOCUS_LOST) {
        view->dragRow = -1;
        view->presetOpen = false;
        SDL_CaptureMouse(SDL_FALSE);
        return 0;
    }
    if (event->type == SDL_KEYDOWN && view->presetOpen) {
        switch (event->key.keysym.sym) {
        case SDLK_ESCAPE: view->presetOpen = false; return 1;
        case SDLK_RETURN: case SDLK_KP_ENTER: return loadPreset(view, view->presetHighlight);
        case SDLK_UP: view->presetHighlight = (int)fmax(0, view->presetHighlight - 1); break;
        case SDLK_DOWN: view->presetHighlight = (int)fmin(SYNTH_PRESET_COUNT - 1, view->presetHighlight + 1); break;
        case SDLK_HOME: view->presetHighlight = 0; break;
        case SDLK_END: view->presetHighlight = SYNTH_PRESET_COUNT - 1; break;
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
        if (SDL_GetModState() & KMOD_SHIFT) delta *= .1;
        view->dragY = event->motion.y;
        /* Integer-valued envelopes accumulate fractional movement. */
        view->dragRemainder += delta;
        int steps = (int)view->dragRemainder;
        if (!steps) return 1;
        view->dragRemainder -= steps;
        return changeRow(view, view->dragRow, steps) < 0 ? -1 : 1;
    }
    int x, y, direction = 0;
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
        direction = event->wheel.y;
        if (event->wheel.direction == SDL_MOUSEWHEEL_FLIPPED) direction = -direction;
        if (!direction) return 0;
        direction = direction > 0 ? 1 : -1;
    } else return 0;
    SDL_Rect list = {80, PRESET_LIST_Y, 450, PRESET_VISIBLE * PRESET_ITEM_H};
    if (view->presetOpen) {
        if (direction) {
            view->presetScroll = (int)adjust(view->presetScroll, 1, -direction, 0, SYNTH_PRESET_COUNT - PRESET_VISIBLE);
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
    if (click && inside(x, y, presetBox)) {
        view->presetOpen = true;
        view->presetHighlight = view->preset >= 0 ? view->preset : 0;
        revealPreset(view);
        return 1;
    }
    for (int i = 0; i < 6; ++i) {
        SDL_Rect rect = selectorRect(i);
        if (!inside(x, y, rect)) continue;
        if (!direction) {
            if (y < rect.y + 18) return 0;
            if (x < rect.x + 36) direction = -1;
            else if (x >= rect.x + 244) direction = 1;
            else return 0;
        }
        return changeRow(view, selectorRows[i], direction) < 0 ? -1 : 1;
    }
    for (int i = 0; i < KNOB_COUNT; ++i) {
        if (!inside(x, y, knobRect(i))) continue;
        if (direction) return changeRow(view, knobRows[i], direction) < 0 ? -1 : 1;
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
