#pragma once
#include "Parameters.h"
#include "Spectrogram.h"
#include <array>
#include <atomic>
#include <memory>

class CSynthProcessor final : public juce::AudioProcessor {
public:
    CSynthProcessor();
    ~CSynthProcessor() override;
    void prepareToPlay(double, int) override;
    void releaseResources() override;
    void reset() override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    bool isBusesLayoutSupported(const BusesLayout&) const override;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "CSynth"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override;
    int getNumPrograms() override { return SYNTH_PRESET_COUNT; }
    int getCurrentProgram() override { return juce::jmax(0,selectedPreset.load()); }
    void setCurrentProgram(int) override;
    const juce::String getProgramName(int index) override { return synthPresetName(index); }
    void changeProgramName(int,const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*,int) override;

    SynthConfig readConfig() const;
    double echoTempo() const { return tempo.load(); }
    bool hasEchoTempo() const { return tempoReceived.load(); }
    double effectiveEchoDelayMs() const;
    void applyConfig(const SynthConfig&, int factoryIndex = -1);
    int factorySelection() const { return selectedPreset.load(); }
    void releaseAllNotes() { releaseRequested.store(true); }
    bool engineReady() const { return ready.load(); }
    juce::AudioProcessorValueTreeState state;
    juce::MidiKeyboardState keyboard;
    SpectrumTap spectrumTap;
private:
    void updatePatch();
    void handleMidi(const juce::MidiMessage&);
    void syncNote(int);
    void render(juce::AudioBuffer<float>&, int offset, int count);
    void clearMidi();
    std::unique_ptr<Synth,decltype(&synthDestroy)> engine{nullptr,synthDestroy};
    std::vector<std::atomic<float>*> values;
    std::vector<float> lastValues;
    std::atomic<float>* outputGain = nullptr;
    std::atomic<float>* echoTimingParameter = nullptr;
    std::atomic<float>* freeEchoDelay = nullptr;
    std::atomic<double> tempo{120};
    std::atomic<bool> tempoReceived{false};
    double appliedEchoDelay=-1;
    juce::SmoothedValue<float> gain;
    std::array<std::array<bool,128>,16> down{}, latched{};
    std::array<std::array<int,128>,16> velocities{};
    std::array<bool,16> pedal{};
    std::array<int,128> soundingVelocity{};
    std::atomic<int> selectedPreset{0};
    std::atomic<bool> ready{false}, releaseRequested{false};
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CSynthProcessor)
};
