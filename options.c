#include "options.h"

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool parseNumber(const char *value, double *result)
{
    char *end;
    errno = 0;
    double number = strtod(value, &end);
    if (end == value || *end != '\0' || errno != 0 || !isfinite(number))
        return false;
    *result = number;
    return true;
}

static bool parseAdsr(const char *value, FmOperatorConfig *config)
{
    int parts[4];
    for (int i = 0; i < 4; ++i) {
        if (!isdigit((unsigned char)*value))
            return false;
        char *end;
        errno = 0;
        long number = strtol(value, &end, 10);
        if (errno != 0 || number < 0 || number > INT_MAX ||
            *end != (i == 3 ? '\0' : ','))
            return false;
        parts[i] = (int)number;
        value = i == 3 ? end : end + 1;
    }
    if (parts[2] > 100)
        return false;
    config->attackMs = parts[0];
    config->decayMs = parts[1];
    config->sustainPercent = parts[2];
    config->releaseMs = parts[3];
    return true;
}

/* Parse a strictly positive, bounded integer in an option's N slot. */
static bool optionIndex(const char *text, int maximum, int *index, const char **rest)
{
    if (!isdigit((unsigned char)*text) || *text == '0')
        return false;
    char *end;
    errno = 0;
    long number = strtol(text, &end, 10);
    if (errno != 0 || number < 1 || number > maximum || *end != '-')
        return false;
    *index = (int)number - 1;
    *rest = end + 1;
    return true;
}

static bool parseCount(const char *value, int maximum, int *count)
{
    char *end;
    errno = 0;
    long number = strtol(value, &end, 10);
    if (!isdigit((unsigned char)*value) || *end != '\0' || errno != 0 ||
        number < 1 || number > maximum)
        return false;
    *count = (int)number;
    return true;
}

static bool operatorField(const char *field)
{
    return strcmp(field, "ratio") == 0 || strcmp(field, "rm") == 0 ||
           strcmp(field, "i") == 0 || strcmp(field, "i-decay") == 0 ||
           strcmp(field, "i-adsr") == 0 || strcmp(field, "waveform") == 0 ||
           strcmp(field, "pulse-width") == 0 || strcmp(field, "vibrato-rate") == 0 ||
           strcmp(field, "vibrato-depth") == 0;
}

int parseOptions(int argc, char *argv[], SynthConfig *config,
                 char *error, size_t errorSize)
{
    *config = synthDefaultConfig();
    bool usedLayers[SYNTH_MAX_LAYERS] = {false};
    bool usedOps[SYNTH_MAX_LAYERS][FM_MAX_OPERATORS] = {{false}};
    bool modulatorSettings[SYNTH_MAX_LAYERS][FM_MAX_OPERATORS] = {{false}};
    for (int i = 1; i < argc; ++i) {
        const char *option = argv[i];
        if (strcmp(option, "--help") == 0 || strcmp(option, "-h") == 0)
            return 1;
        double *effectValue = NULL;
        if (strcmp(option, "--echo-mix") == 0) effectValue = &config->effects.echoMix;
        else if (strcmp(option, "--echo-delay") == 0) effectValue = &config->effects.echoDelayMs;
        else if (strcmp(option, "--echo-feedback") == 0) effectValue = &config->effects.echoFeedback;
        else if (strcmp(option, "--reverb-mix") == 0) effectValue = &config->effects.reverbMix;
        else if (strcmp(option, "--reverb-room") == 0) effectValue = &config->effects.reverbRoom;
        else if (strcmp(option, "--reverb-damping") == 0) effectValue = &config->effects.reverbDamping;
        if (effectValue) {
            if (++i == argc || !parseNumber(argv[i], effectValue) || !synthEffectsConfigValid(&config->effects)) {
                snprintf(error, errorSize, "Invalid value for %s", option); return -1;
            }
            continue;
        }
        if (strcmp(option, "--master-adsr") == 0) {
            FmOperatorConfig parsed = {0};
            if (++i == argc || !parseAdsr(argv[i], &parsed)) {
                snprintf(error, errorSize, "Invalid --master-adsr: expected A,D,S,R (ms,ms,percent,ms)");
                return -1;
            }
            config->outputEnvelope = (SynthEnvelopeConfig){parsed.attackMs, parsed.decayMs,
                                                          parsed.sustainPercent, parsed.releaseMs};
            continue;
        }
        bool layerCount = strcmp(option, "--layers") == 0 || strcmp(option, "--oscillators") == 0;
        int layerIndex = 0, opIndex = -1;
        const char *field = NULL;
        const char *suffix = NULL;
        if (!layerCount && strncmp(option, "--layer", 7) == 0) {
            if (!optionIndex(option + 7, SYNTH_MAX_LAYERS, &layerIndex, &suffix)) {
                snprintf(error, errorSize, "Invalid layer option: %s (layers 1-%d)",
                         option, SYNTH_MAX_LAYERS);
                return -1;
            }
        } else if (!layerCount && strncmp(option, "--", 2) == 0) {
            suffix = option + 2;
        }
        if (suffix != NULL && strncmp(suffix, "op", 2) == 0 && strcmp(suffix, "ops") != 0) {
            if (!optionIndex(suffix + 2, FM_MAX_OPERATORS, &opIndex, &field)) {
                snprintf(error, errorSize, "Invalid operator option: %s (operators 1-%d)",
                         option, FM_MAX_OPERATORS);
                return -1;
            }
            if (!operatorField(field))
                field = NULL;
        } else if (suffix != NULL && (strcmp(suffix, "ops") == 0 || strcmp(suffix, "rm") == 0 ||
                   (strncmp(option, "--layer", 7) == 0 &&
                    (strcmp(suffix, "gain") == 0 || strcmp(suffix, "detune") == 0)))) {
            field = suffix;
        }
        if (!layerCount && field == NULL) {
            snprintf(error, errorSize, "Unknown option: %s", option);
            return -1;
        }
        if (++i == argc || strncmp(argv[i], "--", 2) == 0) {
            snprintf(error, errorSize, "Missing value for %s", option);
            return -1;
        }
        const char *value = argv[i];
        bool valid = true;
        if (layerCount) {
            valid = parseCount(value, SYNTH_MAX_LAYERS, &config->layerCount);
        } else {
            usedLayers[layerIndex] = true;
            SynthLayerConfig *layer = &config->layers[layerIndex];
            if (opIndex < 0) {
                if (strcmp(field, "ops") == 0) {
                    valid = parseCount(value, FM_MAX_OPERATORS, &layer->fm.operatorCount);
                } else {
                    double number;
                    valid = parseNumber(value, &number);
                    if (valid) {
                        if (strcmp(field, "rm") == 0) {
                            valid = number >= 0.0;
                            for (int op = 0; op < FM_MAX_OPERATORS; ++op)
                                layer->fm.operators[op].rm = number;
                        } else if (strcmp(field, "gain") == 0) {
                            valid = number >= 0.0 && number <= 1.0;
                            layer->gain = number;
                        } else {
                            valid = number >= -4800.0 && number <= 4800.0;
                            layer->detuneCents = number;
                        }
                    }
                }
            } else {
                FmOperatorConfig *op = &layer->fm.operators[opIndex];
                usedOps[layerIndex][opIndex] = true;
                modulatorSettings[layerIndex][opIndex] |= strcmp(field, "rm") == 0 ||
                    strcmp(field, "i") == 0 || strcmp(field, "i-decay") == 0 ||
                    strcmp(field, "i-adsr") == 0;
                if (strcmp(field, "waveform") == 0) {
                    valid = oscillatorParseWaveform(value, &op->waveform);
                } else if (strcmp(field, "i") == 0) {
                    if (strcmp(value, "sustain") == 0 || strcmp(value, "default") == 0)
                        op->indexMode = FM_INDEX_SUSTAIN;
                    else if (strcmp(value, "decay") == 0)
                        op->indexMode = FM_INDEX_DECAY;
                    else if (strcmp(value, "adsr") == 0)
                        op->indexMode = FM_INDEX_ADSR;
                    else
                        valid = false;
                } else if (strcmp(field, "i-adsr") == 0) {
                    valid = parseAdsr(value, op);
                } else {
                    double number;
                    valid = parseNumber(value, &number);
                    if (valid) {
                        if (strcmp(field, "ratio") == 0) {
                            valid = number > 0.0;
                            op->ratio = number;
                        } else if (strcmp(field, "pulse-width") == 0) {
                            valid = number > 0.0 && number < 1.0;
                            op->pulseWidth = number;
                        } else if (strcmp(field, "vibrato-depth") == 0) {
                            valid = number >= 0.0 && number <= 1200.0;
                            op->vibratoDepthCents = number;
                        } else if (strcmp(field, "vibrato-rate") == 0) {
                            valid = number >= 0.0;
                            op->vibratoRateHz = number;
                        } else if (strcmp(field, "rm") == 0) {
                            valid = number >= 0.0;
                            op->rm = number;
                        } else {
                            valid = number >= 0.0;
                            op->decayRate = number;
                        }
                    }
                }
            }
        }
        if (!valid) {
            snprintf(error, errorSize, "Invalid value for %s: %s", option, value);
            return -1;
        }
    }
    /* Resolve references last, so layer/operator counts may appear anywhere. */
    for (int layer = 0; layer < SYNTH_MAX_LAYERS; ++layer) {
        if (usedLayers[layer] && layer >= config->layerCount) {
            snprintf(error, errorSize, "Layer %d does not exist with --layers %d",
                     layer + 1, config->layerCount);
            return -1;
        }
        int count = config->layers[layer].fm.operatorCount;
        for (int op = 0; op < FM_MAX_OPERATORS; ++op) {
            if (usedOps[layer][op] && op >= count) {
                snprintf(error, errorSize, "Layer %d OP%d does not exist with %d operators",
                         layer + 1, op + 1, count);
                return -1;
            }
            if (op == count - 1 && modulatorSettings[layer][op]) {
                snprintf(error, errorSize,
                         "Layer %d OP%d is the output carrier; depth/index options need a modulator",
                         layer + 1, op + 1);
                return -1;
            }
        }
    }
    return 0;
}

void printUsage(const char *program)
{
    printf("Usage: %s [options]\n"
           "  --layers N               Independent sound layers, 1-%d (default 1)\n"
           "  --oscillators N          Alias for --layers\n"
           "  --layerN-ops M           FM operators in layer N, 1-%d (default 2)\n"
           "  --layerN-gain NUMBER     Layer gain 0..1 (default 1)\n"
           "  --layerN-detune NUMBER   Layer pitch offset in cents, -4800..4800 (default 0)\n"
           "  --ops N                  Operator count in layer 1\n"
           "  --rm NUMBER              Depth/index for all layer 1 modulators (default 2)\n"
           "  --opN-ratio NUMBER       Operator frequency / base frequency > 0 (default 1)\n"
           "  --opN-waveform NAME      sine, square, triangle, sawtooth (or saw), pulse, noise\n"
           "  --opN-pulse-width NUMBER Pulse high fraction, 0 < value < 1 (default 0.25)\n"
           "  --opN-vibrato-rate NUM   Vibrato rate in Hz, >= 0 (default 5)\n"
           "  --opN-vibrato-depth NUM  Vibrato depth in cents, 0..1200 (default 0, off)\n"
           "  --opN-rm NUMBER          Depth/index for OP N -> OP N+1, >= 0\n"
           "  --opN-i MODE             sustain (default), default, decay, or adsr\n"
           "  --opN-i-decay NUMBER     Exponential decay rate per second >= 0 (default 2)\n"
           "  --opN-i-adsr A,D,S,R     Integer attack/decay/release ms, sustain 0-100%%\n"
           "                           Defaults: 10,200,50,300; times may be zero\n"
           "  --echo-mix NUMBER        Echo wet gain 0..1 (default 0)\n"
           "  --echo-delay MS          Echo delay 1..2000 ms (default 300)\n"
           "  --echo-feedback NUMBER   Repeat feedback 0..0.95 (default 0.35)\n"
           "  --reverb-mix NUMBER      Reverb wet gain 0..1 (default 0)\n"
           "  --reverb-room NUMBER     Room feedback 0..0.95 (default 0.7)\n"
           "  --reverb-damping NUMBER  High-frequency damping 0..1 (default 0.4)\n"
           "  --master-adsr A,D,S,R    Output amplitude ADSR (default 5,0,100,5)\n"
           "  --help, -h              Show this help\n"
           "Use --layerL-opN-* for operator N in layer L, e.g. --layer2-op1-rm 3.\n"
           "Unprefixed operator/chain options address layer 1; --layerL-rm sets its depths.\n"
           "Each layer routes OP1 -> ... -> final OP (carrier). Layers play together.\n"
           "Carrier options: ratio, waveform, pulse width, and vibrato.\n"
           "--ops 1 plays one oscillator without FM. Depth options apply in argument order.\n"
           "Envelope parameters apply to the mode selected by --opN-i.\n"
           "Play Z-M, A-L, Q-P for C-major notes; Escape quits.\n",
           program, SYNTH_MAX_LAYERS, FM_MAX_OPERATORS);
}
