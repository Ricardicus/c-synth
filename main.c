#include "synth.h"
#include "options.h"
#include "spectrogram.h"

#include <SDL.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

static int noteForKey(SDL_Keycode key)
{
    switch (key) {
    case SDLK_z: return 48; /* C3 */
    case SDLK_x: return 50; /* D3 */
    case SDLK_c: return 52; /* E3 */
    case SDLK_v: return 53; /* F3 */
    case SDLK_b: return 55; /* G3 */
    case SDLK_n: return 57; /* A3 */
    case SDLK_m: return 59; /* B3 */
    case SDLK_a: return 60; /* C4 */
    case SDLK_s: return 62; /* D4 */
    case SDLK_d: return 64; /* E4 */
    case SDLK_f: return 65; /* F4 */
    case SDLK_g: return 67; /* G4 */
    case SDLK_h: return 69; /* A4 */
    case SDLK_j: return 71; /* B4 */
    case SDLK_k: return 72; /* C5 */
    case SDLK_l: return 74; /* D5 */
    case SDLK_q: return 72; /* C5 */
    case SDLK_w: return 74; /* D5 */
    case SDLK_e: return 76; /* E5 */
    case SDLK_r: return 77; /* F5 */
    case SDLK_t: return 79; /* G5 */
    case SDLK_y: return 81; /* A5 */
    case SDLK_u: return 83; /* B5 */
    case SDLK_i: return 84; /* C6 */
    case SDLK_o: return 86; /* D6 */
    case SDLK_p: return 88; /* E6 */
    default: return -1;
    }
}

int main(int argc, char *argv[])
{
    SynthConfig config;
    char error[256];
    int parsed = parseOptions(argc, argv, &config, error, sizeof(error));
    if (parsed != 0) {
        if (parsed < 0)
            fprintf(stderr, "%s\n", error);
        printUsage(argv[0]);
        return parsed < 0 ? 1 : 0;
    }
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) != 0) {
        fprintf(stderr, "SDL initialization failed: %s\n", SDL_GetError());
        return 1;
    }

    SDL_Window *window = SDL_CreateWindow(
        "FM synth | Z-M: C3-B3 | A-L: C4-D5 | Q-P: C5-E6 | Esc quits",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1640, 780,
        SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    if (window != NULL)
        SDL_SetWindowMinimumSize(window, 1230, 585);
    if (window == NULL || synthInitWithLayers(&config) != 0) {
        fprintf(stderr, "Synth startup failed: %s\n", SDL_GetError());
        if (window != NULL)
            SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    Spectrogram *view = spectrogramCreate(window, config.layerCount);
    if (view == NULL) {
        fprintf(stderr, "Spectrogram startup failed: %s\n", SDL_GetError());
        synthShutdown();
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    spectrogramSetConfig(view, &config);
    puts("Click presets and use the live controls to edit sounds while playing.");
    puts("Focus the window. Play Z-M (C3-B3), A-L (C4-D5), Q-P (C5-E6).");
    puts("Hold several keys for chords. Release to stop; Escape quits.");
    for (int layer = 0; layer < config.layerCount; ++layer) {
        const SynthLayerConfig *settings = &config.layers[layer];
        printf("Layer %d: gain=%g detune=%g cents: ", layer + 1, settings->gain, settings->detuneCents);
        for (int i = 0; i < settings->fm.operatorCount; ++i) {
            const FmOperatorConfig *op = &settings->fm.operators[i];
            printf("%sOP%d (%s ratio=%g", i == 0 ? "" : " -> ", i + 1,
                   oscillatorWaveformName(op->waveform), op->ratio);
            if (i != settings->fm.operatorCount - 1)
                printf(" rm=%g index=%s", op->rm,
                       op->indexMode == FM_INDEX_ADSR ? "adsr" :
                       op->indexMode == FM_INDEX_DECAY ? "decay" : "sustain");
            if (op->vibratoDepthCents != 0)
                printf(" vibrato=%gHz/%gc", op->vibratoRateHz, op->vibratoDepthCents);
            putchar(')');
        }
        putchar('\n');
    }

    int result = 0;
    bool running = true;
    int heldKeyNotes[SDL_NUM_SCANCODES] = {0}; /* MIDI note + 1, or 0. */
    int noteHolds[128] = {0};
    while (running) {
        SDL_Event event;
        /* SDL's event backend may leave a warning in SDL_GetError even when
         * pumping events succeeds. An empty queue is normal, not fatal. */
        if (SDL_PollEvent(&event)) {
            do {
                int handled = spectrogramEvent(view, &event);
                if (handled < 0) {
                    fprintf(stderr, "Live configuration failed: %s\n", SDL_GetError());
                    result = 1;
                    running = false;
                }
                if (handled > 0) continue;
                switch (event.type) {
                case SDL_QUIT:
                    running = false;
                    break;
                case SDL_KEYDOWN: {
                    if (event.key.keysym.sym == SDLK_ESCAPE)
                        running = false;
                    else if (!event.key.repeat) {
                        int note = noteForKey(event.key.keysym.sym);
                        SDL_Scancode key = event.key.keysym.scancode;
                        if (note >= 0 && heldKeyNotes[key] == 0) {
                            spectrogramNote(view, note);
                            heldKeyNotes[key] = note + 1;
                            if (noteHolds[note]++ == 0)
                                registerNote(note);
                        }
                    }
                    break;
                }
                case SDL_KEYUP: {
                    SDL_Scancode key = event.key.keysym.scancode;
                    int note = heldKeyNotes[key] - 1;
                    if (note >= 0) {
                        heldKeyNotes[key] = 0;
                        if (--noteHolds[note] == 0)
                            deregisterNote(note);
                    }
                    break;
                }
                case SDL_WINDOWEVENT:
                    if (event.window.event == SDL_WINDOWEVENT_FOCUS_LOST) {
                        for (int note = 0; note < 128; ++note)
                            deregisterNote(note);
                        memset(heldKeyNotes, 0, sizeof(heldKeyNotes));
                        memset(noteHolds, 0, sizeof(noteHolds));
                    }
                    break;
                default:
                    break;
                }
            } while (running && SDL_PollEvent(&event));
        }
        if (running && spectrogramDraw(view) != 0) {
            fprintf(stderr, "Spectrogram rendering failed: %s\n", SDL_GetError());
            result = 1;
            break;
        }
        if (running)
            SDL_Delay(16);
    }

    spectrogramDestroy(view);
    synthShutdown();
    SDL_DestroyWindow(window);
    SDL_Quit();
    return result;
}
