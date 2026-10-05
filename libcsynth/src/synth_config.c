#include "synth_config.h"

#include <math.h>
#include <stddef.h>

#include "factory_bank.inc"

enum { ORIGINAL_PRESETS = 13 };
_Static_assert(ORIGINAL_PRESETS + sizeof(factoryRecipes) / sizeof(factoryRecipes[0]) ==
               SYNTH_PRESET_COUNT, "Factory preset count must match the bank");

SynthConfig synthDefaultConfig(void)
{
    SynthConfig config = {.layerCount = 1, .outputEnvelope = {5, 0, 100, 5}};
    config.effects = (SynthEffectsConfig){0, 300, .35, 0, .7, .4};
    for (int i = 0; i < SYNTH_MAX_LAYERS; ++i)
        config.layers[i] = (SynthLayerConfig){.fm = fmDefaultConfig(), .gain = 1.0};
    return config;
}

bool synthConfigValid(const SynthConfig *config)
{
    if (config == NULL || config->layerCount < 1 || config->layerCount > SYNTH_MAX_LAYERS)
        return false;
    if (!synthEffectsConfigValid(&config->effects)) return false;
    const SynthEnvelopeConfig *env = &config->outputEnvelope;
    if (env->attackMs < 0 || env->decayMs < 0 || env->releaseMs < 0 ||
        env->sustainPercent < 0 || env->sustainPercent > 100)
        return false;
    for (int i = 0; i < config->layerCount; ++i) {
        const SynthLayerConfig *layer = &config->layers[i];
        if (!fmConfigValid(&layer->fm) || !isfinite(layer->gain) ||
            layer->gain < 0.0 || layer->gain > 1.0 ||
            !isfinite(layer->detuneCents) || layer->detuneCents < -4800.0 ||
            layer->detuneCents > 4800.0)
            return false;
    }
    return true;
}

const char *synthPresetName(int index)
{
    static const char *names[ORIGINAL_PRESETS] = {
        "Classic FM", "Pure sine", "Warm triangle", "Saw lead",
        "Pulse bass", "Electric piano", "Glass bell", "Metal chime",
        "Soft organ", "Wide pad", "Brass", "Space wobble", "Flute"
    };
    if (index < 0 || index >= SYNTH_PRESET_COUNT) return "Custom";
    return index < ORIGINAL_PRESETS ? names[index] : factoryRecipes[index - ORIGINAL_PRESETS].name;
}

SynthConfig synthPresetConfig(int index)
{
    SynthConfig c = synthDefaultConfig();
    if (index >= ORIGINAL_PRESETS && index < SYNTH_PRESET_COUNT) {
        const FactoryRecipe *recipe = &factoryRecipes[index - ORIGINAL_PRESETS];
        c.outputEnvelope = recipe->envelope;
        c.effects = recipe->effects;
        c.layerCount = recipe->layerCount;
        for (int l = 0; l < c.layerCount; ++l) {
            const SynthLayerConfig *source = &recipe->layers[l];
            c.layers[l].gain = source->gain;
            c.layers[l].detuneCents = source->detuneCents;
            c.layers[l].fm.operatorCount = source->fm.operatorCount;
            for (int op = 0; op < source->fm.operatorCount; ++op)
                c.layers[l].fm.operators[op] = source->fm.operators[op];
        }
        return c;
    }
    FmConfig *f = &c.layers[0].fm;
    FmOperatorConfig *m = &f->operators[0], *carrier = &f->operators[1];
    switch (index) {
    case 1: case 2: case 3: case 4:
        f->operatorCount = 1;
        m->waveform = index == 1 ? WAVE_SINE : index == 2 ? WAVE_TRIANGLE :
                      index == 3 ? WAVE_SAWTOOTH : WAVE_PULSE;
        if (index == 3) {
            c.outputEnvelope = (SynthEnvelopeConfig){8, 180, 85, 120};
            c.effects.reverbMix = .08;
            c.layers[0].gain = .85;
        }
        if (index == 4) {
            m->ratio = 0.5;
            c.outputEnvelope = (SynthEnvelopeConfig){5, 220, 70, 90};
            c.layers[0].gain = .8;
        }
        break;
    case 5:
        c.effects.reverbMix = .12;
        c.outputEnvelope = (SynthEnvelopeConfig){5, 900, 20, 350};
        m->ratio = 2; m->rm = 2.5; m->indexMode = FM_INDEX_ADSR;
        m->attackMs = 2; m->decayMs = 650; m->sustainPercent = 12; m->releaseMs = 350;
        break;
    case 6: case 7:
        c.effects.reverbMix = .18;
        c.effects.reverbDamping = .45;
        c.outputEnvelope = (SynthEnvelopeConfig){2, 2200, 0, 1200};
        m->ratio = index == 6 ? 3.5 : 7.13; m->rm = index == 6 ? 1.8 : 3;
        m->indexMode = FM_INDEX_ADSR; m->attackMs = 0; m->decayMs = 1800;
        m->sustainPercent = 0; m->releaseMs = 1200;
        break;
    case 8:
        c.outputEnvelope = (SynthEnvelopeConfig){12, 0, 100, 100};
        c.effects.reverbMix = .14;
        c.layerCount = 3;
        for (int i = 0; i < 3; ++i) {
            c.layers[i].fm.operatorCount = 1;
            c.layers[i].fm.operators[0].ratio = i == 0 ? 1 : i == 1 ? 2 : 4;
            c.layers[i].gain = i == 0 ? 1 : 0.6;
        }
        break;
    case 9:
        c.effects.reverbMix = .24;
        c.effects.reverbDamping = .65;
        c.outputEnvelope = (SynthEnvelopeConfig){700, 900, 75, 1000};
        c.layerCount = 3;
        for (int i = 0; i < 3; ++i) {
            c.layers[i].detuneCents = (i - 1) * 9;
            FmOperatorConfig *op = &c.layers[i].fm.operators[0];
            op->rm = 0.8; op->indexMode = FM_INDEX_ADSR;
            op->attackMs = 700; op->decayMs = 900; op->sustainPercent = 65; op->releaseMs = 1000;
            c.layers[i].fm.operators[1].vibratoDepthCents = 5;
        }
        break;
    case 10:
        c.effects.reverbMix = .1;
        c.outputEnvelope = (SynthEnvelopeConfig){100, 250, 80, 180};
        m->rm = 3; m->indexMode = FM_INDEX_ADSR; m->attackMs = 100;
        m->decayMs = 250; m->sustainPercent = 55; m->releaseMs = 180;
        carrier->waveform = WAVE_TRIANGLE;
        break;
    case 11:
        c.outputEnvelope = (SynthEnvelopeConfig){25, 350, 80, 450};
        c.effects = (SynthEffectsConfig){.18, 340, .35, .2, .75, .55};
        m->ratio = 0.5; m->rm = 5; m->vibratoRateHz = 2; m->vibratoDepthCents = 300;
        carrier->vibratoRateHz = 4; carrier->vibratoDepthCents = 25;
        break;
    case 12:
        c.layerCount = 2;
        c.outputEnvelope = (SynthEnvelopeConfig){45, 180, 88, 220};
        m->ratio = 2; m->rm = .12; m->indexMode = FM_INDEX_ADSR;
        m->attackMs = 35; m->decayMs = 160; m->sustainPercent = 35; m->releaseMs = 160;
        carrier->vibratoRateHz = 5.2; carrier->vibratoDepthCents = 9;
        c.layers[1].fm.operatorCount = 1;
        c.layers[1].fm.operators[0].waveform = WAVE_NOISE;
        c.layers[1].gain = .025;
        c.effects.reverbMix = .12;
        c.effects.reverbRoom = .65;
        c.effects.reverbDamping = .65;
        break;
    default: break;
    }
    return c;
}

bool synthEffectsConfigValid(const SynthEffectsConfig *c)
{
    return c && isfinite(c->echoMix) && c->echoMix >= 0 && c->echoMix <= 1 &&
        isfinite(c->echoDelayMs) && c->echoDelayMs >= 1 && c->echoDelayMs <= 2000 &&
        isfinite(c->echoFeedback) && c->echoFeedback >= 0 && c->echoFeedback <= .95 &&
        isfinite(c->reverbMix) && c->reverbMix >= 0 && c->reverbMix <= 1 &&
        isfinite(c->reverbRoom) && c->reverbRoom >= 0 && c->reverbRoom <= .95 &&
        isfinite(c->reverbDamping) && c->reverbDamping >= 0 && c->reverbDamping <= 1;
}
