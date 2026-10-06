#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "EchoTiming.h"
extern "C" {
#include "effects.h"
}
#include <cmath>
#include <iostream>
#include <stdexcept>

#define CHECK(c) do { if (!(c)) throw std::runtime_error(#c); } while(0)
static void set(CSynthProcessor& p, const char* id, float value)
{
    auto* parameter=p.state.getParameter(id); CHECK(parameter);
    parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
}
static double energy(const juce::AudioBuffer<float>& b, int start=0, int count=-1)
{
    if(count<0) count=b.getNumSamples()-start;
    double result=0;
    for(int i=start;i<start+count;++i) { double x=b.getSample(0,i); CHECK(std::isfinite(x)); result+=x*x; }
    return result;
}
struct TestPlayHead : juce::AudioPlayHead {
    PositionInfo position;
    juce::Optional<PositionInfo> getPosition() const override { return position; }
};
int main()
{
    juce::ScopedJuceInitialiser_GUI init;
    try {
        CHECK(std::abs(echoTiming::milliseconds(1,120,213)-62.5)<.001);
        CHECK(std::abs(echoTiming::milliseconds(8,120,213)-375)<.001);
        CHECK(std::abs(echoTiming::milliseconds(11,120,213)-750)<.001);
        CHECK(std::abs(echoTiming::milliseconds(12,120,213)-1000.0/3)<.001);
        CHECK(std::abs(echoTiming::milliseconds(17,60,213)-6000)<.001);
        CSynthProcessor timed;
        set(timed,"echoDelay",213); set(timed,"echoTiming",8);
        CHECK(!timed.hasEchoTempo()); CHECK(std::abs(timed.effectiveEchoDelayMs()-375)<.001);
        TestPlayHead host; host.position.setBpm(60); host.position.setTimeSignature(juce::AudioPlayHead::TimeSignature{6,8});
        timed.setPlayHead(&host); timed.prepareToPlay(48000,512);
        juce::AudioBuffer<float> silence(2,512); juce::MidiBuffer events;
        timed.processBlock(silence,events); CHECK(timed.hasEchoTempo());
        CHECK(std::abs(timed.effectiveEchoDelayMs()-750)<.001);
        host.position.setBpm(120); timed.processBlock(silence,events);
        CHECK(std::abs(timed.effectiveEchoDelayMs()-375)<.001);
        set(timed,"echoTiming",17); host.position.setBpm(60); timed.processBlock(silence,events);
        CHECK(std::abs(timed.effectiveEchoDelayMs()-6000)<.001);
        host.position.setBpm(10); timed.processBlock(silence,events);
        CHECK(std::abs(timed.effectiveEchoDelayMs()-30000)<.001);
        juce::MemoryBlock synced; timed.getStateInformation(synced);
        CSynthProcessor restored; restored.setStateInformation(synced.getData(),static_cast<int>(synced.getSize()));
        CHECK(juce::roundToInt(restored.state.getRawParameterValue("echoTiming")->load())==17);
        set(timed,"echoTiming",0); timed.processBlock(silence,events);
        CHECK(std::abs(timed.effectiveEchoDelayMs()-213)<.001);
        auto oldState=timed.state.copyState(); oldState.setProperty("schema",1,nullptr);
        oldState.removeChild(oldState.getChildWithProperty("id","echoTiming"),nullptr);
        juce::MemoryBlock legacy; juce::AudioProcessor::copyXmlToBinary(*oldState.createXml(),legacy);
        restored.setStateInformation(legacy.getData(),static_cast<int>(legacy.getSize()));
        CHECK(juce::roundToInt(restored.state.getRawParameterValue("echoTiming")->load())==0);
        SynthEffects longEcho{}; auto fx=synthDefaultConfig().effects;
        fx.echoMix=1; fx.echoFeedback=0; fx.reverbMix=0; fx.echoDelayMs=CSYNTH_ECHO_MAX_DELAY_MS;
        CHECK(effectsInit(&longEcho,1000,fx)==0);
        for (int i=0;i<=CSYNTH_ECHO_MAX_DELAY_MS;++i)
            CHECK(std::abs(effectsNext(&longEcho,i==0 ? 1.0f : 0.0f)-(i==0 || i==CSYNTH_ECHO_MAX_DELAY_MS ? 1.0f : 0.0f))<1.e-6f);
        effectsDestroy(&longEcho);
        timed.setPlayHead(nullptr); timed.releaseResources();
        SpectrumTap queue;
        std::array<float,512> captured{};
        captured.fill(.25f);
        queue.push(captured.data(),512); CHECK(queue.pop(captured.data(),512)==0);
        queue.enabled.store(true);
        for (int i=0;i<65;++i) queue.push(captured.data(),512);
        CHECK(queue.dropped.load());
        int queued=0;
        while (int count=queue.pop(captured.data(),512)) {
            queued+=count;
            for (int i=0;i<count;++i) CHECK(std::abs(captured[static_cast<std::size_t>(i)]-.25f)<1.e-9f);
        }
        CHECK(queued==32767);
        CSynthProcessor a,b;
        a.setCurrentProgram(1); b.setCurrentProgram(1);
        set(a,"attack",0); set(a,"release",0);
        a.prepareToPlay(48000,512); b.prepareToPlay(48000,512);
        CHECK(a.engineReady() && b.engineReady());
        juce::AudioBuffer<float> buffer(2,512); juce::MidiBuffer midi;
        midi.addEvent(juce::MidiMessage::noteOn(1,69,static_cast<juce::uint8>(127)),128);
        midi.addEvent(juce::MidiMessage::noteOff(1,69),384);
        a.spectrumTap.enabled.store(true);
        a.processBlock(buffer,midi);
        CHECK(a.spectrumTap.pop(captured.data(),512)==512);
        for (int i=0;i<512;++i) CHECK(std::abs(captured[static_cast<std::size_t>(i)]-buffer.getSample(0,i))<1.e-9f);
        a.spectrumTap.enabled.store(false);
        CHECK(energy(buffer,0,128)<=0 && energy(buffer,128,256)>.01 && energy(buffer,384,128)<=0);
        CHECK(midi.isEmpty());
        for(int i=0;i<512;++i) CHECK(std::abs(buffer.getSample(0,i)-buffer.getSample(1,i))<1.e-12f);
        b.processBlock(buffer,midi); CHECK(energy(buffer)<=0); // Independent instances.

        midi.addEvent(juce::MidiMessage::noteOn(1,69,static_cast<juce::uint8>(100)),0);
        midi.addEvent(juce::MidiMessage::controllerEvent(1,64,127),100);
        midi.addEvent(juce::MidiMessage::noteOff(1,69),200);
        a.processBlock(buffer,midi); CHECK(energy(buffer,250)>0);
        midi.addEvent(juce::MidiMessage::controllerEvent(1,64,0),0);
        a.processBlock(buffer,midi); CHECK(energy(buffer)<=0);
        midi.addEvent(juce::MidiMessage::noteOn(1,69,static_cast<juce::uint8>(100)),0);
        midi.addEvent(juce::MidiMessage::noteOn(2,69,static_cast<juce::uint8>(80)),0);
        midi.addEvent(juce::MidiMessage::noteOff(1,69),100);
        a.processBlock(buffer,midi); CHECK(energy(buffer,250)>0);
        midi.addEvent(juce::MidiMessage::controllerEvent(2,123,0),0);
        a.processBlock(buffer,midi); CHECK(energy(buffer)<=0);

        set(a,"lowpass",1200); set(a,"highpass",80); set(a,"l3_o5_ratio",3.14f);
        set(a,"echoMix",.2f); set(a,"gain",-6);
        juce::MemoryBlock state; a.getStateInformation(state);
        b.setStateInformation(state.getData(),static_cast<int>(state.getSize()));
        CHECK(std::abs(b.readConfig().filters.lowpassHz-1200)<1);
        CHECK(std::abs(b.readConfig().layers[3].fm.operators[5].ratio-3.14)<.001);
        CHECK(std::abs(b.state.getRawParameterValue("gain")->load()+6)<1.e-5f);
        const char bad[]="not a state"; b.setStateInformation(bad,sizeof(bad));
        CHECK(std::abs(b.state.getRawParameterValue("gain")->load()+6)<1.e-5f);
        for(int i=0;i<SYNTH_PRESET_COUNT;++i) {
            b.setCurrentProgram(i); auto config=b.readConfig(); CHECK(synthConfigValid(&config));
            midi.addEvent(juce::MidiMessage::noteOn(1,72,static_cast<juce::uint8>(100)),0);
            b.processBlock(buffer,midi); energy(buffer);
            midi.addEvent(juce::MidiMessage::noteOff(1,72),0); b.processBlock(buffer,midi);
        }
        auto editor=std::unique_ptr<juce::AudioProcessorEditor>(a.createEditor()); CHECK(editor);
        CHECK(a.spectrumTap.enabled.load());
        // Feed real output and fire the UI timers to exercise the FFT/scroll path.
        for (int block=0;block<16;++block) {
            if(block==0) midi.addEvent(juce::MidiMessage::noteOn(1,69,static_cast<juce::uint8>(100)),0);
            a.processBlock(buffer,midi);
            if(block%4==3) { juce::Thread::sleep(40); juce::Timer::callPendingTimersSynchronously(); }
        }
        auto snapshot=editor->createComponentSnapshot(editor->getLocalBounds());
        bool spectrumVisible=false;
        for (int y=590;y<665;++y)
            for (int x=1145;x<1159;++x)
                if (snapshot.getPixelAt(x,y).getGreen()>80) spectrumVisible=true;
        CHECK(spectrumVisible); // A4 produces a visible frequency band in the plot.
        if (auto* path=std::getenv("CSYNTH_EDITOR_CAPTURE")) {
            juce::FileOutputStream output{juce::File(path)}; CHECK(output.openedOk()); output.setPosition(0); output.truncate();
            CHECK(juce::PNGImageFormat().writeImageToStream(snapshot,output));
        }
        CHECK(editor->getHeight()==700);
        for (const auto& pageName : {"Output & filters","Effects","Keyboard","Synth"}) {
            bool selected=false;
            for (auto* child : editor->getChildren())
                if (auto* button=dynamic_cast<juce::TextButton*>(child))
                    if (button->getButtonText()==pageName) { button->onClick(); CHECK(button->getToggleState()); selected=true; }
            CHECK(selected);
            if (juce::String(pageName)=="Effects") {
                for (auto* child : editor->getChildren())
                    if (auto* combo=dynamic_cast<juce::ComboBox*>(child))
                        if (combo->getName()=="Echo timing") {
                            combo->setSelectedId(9,juce::sendNotificationSync);
                            CHECK(juce::roundToInt(a.state.getRawParameterValue("echoTiming")->load())==8);
                            for (auto* knob : editor->getChildren())
                                if (knob->getName()=="Echo delay")
                                    for (auto* control : knob->getChildren())
                                        if (auto* slider=dynamic_cast<juce::Slider*>(control)) CHECK(!slider->isEnabled());
                        }
                juce::Thread::sleep(80); juce::Timer::callPendingTimersSynchronously();
                if (auto* path=std::getenv("CSYNTH_EDITOR_CAPTURE")) {
                    auto image=editor->createComponentSnapshot(editor->getLocalBounds());
                    auto file=juce::File(path).getSiblingFile("csynth-effects.png");
                    juce::FileOutputStream output{file}; CHECK(output.openedOk()); output.setPosition(0); output.truncate();
                    CHECK(juce::PNGImageFormat().writeImageToStream(image,output));
                }
                set(a,"echoTiming",0);
            }
            if (juce::String(pageName)=="Keyboard") {
                a.keyboard.noteOn(1,72,.8f); a.processBlock(buffer,midi); CHECK(energy(buffer)>0);
                a.keyboard.noteOff(1,72,0); a.processBlock(buffer,midi);
                if (auto* path=std::getenv("CSYNTH_EDITOR_CAPTURE")) {
                    auto image=editor->createComponentSnapshot(editor->getLocalBounds());
                    juce::FileOutputStream output{juce::File(path).getSiblingFile("csynth-keyboard.png")};
                    CHECK(output.openedOk()); output.setPosition(0); output.truncate();
                    CHECK(juce::PNGImageFormat().writeImageToStream(image,output));
                }
            }
            for (auto* child : editor->getChildren()) {
                if (dynamic_cast<juce::MidiKeyboardComponent*>(child)!=nullptr) CHECK(child->isVisible()==(juce::String(pageName)=="Keyboard"));
                if (child->isVisible()) CHECK(editor->getLocalBounds().contains(child->getBounds()));
                if (child->getName()=="Low-pass") CHECK(child->isVisible()==(juce::String(pageName)=="Output & filters"));
                if (child->getName()=="Echo mix") CHECK(child->isVisible()==(juce::String(pageName)=="Effects"));
                if (child->getName()=="Ratio") CHECK(child->isVisible()==(juce::String(pageName)=="Synth"));
            }
        }
        editor.reset(); CHECK(!a.spectrumTap.enabled.load()); a.releaseResources(); b.releaseResources();
        std::cout << "MIDI offsets, sustain, channels, automation/state, 64 programs and editor passed.\n";
        return 0;
    } catch(const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
