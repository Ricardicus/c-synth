#include "envelope.h"
#include <math.h>

static void stage(OutputEnvelope *env, FmEnvelopeStage next)
{
    env->stage = next;
    env->start = env->level;
    env->position = 0;
}

void outputEnvelopeConfigure(OutputEnvelope *env, SynthEnvelopeConfig config)
{
    if (env->config.attackMs == config.attackMs && env->config.decayMs == config.decayMs &&
        env->config.sustainPercent == config.sustainPercent && env->config.releaseMs == config.releaseMs)
        return;
    env->config = config;
    /* Continue from the current level when a knob changes an active stage. */
    stage(env, env->stage);
}

void outputEnvelopeOn(OutputEnvelope *env)
{
    /* Retrigger release tails from their current level to avoid a discontinuity. */
    stage(env, FM_ENV_ATTACK);
}

void outputEnvelopeOff(OutputEnvelope *env)
{
    if (env->stage != FM_ENV_IDLE && env->stage != FM_ENV_RELEASE)
        stage(env, FM_ENV_RELEASE);
}

double outputEnvelopeNext(OutputEnvelope *env, double sampleRate)
{
    /* Loop also resolves zero-duration attack, decay and release in one sample. */
    for (int i = 0; i < 3; ++i) {
        int duration;
        double target;
        FmEnvelopeStage next;
        switch (env->stage) {
        case FM_ENV_ATTACK:
            duration = env->config.attackMs; target = 1; next = FM_ENV_DECAY; break;
        case FM_ENV_DECAY:
            duration = env->config.decayMs; target = env->config.sustainPercent / 100.0;
            next = FM_ENV_SUSTAIN; break;
        case FM_ENV_RELEASE:
            duration = env->config.releaseMs; target = 0; next = FM_ENV_IDLE; break;
        case FM_ENV_SUSTAIN:
            /* Smooth live sustain edits over 5 ms. */
            target = env->config.sustainPercent / 100.0;
            double step = 1.0 / (.005 * sampleRate);
            if (fabs(target - env->level) <= step + 1e-12)
                env->level = target;
            else
                env->level += target > env->level ? step : -step;
            return env->level;
        default: return 0;
        }
        if (duration == 0) {
            env->level = target;
            stage(env, next);
            continue;
        }
        double progress = fmin(1, ++env->position / (duration * sampleRate / 1000.0));
        env->level = env->start + (target - env->start) * progress;
        if (progress == 1) stage(env, next);
        return env->level;
    }
    return env->level;
}
