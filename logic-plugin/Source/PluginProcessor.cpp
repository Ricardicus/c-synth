#include <cstring>
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "EchoTiming.h"
#include <cmath>
#include <limits>
#include <cerrno>
#include <cstdlib>

CSynthProcessor::CSynthProcessor()
    : AudioProcessor(BusesProperties().withOutput("Output",juce::AudioChannelSet::stereo(),true)),
      state(*this,nullptr,"CSynthState",parameters::layout())
{
    for (const auto& spec : parameters::specs()) values.push_back(state.getRawParameterValue(spec.id));
    lastValues.resize(values.size(),std::numeric_limits<float>::quiet_NaN());
    outputGain = state.getRawParameterValue("gain");
    echoTimingParameter = state.getRawParameterValue("echoTiming");
    freeEchoDelay = state.getRawParameterValue("echoDelay");
}
CSynthProcessor::~CSynthProcessor() { releaseResources(); }
std::vector<CSynthProcessor::SampleFile> CSynthProcessor::sampleFiles() const
{
    const juce::ScopedLock lock(getCallbackLock()); return draftSamples;
}
void CSynthProcessor::setSampleFiles(std::vector<SampleFile> files)
{
    if (files.size()>SYNTH_SAMPLE_MAX_ENTRIES) return;
    const juce::ScopedLock lock(getCallbackLock()); draftSamples=std::move(files);
}
juce::String CSynthProcessor::sampleLoadError() const
{
    const juce::ScopedLock lock(getCallbackLock()); return sampleError;
}
juce::Result CSynthProcessor::loadSampleFiles(const std::vector<SampleFile>& files, bool enabled)
{
    auto fail=[this](const juce::String& message) {
        const juce::ScopedLock lock(getCallbackLock()); sampleError=message;
        return juce::Result::fail(message);
    };
    if (files.empty() || files.size()>SYNTH_SAMPLE_MAX_ENTRIES) return fail("Add 1 to 128 recordings first.");
    std::unique_ptr<SynthSampleBank,decltype(&synthSampleBankDestroy)> bank{synthSampleBankCreate(),synthSampleBankDestroy};
    if (!bank) return fail("Could not allocate sample bank.");
    // Decoding and file access happen outside the render callback and its lock.
    for (std::size_t i=0;i<files.size();++i) {
        auto number=files[i].hz.trim(); char* end=nullptr; errno=0;
        double hz=std::strtod(number.toRawUTF8(),&end);
        if (number.isEmpty() || errno || !end || *end || !std::isfinite(hz) || hz<1)
            return fail("Row "+juce::String(static_cast<int>(i+1))+": enter a valid base frequency in Hz (at least 1).");
        char error[256]={};
        if (synthSampleBankAddFile(bank.get(),files[i].path.toRawUTF8(),hz,1,0,0,error,sizeof(error)))
            return fail("Row "+juce::String(static_cast<int>(i+1))+": "+juce::String::fromUTF8(error));
    }
    const juce::ScopedLock lock(getCallbackLock());
    if (engine && synthApplySampleBank(engine.get(),bank.get())) return fail("Could not apply sample bank.");
    if (engine) synthSetSourceMode(engine.get(),enabled ? SYNTH_SOURCE_SAMPLES : SYNTH_SOURCE_FM);
    sampleBank=std::move(bank); activeSamples=files;
    bankSize.store(static_cast<int>(files.size())); samplesEnabled.store(enabled); sampleError.clear();
    return juce::Result::ok();
}
juce::Result CSynthProcessor::applySampleFiles() { return loadSampleFiles(sampleFiles(),true); }
juce::Result CSynthProcessor::useSamples(bool enabled)
{
    const juce::ScopedLock lock(getCallbackLock());
    if (enabled && !sampleBank) return juce::Result::fail("Apply a sample bank first.");
    if (engine && synthSetSourceMode(engine.get(),enabled ? SYNTH_SOURCE_SAMPLES : SYNTH_SOURCE_FM))
        return juce::Result::fail("Could not switch source.");
    samplesEnabled.store(enabled); return juce::Result::ok();
}
SynthConfig CSynthProcessor::readConfig() const
{
    auto config = synthDefaultConfig();
    const auto& specs = parameters::specs();
    for (std::size_t i=0;i<specs.size();++i) specs[i].write(config,values[i]->load());
    return config;
}
void CSynthProcessor::applyConfig(const SynthConfig& config, int factoryIndex)
{
    if (!synthConfigValid(&config)) return;
    for (const auto& s : parameters::specs()) {
        auto* param = state.getParameter(s.id);
        param->beginChangeGesture();
        param->setValueNotifyingHost(param->convertTo0to1(juce::jlimit(s.minimum,s.maximum,static_cast<float>(s.read(config)))));
        param->endChangeGesture();
    }
    auto* timing=state.getParameter("echoTiming");
    timing->beginChangeGesture(); timing->setValueNotifyingHost(0); timing->endChangeGesture();
    selectedPreset.store(factoryIndex);
}
void CSynthProcessor::setCurrentProgram(int index)
{
    if (index>=0 && index<SYNTH_PRESET_COUNT) { useSamples(false); applyConfig(synthPresetConfig(index),index); }
}
void CSynthProcessor::clearMidi()
{
    down={}; latched={}; pedal={}; velocities={}; soundingVelocity={};
}
void CSynthProcessor::prepareToPlay(double rate, int)
{
    const juce::ScopedLock lock(getCallbackLock());
    clearMidi();
    spectrumTap.sampleRate.store(rate,std::memory_order_relaxed);
    auto config=readConfig();
    config.effects.echoDelayMs=effectiveEchoDelayMs();
    engine.reset(synthCreate(juce::roundToInt(rate),&config));
    if (engine && sampleBank) {
        synthApplySampleBank(engine.get(),sampleBank.get());
        synthSetSourceMode(engine.get(),samplesEnabled.load() ? SYNTH_SOURCE_SAMPLES : SYNTH_SOURCE_FM);
    }
    appliedEchoDelay=-1;
    ready.store(engine!=nullptr);
    std::fill(lastValues.begin(),lastValues.end(),std::numeric_limits<float>::quiet_NaN());
    gain.reset(rate,.02);
    gain.setCurrentAndTargetValue(juce::Decibels::decibelsToGain(outputGain->load()));
}
void CSynthProcessor::releaseResources()
{
    const juce::ScopedLock lock(getCallbackLock());
    ready.store(false); engine.reset(); clearMidi();
}
void CSynthProcessor::reset()
{
    const juce::ScopedLock lock(getCallbackLock());
    for (int note=0;note<128;++note) synthMidiNoteOff(engine.get(),note);
    clearMidi();
}
bool CSynthProcessor::isBusesLayoutSupported(const BusesLayout& layout) const
{
    return layout.getMainInputChannelSet().isDisabled() &&
        (layout.getMainOutputChannelSet()==juce::AudioChannelSet::mono() ||
         layout.getMainOutputChannelSet()==juce::AudioChannelSet::stereo());
}
void CSynthProcessor::updatePatch()
{
    bool changed=false;
    for (std::size_t i=0;i<values.size();++i) {
        float value=values[i]->load();
        if (std::memcmp(&lastValues[i], &value, sizeof(value))!=0) { changed=true; lastValues[i]=value; }
    }
    double delay=effectiveEchoDelayMs();
    if (std::abs(appliedEchoDelay-delay)>.0001) changed=true;
    if (changed && engine) {
        auto config=readConfig(); config.effects.echoDelayMs=delay;
        synthConfigure(engine.get(),&config); appliedEchoDelay=delay;
    }
    gain.setTargetValue(juce::Decibels::decibelsToGain(outputGain->load()));
}
void CSynthProcessor::syncNote(int note)
{
    int velocity=0;
    for (int channel=0;channel<16;++channel)
        if (down[static_cast<std::size_t>(channel)][static_cast<std::size_t>(note)] ||
            latched[static_cast<std::size_t>(channel)][static_cast<std::size_t>(note)])
            velocity=juce::jmax(velocity,velocities[static_cast<std::size_t>(channel)][static_cast<std::size_t>(note)]);
    auto index=static_cast<std::size_t>(note);
    if (velocity==soundingVelocity[index]) return;
    soundingVelocity[index]=velocity;
    if (velocity) synthMidiNoteOn(engine.get(),note,velocity);
    else synthMidiNoteOff(engine.get(),note);
}
void CSynthProcessor::handleMidi(const juce::MidiMessage& message)
{
    int channel=message.getChannel()-1;
    if (channel<0 || channel>=16) return;
    auto c=static_cast<std::size_t>(channel);
    if (message.isNoteOn()) {
        int note=message.getNoteNumber(); auto n=static_cast<std::size_t>(note);
        down[c][n]=true; latched[c][n]=false; velocities[c][n]=message.getVelocity(); syncNote(note);
    } else if (message.isNoteOff()) {
        int note=message.getNoteNumber(); auto n=static_cast<std::size_t>(note);
        if (down[c][n]) latched[c][n]=pedal[c];
        down[c][n]=false; syncNote(note);
    } else if (message.isController()) {
        int controller=message.getControllerNumber();
        if (controller==64) {
            pedal[c]=message.getControllerValue()>=64;
            if (!pedal[c]) for (int note=0;note<128;++note) { latched[c][static_cast<std::size_t>(note)]=false; syncNote(note); }
        } else if (controller==120 || controller==123) {
            for (int note=0;note<128;++note) {
                auto n=static_cast<std::size_t>(note);
                latched[c][n]=controller==123 && pedal[c] && (down[c][n] || latched[c][n]);
                down[c][n]=false; syncNote(note);
            }
        } else if (controller==121) {
            pedal[c]=false;
            for (int note=0;note<128;++note) { latched[c][static_cast<std::size_t>(note)]=false; syncNote(note); }
        }
    }
}
void CSynthProcessor::render(juce::AudioBuffer<float>& buffer, int offset, int count)
{
    if (count<=0) return;
    auto* mono=buffer.getWritePointer(0,offset);
    synthRender(engine.get(),mono,static_cast<std::size_t>(count));
    for (int i=0;i<count;++i) mono[i]*=gain.getNextValue();
    for (int channel=1;channel<buffer.getNumChannels();++channel)
        buffer.copyFrom(channel,offset,buffer,0,offset,count);
}
void CSynthProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    const juce::ScopedLock lock(getCallbackLock());
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();
    if (!engine || buffer.getNumChannels()==0) return;
    if (releaseRequested.exchange(false)) { reset(); keyboard.reset(); }
    if (auto* playHead=getPlayHead())
        if (auto position=playHead->getPosition())
            if (auto bpm=position->getBpm())
                if (std::isfinite(*bpm) && *bpm>0) { tempo.store(*bpm); tempoReceived.store(true); }
    updatePatch();
    keyboard.processNextMidiBuffer(midi,0,buffer.getNumSamples(),true);
    int position=0;
    for (const auto metadata : midi) {
        int eventPosition=juce::jlimit(position,buffer.getNumSamples(),metadata.samplePosition);
        render(buffer,position,eventPosition-position);
        position=eventPosition;
        handleMidi(metadata.getMessage());
    }
    render(buffer,position,buffer.getNumSamples()-position);
    spectrumTap.push(buffer.getReadPointer(0),buffer.getNumSamples());
    midi.clear();
}
double CSynthProcessor::effectiveEchoDelayMs() const
{
    return juce::jlimit(1.0,static_cast<double>(CSYNTH_ECHO_MAX_DELAY_MS),
        echoTiming::milliseconds(juce::roundToInt(echoTimingParameter->load()),tempo.load(),
                                 freeEchoDelay->load()));
}
double CSynthProcessor::getTailLengthSeconds() const
{
    auto sound=readConfig();
    auto repeats=[](double feedback) { return feedback<=0 ? 1.0 : std::ceil(std::log(.001)/std::log(feedback)); };
    double echo=sound.effects.echoMix>0 ? effectiveEchoDelayMs()*.001*repeats(sound.effects.echoFeedback) : 0;
    double reverb=sound.effects.reverbMix>0 ? .054*repeats(sound.effects.reverbRoom) : 0;
    return sound.outputEnvelope.releaseMs*.001+echo+reverb;
}
void CSynthProcessor::getStateInformation(juce::MemoryBlock& block)
{
    auto tree=state.copyState();
    tree.setProperty("schema",1,nullptr);
    tree.setProperty("factory",selectedPreset.load(),nullptr);
    const juce::ScopedLock lock(getCallbackLock());
    tree.removeChild(tree.getChildWithName("SampleInstrument"),nullptr);
    juce::ValueTree instrument{"SampleInstrument"};
    instrument.setProperty("enabled",samplesEnabled.load(),nullptr);
    auto append=[&instrument](const char* name,const std::vector<SampleFile>& files) {
        juce::ValueTree list{name};
        for (const auto& file : files) {
            juce::ValueTree row{"Sample"}; row.setProperty("path",file.path,nullptr); row.setProperty("hz",file.hz,nullptr);
            list.addChild(row,-1,nullptr);
        }
        instrument.addChild(list,-1,nullptr);
    };
    append("Draft",draftSamples); append("Active",activeSamples); tree.addChild(instrument,-1,nullptr);
    if (auto xml=tree.createXml()) copyXmlToBinary(*xml,block);
}
void CSynthProcessor::setStateInformation(const void* data, int size)
{
    if (size<=0 || !data) return;
    auto xml=getXmlFromBinary(data,size);
    if (!xml || !xml->hasTagName("CSynthState")) return;
    auto tree=juce::ValueTree::fromXml(*xml);
    if (static_cast<int>(tree.getProperty("schema",0))!=1) return;
    int preset=static_cast<int>(tree.getProperty("factory",-1));
    selectedPreset.store(preset>=0 && preset<SYNTH_PRESET_COUNT ? preset : -1);
    if (!tree.getChildWithProperty("id","echoTiming").isValid()) {
        juce::ValueTree timing{"PARAM"}; timing.setProperty("id","echoTiming",nullptr);
        timing.setProperty("value",0,nullptr); tree.addChild(timing,-1,nullptr);
    }
    auto instrument=tree.getChildWithName("SampleInstrument");
    auto read=[](juce::ValueTree list) {
        std::vector<SampleFile> files;
        for (auto row : list) {
            if (files.size()==SYNTH_SAMPLE_MAX_ENTRIES) break;
            files.push_back({row.getProperty("path").toString(),row.getProperty("hz","440.00").toString()});
        }
        return files;
    };
    auto draft=read(instrument.getChildWithName("Draft"));
    auto active=read(instrument.getChildWithName("Active"));
    useSamples(false);
    {
        const juce::ScopedLock lock(getCallbackLock());
        if (engine) synthApplySampleBank(engine.get(),nullptr);
        sampleBank.reset(); activeSamples.clear(); bankSize.store(0); sampleError.clear();
    }
    setSampleFiles(std::move(draft));
    if (!active.empty()) loadSampleFiles(active,static_cast<bool>(instrument.getProperty("enabled",false)));
    state.replaceState(tree);
}
juce::AudioProcessorEditor* CSynthProcessor::createEditor() { return new CSynthEditor(*this); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new CSynthProcessor(); }
