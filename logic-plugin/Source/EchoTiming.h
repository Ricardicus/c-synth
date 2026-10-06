#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <cmath>

namespace echoTiming {
inline juce::StringArray choices()
{
    juce::StringArray result{"Free (ms)"};
    for (int divisor : {32,16,8,4,2,1}) {
        auto note="1/"+juce::String(divisor);
        result.add(note); result.add(note+" dotted"); result.add(note+" triplet");
    }
    return result;
}
inline double milliseconds(int selection, double bpm, double freeMs)
{
    if (selection<=0) return freeMs;
    selection=juce::jlimit(1,18,selection)-1;
    const int divisors[]={32,16,8,4,2,1};
    double quarters=4.0/divisors[selection/3];
    if (selection%3==1) quarters*=1.5;
    if (selection%3==2) quarters*=2.0/3.0;
    if (!std::isfinite(bpm) || bpm<=0) bpm=120;
    return 60000.0/bpm*quarters;
}
}
