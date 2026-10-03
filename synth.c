#include "synth.h"
#include "envelope.h"

#include <SDL.h>
#include <math.h>
#include <stdbool.h>
#include <string.h>

#define NOTE_COUNT 128

typedef struct {
    FmSynth generator;
    double pitchMultiplier;
    double mixGain;
} LayerVoice;

typedef struct {
    OutputEnvelope envelope;
    double frequency;
    bool held;
    bool active;
    LayerVoice layers[SYNTH_MAX_LAYERS];
} Voice;

static Voice voices[NOTE_COUNT];
static SDL_AudioDeviceID device;
static int sampleRate;
static int layerCount;
static float analysisSamples[SYNTH_ANALYSIS_SAMPLES];
static int analysisCursor;
static uint64_t samplePosition;

static void audioCallback(void *userdata, Uint8 *stream, int length)
{
    (void)userdata;
    float *samples = (float *)stream;
    const int count = length / (int)sizeof(*samples);

    for (int i = 0; i < count; ++i) {
        double sample = 0.0;
        for (int note = 0; note < NOTE_COUNT; ++note) {
            Voice *voice = &voices[note];
            if (!voice->active)
                continue;
            double amplitude = outputEnvelopeNext(&voice->envelope, sampleRate);
            for (int layer = 0; layer < layerCount; ++layer) {
                LayerVoice *part = &voice->layers[layer];
                sample += 0.1 * amplitude * part->mixGain *
                          fmNextSample(&part->generator, voice->frequency * part->pitchMultiplier);
            }
            voice->active = voice->held || voice->envelope.stage != FM_ENV_IDLE;
        }
        samples[i] = (float)fmax(-1.0, fmin(1.0, sample));
        /* Capture the actual mixed output; analysis stays on the main thread. */
        analysisSamples[analysisCursor] = samples[i];
        analysisCursor = (analysisCursor + 1) % SYNTH_ANALYSIS_SAMPLES;
    }
    samplePosition += (uint64_t)count;
}

int synthInit(void)
{
    SynthConfig config = synthDefaultConfig();
    return synthInitWithLayers(&config);
}

int synthInitWithConfig(const FmConfig *config)
{
    if (!fmConfigValid(config))
        return SDL_SetError("Invalid FM configuration");
    SynthConfig settings = synthDefaultConfig();
    settings.layers[0].fm = *config;
    return synthInitWithLayers(&settings);
}

int synthInitWithLayers(const SynthConfig *config)
{
    if (device != 0)
        return 0;
    if (!synthConfigValid(config))
        return SDL_SetError("Invalid synth configuration");

    layerCount = config->layerCount;
    memset(analysisSamples, 0, sizeof(analysisSamples));
    analysisCursor = 0;
    samplePosition = 0;
    for (int note = 0; note < NOTE_COUNT; ++note)
        voices[note] = (Voice){.frequency = 440.0 * pow(2.0, (note - 69) / 12.0)};

    SDL_AudioSpec wanted = {0};
    SDL_AudioSpec obtained;
    wanted.freq = 48000;
    wanted.format = AUDIO_F32SYS;
    wanted.channels = 1;
    wanted.samples = 256;
    wanted.callback = audioCallback;
    device = SDL_OpenAudioDevice(NULL, 0, &wanted, &obtained,
                                SDL_AUDIO_ALLOW_FREQUENCY_CHANGE);
    if (device == 0)
        return -1;

    sampleRate = obtained.freq;
    for (int note = 0; note < NOTE_COUNT; ++note) {
        outputEnvelopeConfigure(&voices[note].envelope, config->outputEnvelope);
        for (int layer = 0; layer < layerCount; ++layer) {
            LayerVoice *part = &voices[note].layers[layer];
            const SynthLayerConfig *settings = &config->layers[layer];
            fmInit(&part->generator, sampleRate, &settings->fm);
            part->pitchMultiplier = exp2(settings->detuneCents / 1200.0);
            part->mixGain = settings->gain / layerCount;
        }
    }
    SDL_PauseAudioDevice(device, 0);
    return 0;
}

int synthConfigure(const SynthConfig *config)
{
    if (!synthConfigValid(config))
        return SDL_SetError("Invalid synth configuration");
    if (device == 0)
        return SDL_SetError("Audio is not running");
    SDL_LockAudioDevice(device);
    int previousLayers = layerCount;
    layerCount = config->layerCount;
    for (int note = 0; note < NOTE_COUNT; ++note) {
        Voice *voice = &voices[note];
        outputEnvelopeConfigure(&voice->envelope, config->outputEnvelope);
        for (int layer = 0; layer < layerCount; ++layer) {
            LayerVoice *part = &voice->layers[layer];
            FmSynth previous = part->generator;
            bool existing = layer < previousLayers;
            const SynthLayerConfig *settings = &config->layers[layer];
            fmInit(&part->generator, sampleRate, &settings->fm);
            if (voice->active) {
                fmNoteOn(&part->generator, true);
                if (!voice->held)
                    fmNoteOff(&part->generator);
            }
            if (existing) {
                for (int i = 0; i < previous.operatorCount && i < part->generator.operatorCount; ++i) {
                    FmOperator *op = &part->generator.operators[i];
                    const FmOperator *old = &previous.operators[i];
                    op->oscillator.phase = old->oscillator.phase;
                    op->oscillator.vibratoPhase = old->oscillator.vibratoPhase;
                    if (i < previous.operatorCount - 1 && i < part->generator.operatorCount - 1 &&
                        op->config.indexMode == old->config.indexMode) {
                        op->stage = old->stage;
                        op->stageStart = old->stageStart;
                        op->stagePosition = old->stagePosition;
                        op->indexEnvelope = op->stage == FM_ENV_SUSTAIN && op->config.indexMode == FM_INDEX_ADSR
                            ? op->config.sustainPercent / 100.0 : old->indexEnvelope;
                    }
                }
            }
            part->pitchMultiplier = exp2(settings->detuneCents / 1200.0);
            part->mixGain = settings->gain / layerCount;
        }
    }
    SDL_UnlockAudioDevice(device);
    return 0;
}

void synthShutdown(void)
{
    if (device != 0) {
        SDL_CloseAudioDevice(device);
        device = 0;
    }
}

int synthAudioSnapshot(float *samples, uint64_t *position)
{
    if (device == 0)
        return 0;
    SDL_LockAudioDevice(device);
    int first = SYNTH_ANALYSIS_SAMPLES - analysisCursor;
    memcpy(samples, analysisSamples + analysisCursor, (size_t)first * sizeof(*samples));
    memcpy(samples + first, analysisSamples, (size_t)analysisCursor * sizeof(*samples));
    *position = samplePosition;
    SDL_UnlockAudioDevice(device);
    return sampleRate;
}

void registerNote(int note)
{
    if (device == 0 || note < 0 || note >= NOTE_COUNT)
        return;
    SDL_LockAudioDevice(device);
    Voice *voice = &voices[note];
    if (!voice->held) {
        bool reset = voice->envelope.level == 0;
        outputEnvelopeOn(&voice->envelope);
        for (int layer = 0; layer < layerCount; ++layer) {
            LayerVoice *part = &voice->layers[layer];
            fmNoteOn(&part->generator, reset);
        }
    }
    voice->held = true;
    voice->active = true;
    SDL_UnlockAudioDevice(device);
}

void deregisterNote(int note)
{
    if (device == 0 || note < 0 || note >= NOTE_COUNT)
        return;
    SDL_LockAudioDevice(device);
    Voice *voice = &voices[note];
    if (voice->held) {
        voice->held = false;
        outputEnvelopeOff(&voice->envelope);
        for (int layer = 0; layer < layerCount; ++layer) {
            LayerVoice *part = &voice->layers[layer];
            fmNoteOff(&part->generator);
        }
    }
    SDL_UnlockAudioDevice(device);
}
