#include <AudioToolbox/AudioToolbox.h>
#include <CoreFoundation/CoreFoundation.h>
#include <array>
#include <cmath>
#include <iostream>
#include <stdexcept>
#define CHECK(c) do { if (!(c)) throw std::runtime_error(#c); } while(0)
static void check(OSStatus status,const char* operation)
{
    if(status!=noErr) throw std::runtime_error(std::string(operation)+": "+std::to_string(status));
}
int main(int argc,char** argv)
{
    CFBundleRef bundle=nullptr; AudioUnit unit=nullptr;
    try {
        CHECK(argc==2);
        auto url=CFURLCreateFromFileSystemRepresentation(nullptr,reinterpret_cast<const UInt8*>(argv[1]),
            static_cast<CFIndex>(std::char_traits<char>::length(argv[1])),true);
        bundle=CFBundleCreate(nullptr,url); CFRelease(url);
        CHECK(bundle && CFBundleLoadExecutable(bundle));
        auto factory=reinterpret_cast<AudioComponentFactoryFunction>(CFBundleGetFunctionPointerForName(bundle,CFSTR("CSynthAUFactory")));
        CHECK(factory);
        AudioComponentDescription description{'aumu','Csyn','Rica',0,0};
        auto component=AudioComponentRegister(&description,CFSTR("Ricardicus: CSynth Test"),0x00010000,factory);
        CHECK(component); check(AudioComponentInstanceNew(component,&unit),"instantiate");
        AudioStreamBasicDescription format{};
        format.mSampleRate=48000; format.mFormatID=kAudioFormatLinearPCM;
        format.mFormatFlags=kAudioFormatFlagIsFloat|kAudioFormatFlagIsPacked|kAudioFormatFlagIsNonInterleaved;
        format.mBytesPerPacket=4; format.mFramesPerPacket=1; format.mBytesPerFrame=4;
        format.mChannelsPerFrame=2; format.mBitsPerChannel=32;
        check(AudioUnitSetProperty(unit,kAudioUnitProperty_StreamFormat,kAudioUnitScope_Output,0,&format,sizeof(format)),"format");
        AUPreset preset{1,CFSTR("Pure sine")};
        check(AudioUnitSetProperty(unit,kAudioUnitProperty_PresentPreset,kAudioUnitScope_Global,0,&preset,sizeof(preset)),"factory preset");
        check(AudioUnitInitialize(unit),"initialize");
        check(MusicDeviceMIDIEvent(unit,0x90,69,127,128),"MIDI note-on");
        std::array<float,512> left{},right{};
        struct Buffers { UInt32 count; AudioBuffer buffers[2]; } output{2,{{1,sizeof(left),left.data()},{1,sizeof(right),right.data()}}};
        AudioTimeStamp time{}; time.mFlags=kAudioTimeStampSampleTimeValid;
        AudioUnitRenderActionFlags flags=0;
        check(AudioUnitRender(unit,&flags,&time,0,512,reinterpret_cast<AudioBufferList*>(&output)),"render");
        double energy=0;
        for(std::size_t i=0;i<left.size();++i) {
            CHECK(std::isfinite(left[i]) && left[i]==right[i]);
            if(i<128) CHECK(left[i]==0);
            energy+=left[i]*left[i];
        }
        CHECK(energy>.01);
        CFPropertyListRef state=nullptr; UInt32 size=sizeof(state);
        check(AudioUnitGetProperty(unit,kAudioUnitProperty_ClassInfo,kAudioUnitScope_Global,0,&state,&size),"get state");
        CHECK(state); check(AudioUnitSetProperty(unit,kAudioUnitProperty_ClassInfo,kAudioUnitScope_Global,0,&state,size),"restore state"); CFRelease(state);
        check(MusicDeviceMIDIEvent(unit,0x80,69,0,0),"MIDI note-off");
        time.mSampleTime=512; flags=0;
        check(AudioUnitRender(unit,&flags,&time,0,512,reinterpret_cast<AudioBufferList*>(&output)),"release render");
        AudioUnitUninitialize(unit); AudioComponentInstanceDispose(unit); unit=nullptr;
        CFRelease(bundle); bundle=nullptr;
        std::cout << "Built Audio Unit loaded, instantiated, rendered offset MIDI and restored state.\n";
        return 0;
    } catch(const std::exception& error) {
        std::cerr << error.what() << '\n';
        if(unit) AudioComponentInstanceDispose(unit);
        if(bundle) CFRelease(bundle);
        return 1;
    }
}
