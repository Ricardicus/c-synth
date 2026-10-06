#pragma once
#include "PluginProcessor.h"
#include <juce_audio_utils/juce_audio_utils.h>

class CSynthEditor final : public juce::AudioProcessorEditor, private juce::Timer {
public:
    explicit CSynthEditor(CSynthProcessor&);
    ~CSynthEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;
private:
    struct Knob : juce::Component {
        Knob(const juce::String& name, const juce::String& suffix = {});
        void bind(juce::AudioProcessorValueTreeState&, const juce::String& id);
        void resized() override;
        void setAvailability(const juce::String& reason, const juce::String& explanation = {});
        juce::Label label, availability;
        juce::Slider slider;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };
    void bindSelection();
    void timerCallback() override;
    void chooseFile(bool importing);
    void showPage(int page);
    CSynthProcessor& synthProcessor;
    juce::LookAndFeel_V4 look;
    juce::ComboBox presets, layer, op, waveform, envelope, echoTiming;
    juce::TextButton previous{"<"}, next{">"}, import{"Import .synth"}, exportSound{"Export .synth"}, release{"Release notes"};
    juce::Label layerLabel, operatorLabel, status, envelopeAvailability, echoTimingInfo;
    juce::TextButton synthTab{"Synth"}, outputTab{"Output & filters"}, effectsTab{"Effects"}, keyboardTab{"Keyboard"};
    juce::MidiKeyboardComponent keyboard;
    int currentPage=0;
    OutputSpectrogram spectrogram;
    juce::TooltipWindow tooltips{this,500};
    std::vector<std::unique_ptr<Knob>> globalKnobs, effectKnobs, layerKnobs, operatorKnobs;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> waveAttachment, envelopeAttachment, echoTimingAttachment;
    std::unique_ptr<juce::FileChooser> chooser;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CSynthEditor)
};
