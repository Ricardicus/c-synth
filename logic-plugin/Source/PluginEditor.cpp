#include "PluginEditor.h"
#include "EchoTiming.h"

CSynthEditor::Knob::Knob(const juce::String& name, const juce::String& suffix)
{
    setName(name);
    label.setText(name,juce::dontSendNotification);
    label.setJustificationType(juce::Justification::centred);
    label.setFont(juce::FontOptions(14.0f));
    slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle(juce::Slider::TextBoxBelow,false,110,22);
    slider.setTextValueSuffix(suffix);
    slider.setPopupDisplayEnabled(true,false,this);
    slider.setVelocityModeParameters(1,1,.1,true,juce::ModifierKeys::shiftModifier);
    availability.setFont(juce::FontOptions(12.0f));
    availability.setJustificationType(juce::Justification::centred);
    availability.setColour(juce::Label::textColourId,juce::Colour(0xffb9c9db));
    addAndMakeVisible(label); addAndMakeVisible(slider); addChildComponent(availability);
}
void CSynthEditor::Knob::bind(juce::AudioProcessorValueTreeState& state, const juce::String& id)
{
    attachment.reset();
    attachment=std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(state,id,slider);
    if (id=="lowpass" || id=="highpass") {
        slider.textFromValueFunction=[](double hz) { return hz<20 ? juce::String("Off") : juce::String(hz,0)+" Hz"; };
        slider.valueFromTextFunction=[](const juce::String& text) { return text.equalsIgnoreCase("Off") ? 0.0 : text.getDoubleValue(); };
    }
}
void CSynthEditor::Knob::setAvailability(const juce::String& reason, const juce::String& explanation)
{
    slider.setEnabled(reason.isEmpty());
    availability.setText(reason,juce::dontSendNotification);
    auto tooltip=explanation.isEmpty() ? reason : explanation;
    label.setTooltip(tooltip); availability.setTooltip(tooltip); slider.setTooltip(tooltip);
    if (availability.isVisible()!=reason.isNotEmpty()) {
        availability.setVisible(reason.isNotEmpty()); resized();
    }
}
void CSynthEditor::Knob::resized()
{
    auto area=getLocalBounds(); label.setBounds(area.removeFromTop(22));
    if (availability.isVisible()) availability.setBounds(area.removeFromBottom(18));
    slider.setBounds(area);
}
CSynthEditor::CSynthEditor(CSynthProcessor& p)
    : AudioProcessorEditor(p), synthProcessor(p), samplePanel(p), keyboard(p.keyboard,juce::MidiKeyboardComponent::horizontalKeyboard), spectrogram(p.spectrumTap)
{
    look.setColour(juce::ResizableWindow::backgroundColourId,juce::Colour(0xff101722));
    look.setColour(juce::Slider::rotarySliderFillColourId,juce::Colour(0xff47c8c0));
    look.setColour(juce::Slider::rotarySliderOutlineColourId,juce::Colour(0xff29374a));
    look.setColour(juce::Slider::textBoxOutlineColourId,juce::Colours::transparentBlack);
    look.setColour(juce::ComboBox::backgroundColourId,juce::Colour(0xff253247));
    look.setColour(juce::TextButton::buttonColourId,juce::Colour(0xff253247));
    look.setColour(juce::TextButton::buttonOnColourId,juce::Colour(0xff21666d));
    setLookAndFeel(&look);
    for (auto* component : std::initializer_list<juce::Component*>{&presets,&previous,&next,&import,&exportSound,&release,
            &layer,&op,&waveform,&envelope,&layerLabel,&operatorLabel,&status,&envelopeAvailability,&spectrogram,&echoTiming,&echoTimingInfo,&synthTab,&outputTab,&effectsTab,&keyboardTab,&samplesTab,&samplePanel,&keyboard}) addAndMakeVisible(component);
    for (auto* tab : {&synthTab,&outputTab,&effectsTab,&keyboardTab,&samplesTab}) {
        tab->setClickingTogglesState(true); tab->setRadioGroupId(1);
    }
    synthTab.onClick=[this] { showPage(0); };
    outputTab.onClick=[this] { showPage(1); };
    effectsTab.onClick=[this] { showPage(2); };
    samplesTab.onClick=[this] { showPage(4); };
    keyboardTab.onClick=[this] { showPage(3); };
    keyboard.setAvailableRange(36,96); keyboard.setLowestVisibleKey(36);
    keyboard.setVelocity(.8f,false); keyboard.setWantsKeyboardFocus(false); keyboard.clearKeyMappings();
    for (int i=0;i<SYNTH_PRESET_COUNT;++i) presets.addItem(synthPresetName(i),i+1);
    presets.setTextWhenNothingSelected("Custom sound");
    presets.onChange=[this] { if (presets.getSelectedId()>0) synthProcessor.setCurrentProgram(presets.getSelectedId()-1); };
    previous.onClick=[this] { int index=synthProcessor.factorySelection(); synthProcessor.setCurrentProgram(index<0 ? SYNTH_PRESET_COUNT-1 : (index+SYNTH_PRESET_COUNT-1)%SYNTH_PRESET_COUNT); };
    next.onClick=[this] { int index=synthProcessor.factorySelection(); synthProcessor.setCurrentProgram(index<0 ? 0 : (index+1)%SYNTH_PRESET_COUNT); };
    import.onClick=[this] { chooseFile(true); }; exportSound.onClick=[this] { chooseFile(false); };
    release.onClick=[this] { synthProcessor.releaseAllNotes(); };
    for (int i=0;i<8;++i) { layer.addItem("Layer "+juce::String(i+1),i+1); op.addItem("Operator "+juce::String(i+1),i+1); }
    layer.setSelectedId(1,juce::dontSendNotification); op.setSelectedId(1,juce::dontSendNotification);
    layer.onChange=[this] { bindSelection(); }; op.onChange=[this] { bindSelection(); };
    waveform.addItemList({"Sine","Square","Triangle","Saw","Pulse","Noise"},1);
    envelope.addItemList({"Sustain","Decay","ADSR"},1);
    echoTiming.setName("Echo timing");
    echoTiming.addItemList(echoTiming::choices(),1);
    echoTiming.setTooltip("Free uses milliseconds. Note divisions follow Logic's tempo; dotted notes last 1.5 times as long, triplets two-thirds as long.");
    echoTimingAttachment=std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(p.state,"echoTiming",echoTiming);
    echoTimingInfo.setFont(juce::FontOptions(13.0f));
    waveform.setTooltip("Waveform of the selected operator. The last active operator is the carrier.");
    envelope.setTooltip("Modulator envelopes shape timbre. Master ADSR controls volume.");
    auto make=[this](auto& list,const char* label,const char* suffix) {
        auto knob=std::make_unique<Knob>(label,suffix); addAndMakeVisible(*knob); list.push_back(std::move(knob));
    };
    const char* globals[]={"attack","decay","sustain","release","lowpass","highpass","gain"};
    const char* globalNames[]={"Attack","Decay","Sustain","Release","Low-pass","High-pass","Output"};
    const char* globalUnits[]={" ms"," ms"," %"," ms","",""," dB"};
    for (int i=0;i<7;++i) { make(globalKnobs,globalNames[i],globalUnits[i]); globalKnobs.back()->bind(p.state,globals[i]); }
    const char* effects[]={"echoMix","echoDelay","echoFeedback","reverbMix","reverbRoom","reverbDamping"};
    const char* effectNames[]={"Echo mix","Echo delay","Feedback","Reverb mix","Room","Damping"};
    for (int i=0;i<6;++i) { make(effectKnobs,effectNames[i],i==1 ? " ms" : ""); effectKnobs.back()->bind(p.state,effects[i]); }
    make(layerKnobs,"Active layers",""); layerKnobs.back()->bind(p.state,"layers");
    make(layerKnobs,"Layer gain",""); make(layerKnobs,"Layer detune"," cents"); make(layerKnobs,"Active operators","");
    const char* opNames[]={"Ratio","FM depth","Pulse width","Vibrato rate","Vibrato depth","Index decay rate","Index attack","Index decay","Index sustain","Index release"};
    const char* opUnits[]={"","",""," Hz"," cents"," /s"," ms"," ms"," %"," ms"};
    for (int i=0;i<10;++i) make(operatorKnobs,opNames[i],opUnits[i]);
    echoTiming.onChange=[this] { timerCallback(); };
    layerLabel.setText("Edit layer",juce::dontSendNotification); operatorLabel.setText("Edit operator",juce::dontSendNotification);
    status.setFont(juce::FontOptions(13.0f));
    envelopeAvailability.setFont(juce::FontOptions(12.0f));
    envelopeAvailability.setColour(juce::Label::textColourId,juce::Colour(0xffb9c9db));
    bindSelection();
    setResizable(true,true); setResizeLimits(1000,650,1600,900); setSize(1180,700);
    showPage(0);
    startTimerHz(15);
}
CSynthEditor::~CSynthEditor()
{
    stopTimer(); keyboard.clearKeyMappings(); waveAttachment.reset(); envelopeAttachment.reset(); echoTimingAttachment.reset();
    globalKnobs.clear(); effectKnobs.clear(); layerKnobs.clear(); operatorKnobs.clear();
    setLookAndFeel(nullptr);
}
void CSynthEditor::bindSelection()
{
    int l=layer.getSelectedId()-1, o=op.getSelectedId()-1;
    layerKnobs[1]->bind(synthProcessor.state,parameters::layerId(l,"gain"));
    layerKnobs[2]->bind(synthProcessor.state,parameters::layerId(l,"detune"));
    layerKnobs[3]->bind(synthProcessor.state,parameters::layerId(l,"count"));
    const char* fields[]={"ratio","depth","pulse","vibRate","vibDepth","expDecay","attack","decay","sustain","release"};
    for (int i=0;i<10;++i) operatorKnobs[static_cast<std::size_t>(i)]->bind(synthProcessor.state,parameters::operatorId(l,o,fields[i]));
    waveAttachment.reset(); envelopeAttachment.reset();
    waveAttachment=std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(synthProcessor.state,parameters::operatorId(l,o,"wave"),waveform);
    envelopeAttachment=std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(synthProcessor.state,parameters::operatorId(l,o,"mode"),envelope);
    timerCallback();
}
void CSynthEditor::timerCallback()
{
    presets.setSelectedId(synthProcessor.factorySelection()+1,juce::dontSendNotification);
    auto config=synthProcessor.readConfig(); int l=layer.getSelectedId()-1, o=op.getSelectedId()-1;
    bool modulator=o<config.layers[l].fm.operatorCount-1;
    int mode=config.layers[l].fm.operators[o].indexMode;
    const bool carrier=o==config.layers[l].fm.operatorCount-1;
    const juce::String roleReason=modulator ? "" : carrier ? "Carrier: select a modulator" : "Inactive: add more operators";
    const juce::String roleHelp=modulator ? "" : carrier
        ? "This operator is the carrier: it produces the layer's audio. FM depth and index envelopes only apply to modulators. Select an earlier operator, or increase Active operators to make this one a modulator. Use master ADSR to shape output volume."
        : "This operator is outside the active chain. Increase Active operators beyond this operator's number to make it a modulator, then edit its FM depth and index envelope.";
    operatorKnobs[1]->setAvailability(roleReason,roleHelp);
    envelope.setEnabled(modulator);
    envelope.setTooltip(modulator ? "Modulator envelopes shape timbre. Master ADSR controls volume." : roleHelp);
    envelopeAvailability.setText(modulator ? "Index envelope shapes FM depth; master ADSR shapes volume."
        : carrier ? "Carrier: FM depth and index envelopes apply only to modulators."
                  : "Inactive operator: increase Active operators to enable modulation.",juce::dontSendNotification);
    envelopeAvailability.setTooltip(roleHelp);
    operatorKnobs[5]->setAvailability(!modulator ? roleReason : mode==FM_INDEX_DECAY ? "" : "Choose Decay mode",
        !modulator ? roleHelp : mode==FM_INDEX_DECAY ? "" : "Index decay rate is used only in Decay mode. Choose Decay in the index-envelope menu above.");
    for (int i=6;i<10;++i) operatorKnobs[static_cast<std::size_t>(i)]->setAvailability(
        !modulator ? roleReason : mode==FM_INDEX_ADSR ? "" : "Choose ADSR mode",
        !modulator ? roleHelp : mode==FM_INDEX_ADSR ? "" : "Index ADSR controls are used only in ADSR mode. Choose ADSR in the index-envelope menu above. Master ADSR remains available for output volume.");
    operatorKnobs[2]->setAvailability(config.layers[l].fm.operators[o].waveform==WAVE_PULSE ? "" : "Choose Pulse waveform",
        config.layers[l].fm.operators[o].waveform==WAVE_PULSE ? "" : "Pulse width is used only by the Pulse waveform. Choose Pulse in the waveform menu above.");
    int timing=juce::roundToInt(synthProcessor.state.getRawParameterValue("echoTiming")->load());
    effectKnobs[1]->setAvailability(timing==0 ? "" : "Tempo sync: choose Free",
        timing==0 ? "" : "The note division determines delay time from project tempo. Choose Free (ms) to edit this knob; its value is preserved while synced.");
    double bpm=synthProcessor.echoTempo();
    double requested=echoTiming::milliseconds(timing,bpm,config.effects.echoDelayMs);
    echoTimingInfo.setText(timing==0 ? "Free delay: "+juce::String(config.effects.echoDelayMs,0)+" ms"
        : juce::String(synthProcessor.effectiveEchoDelayMs(),1)+" ms / "+juce::String(bpm,1)+" BPM"
          +(synthProcessor.hasEchoTempo() ? "" : " (fallback)")
          +(requested>CSYNTH_ECHO_MAX_DELAY_MS ? " / 30 s limit" : requested<1 ? " / 1 ms limit" : ""),juce::dontSendNotification);
    const bool sampled=synthProcessor.usesSamples();
    if (sampled) {
        for (auto& knob : operatorKnobs) knob->setAvailability("Sample source: FM only","These controls shape FM operators. Master ADSR, filters, effects, layer gain and detune still affect samples.");
        layerKnobs[3]->setAvailability("Sample source: FM only"); waveform.setEnabled(false); envelope.setEnabled(false);
        waveform.setTooltip("FM waveforms are replaced by your recordings in sample mode.");
        envelopeAvailability.setText("Sample source: use master ADSR on Output & filters.",juce::dontSendNotification);
    } else {
        layerKnobs[3]->setAvailability(""); waveform.setEnabled(true);
        for (int i : {0,3,4}) operatorKnobs[static_cast<std::size_t>(i)]->setAvailability("");
        waveform.setTooltip("Waveform of the selected operator. The last active operator is the carrier.");
    }
    samplePanel.refresh();
    bool active=l<config.layerCount && o<config.layers[l].fm.operatorCount;
    status.setText(!synthProcessor.engineReady() ? "Audio engine isn't running. In Standalone, choose an audio output in Options." :
        sampled ? "Sample source active. Master ADSR, filters and effects work on your recordings; FM operator controls are inactive." :
        active ? "Layer/operator selection changes what you edit. Last active operator = carrier. Shift-drag for fine control." :
                 "This slot is inactive. Increase the layer/operator counts to hear it; its settings are still saved.",juce::dontSendNotification);
}
void CSynthEditor::chooseFile(bool importing)
{
    chooser=std::make_unique<juce::FileChooser>(importing ? "Import synth setting" : "Export synth setting",
        juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile("CSynth patch.synth"),"*.synth");
    juce::Component::SafePointer<CSynthEditor> safe(this);
    chooser->launchAsync((importing ? juce::FileBrowserComponent::openMode : juce::FileBrowserComponent::saveMode) |
        juce::FileBrowserComponent::canSelectFiles,
        [safe,importing](const juce::FileChooser& selected) {
            if (!safe) return;
            auto file=selected.getResult(); if (file==juce::File{}) return;
            char error[256]={0}; SynthConfig config;
            int result;
            if (importing) {
                char name[PRESET_NAME_MAX+1]; result=presetRead(file.getFullPathName().toRawUTF8(),name,&config,error,sizeof(error));
                if (result==0) safe->synthProcessor.applyConfig(config);
            } else {
                file=file.withFileExtension("synth");
                config=safe->synthProcessor.readConfig();
                auto name=file.getFileNameWithoutExtension().retainCharacters("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789 -_").trim().substring(0,32).trim();
                if (name.isEmpty()) name="My patch";
                result=presetWrite(file.getFullPathName().toRawUTF8(),name.toRawUTF8(),&config,error,sizeof(error));
            }
            if (result!=0) juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,"CSynth",error);
        });
}
void CSynthEditor::showPage(int page)
{
    if (currentPage==3 && page!=3) keyboard.clearKeyMappings();
    currentPage=page;
    samplePanel.setVisible(page==4); samplesTab.setToggleState(page==4,juce::dontSendNotification);
    keyboard.setVisible(page==3);
    keyboardTab.setToggleState(page==3,juce::dontSendNotification);
    synthTab.setToggleState(page==0,juce::dontSendNotification);
    outputTab.setToggleState(page==1,juce::dontSendNotification);
    effectsTab.setToggleState(page==2,juce::dontSendNotification);
    for (auto& knob : globalKnobs) knob->setVisible(page==1);
    for (auto& knob : effectKnobs) knob->setVisible(page==2);
    echoTiming.setVisible(page==2); echoTimingInfo.setVisible(page==2);
    for (auto& knob : layerKnobs) knob->setVisible(page==0);
    for (auto& knob : operatorKnobs) knob->setVisible(page==0);
    for (auto* component : std::initializer_list<juce::Component*>{&layer,&op,&waveform,&envelope,&layerLabel,&operatorLabel,&envelopeAvailability})
        component->setVisible(page==0);
    resized(); repaint();
}
void CSynthEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff101722));
    float sx=getWidth()/1180.0f, sy=getHeight()/700.0f;
    g.addTransform(juce::AffineTransform::scale(sx,sy));
    g.setColour(juce::Colour(0xff47c8c0)); g.setFont(juce::FontOptions(27.0f).withStyle("Bold"));
    g.drawText("CSynth",20,17,200,35,juce::Justification::centredLeft);
    g.setColour(juce::Colour(0xffa9bbce)); g.setFont(juce::FontOptions(14.0f));
    g.drawText("libcsynth / Audio Unit",20,50,210,22,juce::Justification::centredLeft);
    if (currentPage==0) g.drawText("OPERATOR",20,259,160,25,juce::Justification::centredLeft);
    if (currentPage==1) {
        g.drawText("MASTER ADSR",20,130,500,22,juce::Justification::centredLeft);
        g.drawText("FILTERS / OUTPUT",20,295,500,22,juce::Justification::centredLeft);
        g.drawText("Master ADSR shapes volume. Filters, effects, and output gain follow the synth layers.",20,470,1140,25,juce::Justification::centredLeft);
    }
    if (currentPage==2) {
        g.drawText("ECHO",20,130,500,22,juce::Justification::centredLeft);
        g.drawText("REVERB",20,295,500,22,juce::Justification::centredLeft);
        g.drawText("Raise Echo mix or Reverb mix to hear the effect. Changes apply while playing.",20,470,1140,25,juce::Justification::centredLeft);
    }
    if (currentPage==3) {
        g.drawText("PLAY WITH THE MOUSE",20,130,500,22,juce::Justification::centredLeft);
        g.drawText("Click or drag across the keys to test your sound. MIDI from Logic works on every tab.",20,340,1140,25,juce::Justification::centredLeft);
    }
    g.setColour(juce::Colour(0xff29374a));
    for (int y : {78,120,540}) g.drawHorizontalLine(y,20,1160);
}
void CSynthEditor::resized()
{
    auto place=[this](juce::Component& c,int x,int y,int w,int h) {
        c.setBounds(juce::roundToInt(x*getWidth()/1180.0),juce::roundToInt(y*getHeight()/700.0),
                    juce::roundToInt(w*getWidth()/1180.0),juce::roundToInt(h*getHeight()/700.0));
    };
    place(presets,240,26,330,34); place(previous,580,26,36,34); place(next,622,26,36,34);
    place(import,675,26,140,34); place(exportSound,825,26,140,34); place(release,980,26,180,34);
    place(echoTiming,300,126,235,30); place(echoTimingInfo,545,126,615,30);
    place(synthTab,20,85,150,30); place(outputTab,180,85,180,30); place(effectsTab,370,85,150,30); place(keyboardTab,530,85,150,30);
    place(samplesTab,690,85,150,30); place(samplePanel,20,130,1140,365);
    place(keyboard,20,180,1140,140); keyboard.setKeyWidth(keyboard.getWidth()/36.0f);
    for (int i=0;i<4;++i) place(*globalKnobs[static_cast<std::size_t>(i)],20+i*285,160,265,120);
    for (int i=4;i<7;++i) place(*globalKnobs[static_cast<std::size_t>(i)],20+(i-4)*380,325,360,120);
    for (int i=0;i<6;++i) place(*effectKnobs[static_cast<std::size_t>(i)],20+(i%3)*380,160+(i/3)*165,360,120);
    place(layerLabel,20,128,85,30); place(layer,105,128,155,30);
    place(operatorLabel,300,128,100,30); place(op,405,128,155,30);
    for (int i=0;i<4;++i) place(*layerKnobs[static_cast<std::size_t>(i)],20+i*285,158,265,96);
    place(waveform,220,259,200,30); place(envelope,450,259,200,30);
    place(envelopeAvailability,665,259,495,30);
    for (int i=0;i<10;++i) place(*operatorKnobs[static_cast<std::size_t>(i)],20+(i%5)*228,300+(i/5)*104,215,104);
    place(status,20,510,1140,28); place(spectrogram,20,548,1140,140);
}
