//
//  JuceMidiFilePlayer.h
//  JuceAbletonMidiFilePlayer
//
//  Created by Andrew Robertson on 31/07/2014.
//
//

#ifndef __JuceAbletonMidiFilePlayer__JuceMidiFilePlayer__
#define __JuceAbletonMidiFilePlayer__JuceMidiFilePlayer__

#include <iostream>
#include "../JuceLibraryCode/JuceHeader.h"

#include "JuceSequenceLoopPlayer.h"

struct AbletonBeat{
public:
    int index;//
    float tempo;//in millis per beat period
    unsigned long systemTime;//as sent by ableton but truncated to int - not brilliant that aspect but should still work okay
    float ticks;//in MIDI ticks - depends on the tempo of course
    int millis;//internal millis counter
};

class JuceMidiFilePlayer : private HighResolutionTimer {
public:
    JuceMidiFilePlayer();
    ~JuceMidiFilePlayer();
    
    MidiFile midiFile;
    
    MidiMessageSequence loadMidiFile(String fileLocation, bool mergeOn);
    void startMidiPlayback();
    void stopMidiPlayback();

    void newBeat(int beatIndex, float tempoMillis, int latency);
    
    void reverseSequence(MidiMessageSequence& sequence, int startStamp, int endStamp);
    
    JuceSequenceLoopPlayer looper;
    
    JuceSequenceLoopPlayer prophet;
    
    ScopedPointer<MidiOutput> midiDevice;
    
    std::vector<AbletonBeat> beatsReceived;
    
private:
    
    float playbackSpeed;
    //timer to play file - called every millisecond
    void hiResTimerCallback() override{
        updateMidiPlayPosition();
    }

    float millisToTicks(int millis);
    
    int ppq;
    int beatMillisCounter;//millis counter when on beat
    int beatTick;
    
    
    int millisCounter;
    int lastAltBeatTimeMillis;
    float lastAltBeatTimeTicks;
    float beatPeriod;
    
    int midiPlayIndex;
    void reset();
    
    unsigned long systemTime();
    void alternativeBeatCall(int& beatIndex, float& tempoMillis, int& latency);
    
    void setTempo(float tempoMillis);
    
    void updatePlaybackToBeat(int beatIndex);
    void updateMidiPlayPositionToTickPosition(double position);//for both sequences
    void updateMidiPlayPosition();
    void updateMidiPlayPositionToTickPosition(MidiMessageSequence& sequence, float tickPosition);

    MidiMessageSequence trackSequence;//pointer to track we load in
    MidiMessageSequence::MidiEventHolder* playEvent;//pointer to an individual midi event
    
    //MidiMessageSequence loopSequence, mutatedSequence;//loop what we play, mutate for the transformed version

    void printSequenceEvents(const MidiMessageSequence& sequence);//to see what is in sequences
    
    void bufferTest();//not needed

};
#endif /* defined(__JuceAbletonMidiFilePlayer__JuceMidiFilePlayer__) */
