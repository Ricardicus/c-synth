#include "synth.h"
#include "spectrum.h"

#include <SDL.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#define CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "%s:%d: %s (%s)\n", __FILE__, __LINE__, #condition, SDL_GetError()); \
        exit(1); \
    } \
} while (0)

static float peak(const Spectrum *spectrum, int first, int last)
{
    float value = -90;
    for (int i = first; i <= last; ++i)
        value = fmaxf(value, spectrum->db[i]);
    return value;
}

int main(void)
{
    CHECK(SDL_setenv("SDL_AUDIODRIVER", "dummy", 1) == 0);
    SDL_SetMainReady();
    CHECK(SDL_Init(SDL_INIT_AUDIO) == 0);
    SynthConfig config = synthDefaultConfig();
    config.layerCount = 2;
    config.outputEnvelope.releaseMs = 200;
    config.layers[0].fm.operatorCount = 1;
    config.layers[1].fm.operatorCount = 2;
    config.layers[1].fm.operators[0].rm = 0;
    config.layers[1].fm.operators[0].indexMode = FM_INDEX_ADSR;
    config.layers[1].fm.operators[0].releaseMs = 200;
    config.layers[1].fm.operators[1].ratio = 2;
    CHECK(synthInitWithLayers(&config) == 0);
    float samples[SYNTH_ANALYSIS_SAMPLES];
    uint64_t position;
    CHECK(synthAudioSnapshot(samples, &position) == 48000);
    registerNote(69);
    registerNote(69);
    registerNote(-1);
    registerNote(128);
    SDL_Delay(150);
    CHECK(synthAudioSnapshot(samples, &position) == 48000);
    CHECK(position >= SYNTH_ANALYSIS_SAMPLES);
    for (int i = 0; i < SYNTH_ANALYSIS_SAMPLES; ++i)
        CHECK(isfinite(samples[i]) && fabs(samples[i]) <= 0.101);
    Spectrum spectrum;
    spectrumInit(&spectrum);
    spectrumCompute(&spectrum, samples);
    /* Both independent carriers must survive mixing into the output. */
    CHECK(peak(&spectrum, 17, 21) > -29 && peak(&spectrum, 17, 21) < -25);
    CHECK(peak(&spectrum, 35, 40) > -30 && peak(&spectrum, 35, 40) < -25);
    deregisterNote(69);
    deregisterNote(69);
    SDL_Delay(90);
    CHECK(synthAudioSnapshot(samples, &position) == 48000);
    spectrumCompute(&spectrum, samples);
    /* Both layers share the master amplitude release. */
    CHECK(peak(&spectrum, 17, 21) > -35 && peak(&spectrum, 17, 21) < -28);
    CHECK(peak(&spectrum, 35, 40) > -35 && peak(&spectrum, 35, 40) < -28);
    SDL_Delay(200);
    CHECK(synthAudioSnapshot(samples, &position) == 48000);
    spectrumCompute(&spectrum, samples);
    CHECK(peak(&spectrum, 0, SPECTRUM_BINS - 1) == -90);
    /* Reconfigure a held voice: the carrier must move without another note-on. */
    config = synthPresetConfig(1);
    CHECK(synthConfigure(&config) == 0);
    registerNote(69);
    SDL_Delay(100);
    CHECK(synthAudioSnapshot(samples, &position) == 48000);
    spectrumCompute(&spectrum, samples);
    CHECK(peak(&spectrum, 17, 21) > -25);
    config.layers[0].fm.operators[0].ratio = 3;
    CHECK(synthConfigure(&config) == 0);
    SDL_Delay(100);
    CHECK(synthAudioSnapshot(samples, &position) == 48000);
    spectrumCompute(&spectrum, samples);
    CHECK(peak(&spectrum, 54, 59) > -25);
    CHECK(peak(&spectrum, 17, 21) < -60);
    float fullVolumePeak = peak(&spectrum, 54, 59);
    /* Live output sustain changes amplitude without releasing the held note. */
    config.outputEnvelope.sustainPercent = 25;
    CHECK(synthConfigure(&config) == 0);
    SDL_Delay(100);
    CHECK(synthAudioSnapshot(samples, &position) == 48000);
    spectrumCompute(&spectrum, samples);
    CHECK(fabs((fullVolumePeak - peak(&spectrum, 54, 59)) - 12.0412) < .15);
    SynthConfig invalid = config;
    invalid.layerCount = 0;
    CHECK(synthConfigure(&invalid) == -1);
    for (int i = 0; i < SYNTH_PRESET_COUNT; ++i) {
        deregisterNote(69);
        config = synthPresetConfig(i);
        CHECK(synthConfigValid(&config));
        CHECK(synthConfigure(&config) == 0);
        registerNote(69);
        SDL_Delay(50);
        CHECK(synthAudioSnapshot(samples, &position) == 48000);
        double energy = 0;
        for (int j = 0; j < SYNTH_ANALYSIS_SAMPLES; ++j) {
            CHECK(isfinite(samples[j]) && fabs(samples[j]) <= .101);
            energy += samples[j] * samples[j];
        }
        CHECK(energy > .001);
    }
    deregisterNote(69);
    synthShutdown();
    CHECK(synthAudioSnapshot(samples, &position) == 0);
    /* The flute's fundamental dominates the breath layer and higher partials. */
    config = synthPresetConfig(12);
    CHECK(synthInitWithLayers(&config) == 0);
    registerNote(72);
    SDL_Delay(220);
    CHECK(synthAudioSnapshot(samples, &position) == 48000);
    spectrumCompute(&spectrum, samples);
    CHECK(peak(&spectrum, 20, 25) > -32);
    CHECK(peak(&spectrum, 20, 25) > peak(&spectrum, 40, 500) + 12);
    synthShutdown();
    /* Echo survives note-off and appears in the captured final audio output. */
    config = synthPresetConfig(1);
    config.outputEnvelope.releaseMs = 0;
    config.effects.echoMix = .7;
    config.effects.echoDelayMs = 100;
    config.effects.echoFeedback = .4;
    CHECK(synthInitWithLayers(&config) == 0);
    registerNote(69);
    SDL_Delay(80);
    deregisterNote(69);
    SDL_Delay(60);
    CHECK(synthAudioSnapshot(samples, &position) == 48000);
    double tailEnergy = 0;
    for (int i = 0; i < SYNTH_ANALYSIS_SAMPLES; ++i) tailEnergy += samples[i] * samples[i];
    CHECK(tailEnergy > .01);
    synthShutdown();
    CHECK(synthInit() == 0);
    synthShutdown();
    SDL_Quit();
    puts("Layer mixing, output snapshot, note release, and reinitialization checks passed.");
    return 0;
}
