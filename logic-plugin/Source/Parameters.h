#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
extern "C" {
#include "synth.h"
#include "presets.h"
}
#include <cstddef>
#include <vector>

namespace parameters {
enum class Kind { real, integer, waveform, indexMode };
struct Spec {
    juce::String id, name;
    std::size_t offset;
    Kind kind;
    float minimum, maximum, interval, skew;
    double read(const SynthConfig&) const;
    void write(SynthConfig&, float) const;
};
const std::vector<Spec>& specs();
juce::AudioProcessorValueTreeState::ParameterLayout layout();
juce::String layerId(int layer, const juce::String& field);
juce::String operatorId(int layer, int op, const juce::String& field);
}
