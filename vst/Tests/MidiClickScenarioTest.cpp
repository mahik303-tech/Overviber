#include "TestSynth.h"
#include "MidiDispatcher.h"
#include <juce_audio_basics/juce_audio_basics.h>
#include <fstream>
#include <iostream>
#include <filesystem>
#include <cstring>
#include <cstdlib>

static void word(std::ostream& out, uint32_t value, int bytes=4) {
    for (int i=0;i<bytes;++i) out.put(static_cast<char>(value >> (8*i)));
}
static void saveWav(const std::string& path, const std::vector<float>& samples, int rate) {
    std::ofstream out(path,std::ios::binary); const auto bytes=static_cast<uint32_t>(samples.size()*4);
    out.write("RIFF",4); word(out,36+bytes); out.write("WAVEfmt ",8);
    word(out,16); word(out,3,2); word(out,2,2); word(out,rate); word(out,rate*8); word(out,8,2); word(out,32,2);
    out.write("data",4); word(out,bytes);
    for (float sample:samples) { uint32_t bits; std::memcpy(&bits,&sample,4); word(out,bits); }
    if (!out) throw std::runtime_error("Cannot save render");
}
int main(int argc,char** argv) {
    std::cout << std::unitbuf;
    // Isolate voice retirement from the oscillator waveform: a constant signal
    // must decay through the VCA even when the digital ADSR has already ended.
    for (float rate : {44100.f, 48000.f, 96000.f}) {
        Voice voice; voice.setSampleRate(rate);
        std::array<uint16_t, WTOSC_SAMPLE_COUNT> wave; wave.fill(65535);
        voice.setOscSampleData(wave.data(),wave.data(),wave.data(),wave.data());
        voice.getAmpEnv().setCVs(0,0,65535,0,65535,0x1f);
        voice.gateOn(60,65535,0);
        float previous=0, retirementJump=0;
        for(int i=0;i<static_cast<int>(rate/10);++i) {
            if(i==static_cast<int>(rate/20)) voice.gateOff();
            const bool wasActive=voice.isActive();
            voice.updateEnvelopes();
            voice.updateVoiceCVs(60*WTOSC_CV_SEMITONE,60*WTOSC_CV_SEMITONE,
                wmOff,0,wmOff,0,65535,0,voice.getAmpEnv().getOutput(),0.2f,0,0,false);
            const float sample=voice.processSample(static_cast<uint32_t>(SYNTH_MASTER_CLOCK/rate));
            if(wasActive && !voice.isActive()) retirementJump=std::abs(sample-previous);
            previous=sample;
        }
        std::cout << "Release retirement " << rate << " Hz: " << retirementJump << ", residual gain " << voice.getVca().getCurrentGain() << '\n';
        if(voice.isActive() || retirementJump>0.00001f || voice.getVca().getCurrentGain()>0.000011f) return 1;
    }
    const juce::File input(argc>1 ? argv[1] : OVERVIBER_MIDI_FIXTURE);
    auto stream=input.createInputStream(); juce::MidiFile file;
    if (!stream || !file.readFrom(*stream)) return 1;
    file.convertTimestampTicksToSeconds(); juce::MidiMessageSequence sequence;
    for (int t=0;t<file.getNumTracks();++t) sequence.addSequence(*file.getTrack(t),0);
    int notes=0; for (int i=0;i<sequence.getNumEvents();++i) notes += sequence.getEventPointer(i)->message.isNoteOn();
    std::cout << "Tracks " << file.getNumTracks() << ", note-ons " << notes << ", seconds " << sequence.getEndTime() << '\n';
    if(argc>2) {
        std::ofstream events(std::string(argv[2])+"-events.csv");
        events << "seconds,event,channel,note,velocity\n";
        for(int i=0;i<sequence.getNumEvents();++i) {
            const auto& m=sequence.getEventPointer(i)->message;
            if(m.isNoteOnOrOff()) events << m.getTimeStamp() << ',' << (m.isNoteOn()?"on":"off") << ',' << m.getChannel() << ',' << m.getNoteNumber() << ',' << int(m.getVelocity()) << '\n';
        }
    }
    if (!notes || sequence.getEndTime()>600) return 1;
    const int presetNumber = argc > 3 ? std::atoi(argv[3]) : 23;
    const bool arpHoldTest = argc > 4 && std::string(argv[4]) == "arp-hold-100";
    const int rate=44100, frames=static_cast<int>(std::ceil((sequence.getEndTime()+0.5)*rate));
    const auto run=[&](int block) {
        auto engine=std::make_unique<TestSynth>();
        engine->getWaveManager().setBaseDirectory(std::string(OVERVIBER_TEST_DATA_DIR)+"/WAVEDATA");
        engine->getPresetManager().setBaseDirectory(std::string(OVERVIBER_TEST_DATA_DIR)+"/PRESETS");
        engine->prepare(rate);
        int preset=-1;
        for(int i=0;i<engine->getPresetManager().getPresetCount();++i)
            if(engine->getPresetManager().getPresetNumber(i)==presetNumber) preset=i;
        if(preset<0) throw std::runtime_error("Missing requested preset");
        engine->loadPreset(preset);
        engine->setSteppedParam(spEngineMode,emMultiChannel);
        if (arpHoldTest) {
            engine->setHostSyncEnabled(false);
            engine->setSteppedParam(spArpMode, amUp);
            engine->setSteppedParam(spArpHold, 1);
            engine->getArpeggiator().setGateLength(1.0f);
        }
        std::vector<float> result(frames*2); float l[512],r[512]; int event=0; bool holdReleased=false;
        for(int offset=0;offset<frames;) {
            while(event<sequence.getNumEvents() && std::llround(sequence.getEventTime(event)*rate)<=offset) {
                const auto& msg=sequence.getEventPointer(event++)->message;
                // The plugin's MIDI input path; tempo from the file's meta events.
                if(msg.isTempoMetaEvent()) engine->setHostBpm(static_cast<float>(60.0/msg.getTempoSecondsPerQuarterNote()));
                else mididispatch::dispatch(*engine,msg.getRawData(),msg.getRawDataSize());
            }
            if (arpHoldTest && !holdReleased && event == sequence.getNumEvents()) {
                engine->setSteppedParam(spArpHold, 0);
                holdReleased = true;
            }
            int count=std::min(block,frames-offset);
            if(event<sequence.getNumEvents()) count=std::min(count,static_cast<int>(std::llround(sequence.getEventTime(event)*rate))-offset);
            engine->renderBlock(l,r,count);
            for(int i=0;i<count;++i) {result[2*(offset+i)]=l[i];result[2*(offset+i)+1]=r[i];}
            offset+=count;
        }
        return result;
    };
    {
        const auto samples=run(512); const auto small=run(64);
        float peak=0,jump=0,error=0; int jumpFrame=0;
        for(int i=0;i<frames*2;++i) {
            if(!std::isfinite(samples[i])) return 1;
            peak=std::max(peak,std::abs(samples[i])); error=std::max(error,std::abs(samples[i]-small[i]));
            if(i>=2 && std::abs(samples[i]-samples[i-2])>jump) {jump=std::abs(samples[i]-samples[i-2]);jumpFrame=i/2;}
        }
        std::cout << "Peak " << peak << ", largest step " << jump << " at " << double(jumpFrame)/rate << " s; block error " << error << '\n';
        if(error>1e-6f) return 1;
        // This fixture previously produced .125 / .043 steps at zero-release
        // note-offs. Keep an audible regression bound as well as block invariance.
        if(!arpHoldTest && jump>0.030f) return 1;
        if(argc>2) saveWav(std::string(argv[2])+".wav",samples,rate);
    }
    return 0;
}
