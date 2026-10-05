#include "synth_sdl.h"
#include <SDL.h>

static SDL_AudioDeviceID device;
static Synth *engine;

static void audioCallback(void *userdata, Uint8 *stream, int length)
{
    (void)userdata;
    synthRender(engine, (float *)stream, (size_t)length / sizeof(float));
}

int sdlSynthInit(void)
{
    return sdlSynthInitWithLayers(NULL);
}

int sdlSynthInitWithConfig(const FmConfig *config)
{
    if (!fmConfigValid(config)) return SDL_SetError("Invalid FM configuration");
    SynthConfig sound = synthDefaultConfig();
    sound.layers[0].fm = *config;
    return sdlSynthInitWithLayers(&sound);
}

int sdlSynthInitWithLayers(const SynthConfig *config)
{
    if (device) return 0;
    if (config && !synthConfigValid(config)) return SDL_SetError("Invalid synth configuration");
    SDL_AudioSpec wanted = {0}, obtained;
    wanted.freq = 48000;
    wanted.format = AUDIO_F32SYS;
    wanted.channels = 1;
    wanted.samples = 256;
    wanted.callback = audioCallback;
    device = SDL_OpenAudioDevice(NULL, 0, &wanted, &obtained, SDL_AUDIO_ALLOW_FREQUENCY_CHANGE);
    if (!device) return -1;
    engine = synthCreate(obtained.freq, config);
    if (!engine) {
        SDL_CloseAudioDevice(device); device = 0;
        return SDL_SetError("Cannot create synth engine");
    }
    SDL_PauseAudioDevice(device, 0);
    return 0;
}

void sdlSynthShutdown(void)
{
    if (!device) return;
    SDL_CloseAudioDevice(device); device = 0;
    synthDestroy(engine); engine = NULL;
}

int sdlSynthConfigure(const SynthConfig *config)
{
    if (!device) return SDL_SetError("Audio is not running");
    SDL_LockAudioDevice(device);
    int result = synthConfigure(engine, config);
    SDL_UnlockAudioDevice(device);
    return result == 0 ? 0 : SDL_SetError("Invalid synth configuration");
}

int sdlSynthAudioSnapshot(float *samples, uint64_t *position)
{
    if (!device) return 0;
    SDL_LockAudioDevice(device);
    int rate = synthAudioSnapshot(engine, samples, position);
    SDL_UnlockAudioDevice(device);
    return rate;
}

static void note(int pitch, bool on, bool midi, int velocity)
{
    if (!device) return;
    SDL_LockAudioDevice(device);
    if (midi) {
        if (on) synthMidiNoteOn(engine, pitch, velocity);
        else synthMidiNoteOff(engine, pitch);
    } else {
        if (on) synthNoteOn(engine, pitch);
        else synthNoteOff(engine, pitch);
    }
    SDL_UnlockAudioDevice(device);
}
void sdlSynthRegisterNote(int pitch) { note(pitch, true, false, 127); }
void sdlSynthDeregisterNote(int pitch) { note(pitch, false, false, 127); }
void sdlSynthRegisterMidiNote(int pitch) { note(pitch, true, true, 127); }
void sdlSynthRegisterMidiNoteWithVelocity(int pitch, int velocity) { note(pitch, true, true, velocity); }
void sdlSynthDeregisterMidiNote(int pitch) { note(pitch, false, true, 127); }
