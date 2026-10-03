#include "options.h"

#include <stdio.h>
#include <stdlib.h>

#define CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition); \
        exit(1); \
    } \
} while (0)

static void reject(const char *option, const char *value)
{
    char *args[] = {"synth", (char *)option, (char *)value};
    SynthConfig config;
    char error[256] = {0};
    CHECK(parseOptions(value == NULL ? 2 : 3, args, &config, error, sizeof(error)) == -1);
    CHECK(error[0] != '\0');
}

int main(void)
{
    SynthConfig config;
    char error[256];
    char *defaults[] = {"synth"};
    CHECK(parseOptions(1, defaults, &config, error, sizeof(error)) == 0);
    CHECK(config.layers[0].fm.operators[0].indexMode == FM_INDEX_SUSTAIN && config.layers[0].fm.operators[0].rm == 2 && config.layers[0].fm.operators[0].ratio == 1);
    char *valid[] = {"synth", "--rm", "3.5", "--op1-ratio", "2", "--op1-i", "adsr",
                     "--op1-i-adsr", "10,200,40,300", "--op1-i-decay", "1.5"};
    CHECK(parseOptions(11, valid, &config, error, sizeof(error)) == 0);
    CHECK(config.layers[0].fm.operators[0].rm == 3.5 && config.layers[0].fm.operators[0].ratio == 2 && config.layers[0].fm.operators[0].indexMode == FM_INDEX_ADSR);
    CHECK(config.layers[0].fm.operators[0].attackMs == 10 && config.layers[0].fm.operators[0].decayMs == 200 && config.layers[0].fm.operators[0].sustainPercent == 40 &&
          config.layers[0].fm.operators[0].releaseMs == 300 && config.layers[0].fm.operators[0].decayRate == 1.5);
    char *decay[] = {"synth", "--op1-i", "decay", "--op1-i-decay", "0"};
    CHECK(parseOptions(5, decay, &config, error, sizeof(error)) == 0);
    CHECK(config.layers[0].fm.operators[0].indexMode == FM_INDEX_DECAY && config.layers[0].fm.operators[0].decayRate == 0);
    char *alias[] = {"synth", "--op1-i", "default"};
    CHECK(parseOptions(3, alias, &config, error, sizeof(error)) == 0);
    CHECK(config.layers[0].fm.operators[0].indexMode == FM_INDEX_SUSTAIN);
    char *sustain[] = {"synth", "--op1-i", "sustain", "--rm", "0"};
    CHECK(parseOptions(5, sustain, &config, error, sizeof(error)) == 0);
    CHECK(config.layers[0].fm.operators[0].indexMode == FM_INDEX_SUSTAIN && config.layers[0].fm.operators[0].rm == 0);
    char *zeros[] = {"synth", "--op1-i-adsr", "0,0,100,0", "--op1-i", "adsr"};
    CHECK(parseOptions(5, zeros, &config, error, sizeof(error)) == 0);
    CHECK(config.layers[0].fm.operators[0].attackMs == 0 && config.layers[0].fm.operators[0].decayMs == 0 && config.layers[0].fm.operators[0].sustainPercent == 100 &&
          config.layers[0].fm.operators[0].releaseMs == 0 && config.layers[0].fm.operators[0].indexMode == FM_INDEX_ADSR);
    char *help[] = {"synth", "--help"};
    CHECK(parseOptions(2, help, &config, error, sizeof(error)) == 1);
    char *chain[] = {"synth", "--op2-rm", "3", "--op2-i", "adsr",
                    "--op2-i-adsr", "20,30,40,50", "--op3-ratio", "0.5",
                    "--ops", "3", "--op1-i", "decay", "--op1-i-decay", "4"};
    CHECK(parseOptions(15, chain, &config, error, sizeof(error)) == 0);
    CHECK(config.layers[0].fm.operatorCount == 3 && config.layers[0].fm.operators[1].rm == 3);
    CHECK(config.layers[0].fm.operators[1].indexMode == FM_INDEX_ADSR &&
          config.layers[0].fm.operators[1].attackMs == 20 && config.layers[0].fm.operators[1].releaseMs == 50);
    CHECK(config.layers[0].fm.operators[0].indexMode == FM_INDEX_DECAY &&
          config.layers[0].fm.operators[0].decayRate == 4 && config.layers[0].fm.operators[2].ratio == 0.5);
    char *depth[] = {"synth", "--ops", "3", "--rm", "5", "--op2-rm", "1"};
    CHECK(parseOptions(7, depth, &config, error, sizeof(error)) == 0);
    CHECK(config.layers[0].fm.operators[0].rm == 5 && config.layers[0].fm.operators[1].rm == 1);
    char *lastWins[] = {"synth", "--ops", "3", "--op2-rm", "1", "--rm", "5"};
    CHECK(parseOptions(7, lastWins, &config, error, sizeof(error)) == 0);
    CHECK(config.layers[0].fm.operators[0].rm == 5 && config.layers[0].fm.operators[1].rm == 5);
    char *single[] = {"synth", "--ops", "1", "--op1-ratio", "2"};
    CHECK(parseOptions(5, single, &config, error, sizeof(error)) == 0);
    CHECK(config.layers[0].fm.operatorCount == 1 && config.layers[0].fm.operators[0].ratio == 2);
    char *maximum[] = {"synth", "--op7-i", "decay", "--op8-ratio", "1.5", "--ops", "8"};
    CHECK(parseOptions(7, maximum, &config, error, sizeof(error)) == 0);
    CHECK(config.layers[0].fm.operatorCount == 8 && config.layers[0].fm.operators[6].indexMode == FM_INDEX_DECAY &&
          config.layers[0].fm.operators[7].ratio == 1.5);
    CHECK(config.layers[0].fm.operators[0].waveform == WAVE_SINE && config.layers[0].fm.operators[7].waveform == WAVE_SINE);
    char *waves[] = {"synth", "--ops", "3", "--op1-waveform", "square",
                    "--op2-waveform", "triangle", "--op3-waveform", "sawtooth"};
    CHECK(parseOptions(9, waves, &config, error, sizeof(error)) == 0);
    CHECK(config.layers[0].fm.operators[0].waveform == WAVE_SQUARE &&
          config.layers[0].fm.operators[1].waveform == WAVE_TRIANGLE &&
          config.layers[0].fm.operators[2].waveform == WAVE_SAWTOOTH);
    char *pulse[] = {"synth", "--op1-waveform", "pulse", "--op1-pulse-width", "0.125",
                    "--ops", "1"};
    CHECK(parseOptions(7, pulse, &config, error, sizeof(error)) == 0);
    CHECK(config.layers[0].fm.operators[0].waveform == WAVE_PULSE && config.layers[0].fm.operators[0].pulseWidth == 0.125);
    char *saw[] = {"synth", "--op2-waveform", "saw"};
    CHECK(parseOptions(3, saw, &config, error, sizeof(error)) == 0);
    CHECK(config.layers[0].fm.operators[1].waveform == WAVE_SAWTOOTH);
    reject("--op1-waveform", "unknown");
    reject("--op1-waveform", NULL);
    reject("--op1-waveform", "");
    reject("--op3-waveform", "sine");
    reject("--op1-pulse-width", "0");
    reject("--op1-pulse-width", "1");
    reject("--op1-pulse-width", "-0.1");
    reject("--op1-pulse-width", "nan");
    reject("--op1-pulse-width", "inf");
    char *layers[] = {"synth", "--layer2-ops", "3", "--layer2-op2-rm", "1.5",
                     "--layer2-op3-vibrato-rate", "6", "--layer2-op3-vibrato-depth", "25",
                     "--layer2-op3-waveform", "triangle", "--layer2-detune", "-8",
                     "--layer2-gain", "0.6", "--layers", "2", "--ops", "1",
                     "--op1-vibrato-depth", "10"};
    CHECK(parseOptions(21, layers, &config, error, sizeof(error)) == 0);
    CHECK(config.layerCount == 2 && config.layers[0].fm.operatorCount == 1);
    CHECK(config.layers[1].fm.operatorCount == 3 && config.layers[1].gain == 0.6 &&
          config.layers[1].detuneCents == -8 && config.layers[1].fm.operators[1].rm == 1.5);
    CHECK(config.layers[1].fm.operators[2].waveform == WAVE_TRIANGLE &&
          config.layers[1].fm.operators[2].vibratoRateHz == 6 &&
          config.layers[1].fm.operators[2].vibratoDepthCents == 25);
    CHECK(config.layers[0].fm.operators[0].vibratoDepthCents == 10);
    CHECK(config.layers[0].fm.operators[0].vibratoRateHz == 5);
    CHECK(synthConfigValid(&config));
    char *layerDepth[] = {"synth", "--layers", "2", "--rm", "5", "--layer2-rm", "1"};
    CHECK(parseOptions(7, layerDepth, &config, error, sizeof(error)) == 0);
    CHECK(config.layers[0].fm.operators[0].rm == 5 && config.layers[1].fm.operators[0].rm == 1);
    char *allLayers[] = {"synth", "--oscillators", "8", "--layer8-ops", "8",
                        "--layer8-op8-waveform", "square", "--layer8-op8-vibrato-depth", "20"};
    CHECK(parseOptions(9, allLayers, &config, error, sizeof(error)) == 0);
    CHECK(config.layerCount == 8 && config.layers[7].fm.operatorCount == 8 &&
          config.layers[7].fm.operators[7].waveform == WAVE_SQUARE);
    reject("--layers", "0");
    reject("--layers", "9");
    reject("--layers", "1.5");
    reject("--layer0-ops", "2");
    reject("--layer9-ops", "2");
    reject("--layer2-ops", "2"); /* Layer 2 absent by default. */
    reject("--layer1-gain", "-1");
    reject("--layer1-gain", "1.1");
    reject("--layer1-detune", "4801");
    reject("--layer1-detune", "-4801");
    reject("--op1-vibrato-rate", "-1");
    reject("--op1-vibrato-depth", "-1");
    reject("--op1-vibrato-depth", "1201");
    reject("--op1-vibrato-depth", "nan");
    reject("--layer1-op2-rm", "1");
    reject("--ops", "0");
    reject("--ops", "9");
    reject("--ops", "2.5");
    reject("--ops", "9999999999999999999999");
    reject("--ops", NULL);
    reject("--op0-ratio", "1");
    reject("--op9-ratio", "1");
    reject("--op999999999999999999999-ratio", "1");
    reject("--op3-ratio", "1"); /* Not present in default chain. */
    reject("--op2-rm", "1"); /* Default output carrier. */
    reject("--op2-i", "adsr");
    reject("--op2-i-decay", "2");
    reject("--op2-i-adsr", "1,2,50,4");
    reject("--op1-unknown", "1");
    reject("--op1-ratio", "--ops");
    reject("--unknown", "1");
    reject("--rm", NULL);
    reject("--rm", "");
    reject("--rm", "-1");
    reject("--rm", "nan");
    reject("--rm", "inf");
    reject("--rm", "1e999");
    reject("--rm", "2garbage");
    reject("--op1-ratio", "0");
    reject("--op1-ratio", "-1");
    reject("--op1-i", "invalid");
    reject("--op1-i-decay", "-1");
    reject("--op1-i-adsr", "1,2,3");
    reject("--op1-i-adsr", "1,2,3,4,5");
    reject("--op1-i-adsr", "1,2,101,4");
    reject("--op1-i-adsr", "-1,2,50,4");
    reject("--op1-i-adsr", "1.5,2,50,4");
    reject("--op1-i-adsr", "1,2,,4");
    reject("--op1-i-adsr", "99999999999999999999999,2,50,4");
    char *master[] = {"synth", "--master-adsr", "100,400,65,800"};
    CHECK(parseOptions(3, master, &config, error, sizeof(error)) == 0);
    CHECK(config.outputEnvelope.attackMs == 100 && config.outputEnvelope.decayMs == 400 &&
          config.outputEnvelope.sustainPercent == 65 && config.outputEnvelope.releaseMs == 800);
    reject("--master-adsr", NULL);
    reject("--master-adsr", "-1,2,50,4");
    reject("--master-adsr", "1,2,101,4");
    reject("--master-adsr", "1,2,50");
    reject("--master-adsr", "1,2,50,4,5");
    puts("Command-line defaults, modes, units, and validation checks passed.");
    return 0;
}
