#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <atomic>
#include <cmath>

// One audio-thread producer, one editor-thread consumer. A full queue drops
// visualization samples rather than waiting or changing the audible output.
class SpectrumTap {
public:
    void push(const float* samples, int count)
    {
        if (!enabled.load(std::memory_order_relaxed)) return;
        int a, na, b, nb;
        fifo.prepareToWrite(count,a,na,b,nb);
        std::copy_n(samples,na,data.begin()+a);
        std::copy_n(samples+na,nb,data.begin()+b);
        fifo.finishedWrite(na+nb);
        if (na+nb<count) dropped.store(true,std::memory_order_relaxed);
    }
    int pop(float* samples, int count)
    {
        int a, na, b, nb;
        fifo.prepareToRead(count,a,na,b,nb);
        std::copy_n(data.begin()+a,na,samples);
        std::copy_n(data.begin()+b,nb,samples+na);
        fifo.finishedRead(na+nb);
        return na+nb;
    }
    std::atomic<bool> enabled{false}, dropped{false};
    std::atomic<double> sampleRate{48000};
private:
    std::array<float,32768> data{};
    juce::AbstractFifo fifo{static_cast<int>(data.size())};
};

class OutputSpectrogram final : public juce::Component, private juce::Timer {
public:
    explicit OutputSpectrogram(SpectrumTap& source) : tap(source)
    {
        while (tap.pop(incoming.data(),static_cast<int>(incoming.size()))>0) {}
        tap.enabled.store(true,std::memory_order_relaxed);
        startTimerHz(30);
    }
    ~OutputSpectrogram() override
    {
        stopTimer(); tap.enabled.store(false,std::memory_order_relaxed);
    }
    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colour(0xff101722));
        g.setFont(juce::FontOptions(13.0f)); g.setColour(juce::Colour(0xffa9bbce));
        g.drawText("LIVE OUTPUT SPECTROGRAM",0,0,260,22,juce::Justification::centredLeft);
        g.drawText("Frequency / Hz   |   brightness = level (-90 to 0 dBFS)",260,0,getWidth()-260,22,juce::Justification::centredRight);
        auto plot=getLocalBounds().withTrimmedTop(25).withTrimmedLeft(48).withTrimmedBottom(18);
        if (plot.isEmpty()) return;
        // Oldest column is at nextColumn; paint two slices to scroll without
        // copying the entire history image for every FFT.
        float split=plot.getWidth()*static_cast<float>(history.getWidth()-nextColumn)/history.getWidth();
        g.drawImage(history,plot.getX(),plot.getY(),juce::roundToInt(split),plot.getHeight(),
                    nextColumn,0,history.getWidth()-nextColumn,history.getHeight());
        if (nextColumn>0) g.drawImage(history,plot.getX()+juce::roundToInt(split),plot.getY(),
                    plot.getWidth()-juce::roundToInt(split),plot.getHeight(),0,0,nextColumn,history.getHeight());
        for (double hz : {40.0,100.0,1000.0,10000.0}) {
            if (hz>upperFrequency()) continue;
            int y=plot.getBottom()-juce::roundToInt(std::log(hz/40.0)/std::log(upperFrequency()/40.0)*plot.getHeight());
            g.setColour(juce::Colour(0xffa9bbce));
            g.drawText(hz>=1000 ? juce::String(hz/1000,0)+"k" : juce::String(hz,0),0,y-7,42,14,juce::Justification::centredRight);
            g.setColour(juce::Colours::white.withAlpha(.09f)); g.drawHorizontalLine(juce::jlimit(plot.getY(),plot.getBottom()-1,y),static_cast<float>(plot.getX()),static_cast<float>(plot.getRight()));
        }
        g.setColour(juce::Colour(0xffa9bbce));
        double seconds=history.getWidth()*hop/rate;
        g.drawText(juce::String(seconds,1)+" seconds ago",plot.getX(),plot.getBottom(),160,18,juce::Justification::centredLeft);
        g.drawText("Now",plot.getRight()-70,plot.getBottom(),70,18,juce::Justification::centredRight);
    }
private:
    static constexpr int size=2048, hop=size/2;
    double upperFrequency() const { return juce::jmax(41.0,juce::jmin(20000.0,rate*.5)); }
    void timerCallback() override
    {
        double newRate=tap.sampleRate.load(std::memory_order_relaxed);
        if (std::abs(newRate-rate)>.1) {
            rate=newRate; filled=0; nextColumn=0;
            history.clear(history.getBounds(),juce::Colour(0xff080e18));
        }
        if (tap.dropped.exchange(false,std::memory_order_relaxed)) {
            // Start a fresh window after a stall; never join discontinuous data.
            for (int chunk=0;chunk<2;++chunk) tap.pop(incoming.data(),static_cast<int>(incoming.size()));
            filled=0;
        }
        int count=tap.pop(incoming.data(),static_cast<int>(incoming.size()));
        for (int i=0;i<count;++i) {
            frame[static_cast<std::size_t>(filled++)]=incoming[static_cast<std::size_t>(i)];
            if (filled==size) {
                appendColumn();
                std::copy(frame.begin()+hop,frame.end(),frame.begin()); filled=size-hop;
            }
        }
        if (count>0) repaint();
    }
    void appendColumn()
    {
        std::fill(fftData.begin(),fftData.end(),0.0f);
        std::copy(frame.begin(),frame.end(),fftData.begin());
        window.multiplyWithWindowingTable(fftData.data(),size);
        fft.performFrequencyOnlyForwardTransform(fftData.data(),true);
        juce::Image::BitmapData pixels(history,juce::Image::BitmapData::writeOnly);
        for (int y=0;y<history.getHeight();++y) {
            auto frequency=[this](double row) { return 40.0*std::pow(upperFrequency()/40.0,1.0-row/history.getHeight()); };
            int first=juce::jlimit(1,size/2,static_cast<int>(std::floor(frequency(y+1)*size/rate)));
            int last=juce::jlimit(first,size/2,static_cast<int>(std::ceil(frequency(y)*size/rate)));
            float magnitude=0;
            for (int bin=first;bin<=last;++bin) magnitude=juce::jmax(magnitude,fftData[static_cast<std::size_t>(bin)]);
            float level=juce::jlimit(0.0f,1.0f,(juce::Decibels::gainToDecibels(magnitude*(4.0f/size),-90.0f)+90.0f)/90.0f);
            auto colour=level<.5f ? juce::Colour(0xff080e18).interpolatedWith(juce::Colour(0xff218da6),level*2)
                                  : juce::Colour(0xff218da6).interpolatedWith(juce::Colour(0xffffde82),(level-.5f)*2);
            pixels.setPixelColour(nextColumn,y,colour);
        }
        nextColumn=(nextColumn+1)%history.getWidth();
    }
    SpectrumTap& tap;
    juce::dsp::FFT fft{11};
    juce::dsp::WindowingFunction<float> window{size,juce::dsp::WindowingFunction<float>::hann,false};
    juce::Image history{juce::Image::RGB,512,128,true};
    std::array<float,size> frame{};
    std::array<float,size*2> fftData{};
    std::array<float,16384> incoming{};
    int filled=0, nextColumn=0;
    double rate=48000;
};
