//
//  JuceSequenceLoopPlayer.cpp
//  JuceAbletonMidiFilePlayer
//
//  Created by Andrew Robertson on 31/07/2014.
//
//

#include "JuceSequenceLoopPlayer.h"

//each looper should receive the beats from ableton and the counter
//given the loop points, and the start time (zero?)
//it can figure out where it is in the loop

//buffer midi out events so that note offs are scheduled ahead of time
//straight from the tick counter

//receive beat index, tempo, millisCounter

//millis counter will help us schedule abny note off

//LOOK at the clock
//should be neater
//in comes ticks globally
//related to internal loop clicks
//relate beats to internal beat structure

//SCHEDULE midi off events from the on events
//relative to the global tick counter
//not waiting for them to happen in the loop

//RHYTHM
//look at the way drum machine rhythm can be used to generate patterns
//sparse and busy

//Vertical and Horizontal players

//key and chord input

//how to map these
//need to listen to variables in the global structure




//need to schedule note offs for each note on
//looping might mean we exceed length of loop
 
JuceSequenceLoopPlayer::JuceSequenceLoopPlayer() : midiLogListBoxModel (midiMessageList){
    ppq = 480; 
    //set loop
    
    setupCorrect = false;
    
    loopStartTicks = 0;
    loopEndTicks = ppq*16;//4 beat loop of beginning
    
    setLoopPointsBeats(0, 32);
//    loopStartBeats = 0;
  //  loopEndBeats = 16;
   // loopWidthBeats = loopEndBeats-loopStartBeats;
    
    checkLock = false;//open to calling
    name = "";
    
    midiPlayIndex = -1;
    midiOutDevice = NULL;
    
    reset();
    
    
    invertScale[0] = 0;
    invertScale[1] = 2;
    invertScale[2] = 3;
    invertScale[3] = 5;
    invertScale[4] = 7;
    invertScale[5] = 9;
    invertScale[6] = 10;
    invertScale[7] = 12;
    invertAnchor = 50;
    
    reversedValue = 0;
    
    outputCheckIndex = 0;
    
    
    midiViewer.setSequence(beatDefinedSequence);

    recordedNoteOffHappened = false;//not really needed in setup
   
}

JuceSequenceLoopPlayer::~JuceSequenceLoopPlayer(){
    stop();
    
    sendAllScheduledNoteOffs();
}

void JuceSequenceLoopPlayer::resized(){
    midiViewer.resized();
}

void JuceSequenceLoopPlayer::setSequence(const MidiMessageSequence& targetSequence, int tmpppq){
    std::cout << name << " set sequence " << targetSequence.getNumEvents() << " events" << std::endl;
    originalSequence = targetSequence;
    originalSequence.updateMatchedPairs();
    transformedSequence = targetSequence;
    transformedSequence.updateMatchedPairs();
    ppq = tmpppq;
    
    beatDefinedSequence = targetSequence;
    beatDefinedSequence.updateMatchedPairs();
    
    changeTicksToBeats(originalSequence);
    changeTicksToBeats(beatDefinedSequence);
    
    printSequenceEvents(beatDefinedSequence);
    std::cout << "END BEAT DEFINED" << std::endl;
    
    //note we count from zero beats
}


void JuceSequenceLoopPlayer::setLoopPointsBeats(float startLoop, float endLoop){
    if (endLoop > startLoop && startLoop >= 0){
        loopStartBeats = startLoop;
        loopEndBeats = endLoop;
        loopWidthBeats = endLoop - startLoop;
        
        midiViewer.loopMax = &loopEndBeats;
        midiViewer.loopMin = &loopStartBeats;
    }
}

void JuceSequenceLoopPlayer::changeTicksToBeats(MidiMessageSequence& sequence){
    
    std::cout <<"\nCHANGE TICKS TO BEATS" << std::endl;
//    printSequenceEvents(sequence);
    
    //might need to think if there were structural changes - eg to 5/8 or something?
    for (int i = 0 ; i < sequence.getNumEvents(); i++){
        double tmpTime = sequence.getEventTime(i);
  //      std::cout << tmpTime << "-> " << tmpTime/ppq << std::endl;
        sequence.getEventPointer(i)->message.setTimeStamp(tmpTime/ppq);
    }
//    std::cout <<"\nTICKS CHANGEED TO BEATS" << std::endl;
//    printSequenceEvents(sequence);
}
 
void JuceSequenceLoopPlayer::reset(){
   // millisCounter = 0;
    //beatMillisCounter = 0;//position of millisCounter at last beat
    beatTick = -1;//counter set every time incoming beat happens
    midiPlayIndex = -1;//index in sequence we have played
    lastTick = 0;
    
    outputCheckIndex = 0;//for checking which events should be output

    lastBeatPosition = 0;//-1;
    
    lastRecordedBeatTime = 0;
    
}

void JuceSequenceLoopPlayer::stop(){
    
    std::cout << name << "STOP" << std::endl;
    
    
    sendAllScheduledNoteOffs();
    
    reset();
    

    
   // printSequenceEvents(transformedSequence);
}

void JuceSequenceLoopPlayer::sendAllScheduledNoteOffs(){
    while (scheduledEvents.getNumEvents() > 0){
        int index = 0;
        sendMessageOut(scheduledEvents.getEventPointer(index)->message);
        std::cout << name << "stop: sending note off " << scheduledEvents.getEventPointer(index)->message.getNoteNumber() << " at " << scheduledEvents.getEventPointer(index)->message.getTimeStamp() << std::endl;
        scheduledEvents.deleteEvent(index, false);
    }
}

//void newBeat(){
//    tickPosition = getTicksFromBeat(beatPosition);
//}



float JuceSequenceLoopPlayer::getLoopPosition(const float& beatPosition){
    if (beatPosition <= loopEndBeats){
        return beatPosition;
    } else {
        float beatTime = beatPosition - loopStartBeats;//amount since we started looping
        beatTime = getModulo(beatTime, loopWidthBeats);
        beatTime += loopStartBeats;
        return beatTime;
    }

}

float JuceSequenceLoopPlayer::getModulo(float& highValue, float& moduloValue){
    int roundedDivided = (int)(highValue / moduloValue);
    float modResult = highValue - moduloValue*roundedDivided;
    return modResult;
}

#pragma mark MainUpdateRoutineForPlayback
void JuceSequenceLoopPlayer::alternativeUpdateToBeat(const float& newBeat){
    //new alternative update based on beats

    if (!setupCorrect)
        std::cout << "please call setup routine first" << std::endl;
    
    currentGlobalBeatPosition = newBeat;
    
    float beatNow = getLoopPosition(newBeat);
    
   // std::cout << name << "altupdate " << newBeat << ", beatnow " << beatNow << std::endl;
    //float zero = 0;
    
    if (beatNow <= lastBeatPosition - 0.1){
        //gone round the loop back to start
        //as we might get the odd beat event out of order we include 0.1 to ensure they are far enough apart
        
        //then we went backwards
        //normal
         std::cout << "gone round to " << beatNow << "  (actual beat " << newBeat << ") from lastBeatPosition " <<lastBeatPosition << std::endl;
        if (!checkLock){
            //bug here if hanging note??
            
            //to go round the bend do these checks last til loop end, then start til now:
            //if (!recordingOn)
            checkOutput(lastBeatPosition, loopEndBeats);
   
            outputCheckIndex = 0;//so we check from beginning again
    
            //if (!recordingOn)
            checkOutput(loopStartBeats, beatNow);
        
            lastBeatPosition = beatNow;
        }
        
        
        if (recordingOn){
            //loop is going round
            //endRecording();
        }
    }
    else if (beatNow > lastBeatPosition && ! checkLock){
        //if (!recordingOn)
        checkOutput(lastBeatPosition, beatNow);
        
        lastBeatPosition = beatNow;
    }

    checkNoteOffs();
    
    checkLoopRecordingEnded(newBeat);
    
}

void JuceSequenceLoopPlayer::endRecording(){
    std::cout << name << ": xsc RECORDED_SEQUENCE" << std::endl;
    printSequenceEvents(recordedSequence);
    
    copyRecordedSequenceOver();
    recordingOn = false;
    /*
    beatDefinedSequence = recordedSequence;
    beatDefinedSequence.updateMatchedPairs();

    originalSequence = beatDefinedSequence;
    originalSequence.updateMatchedPairs();
    
    //printSequenceEvents(beatDefinedSequence, loopEndBeats);
    //check for an unmatched note
    for (int i = 0; i < beatDefinedSequence.getNumEvents(); i++){
        if (beatDefinedSequence.getEventPointer(i)->message.isNoteOn() && beatDefinedSequence.getIndexOfMatchingKeyUp(i) == -1){
            std::cout << "UNMATCHED NOTE! - index " << i << std::endl;
            printSequenceEvents(beatDefinedSequence, loopEndBeats);
        }
    }
     */
    std::cout << "changing viewer" << std::endl;
    //changing viewer value says 'update the window'
    viewerValue->setValue((int)viewerValue->getValue()+1);
}

void JuceSequenceLoopPlayer::copyRecordedSequenceOver(){
    beatDefinedSequence.clear();
    recordedSequence.updateMatchedPairs();
    std::cout << "COPY_RECORDED_SEQ" << std::endl;
    printSequenceEvents(recordedSequence);
    
    int index = recordedSequence.getNumEvents()-1;
    float lastRecordedOnTime = 0;
    while (index >= 0){
        if (recordedSequence.getEventPointer(index)->message.isNoteOn()){
            lastRecordedOnTime = recordedSequence.getEventTime(index);
            break;
        }
        index--;
    }
    std::cout << " last recorded on " << lastRecordedOnTime << std::endl;
    
    
    
    float startTime = lastRecordedOnTime - loopEndBeats + 0.1225;//1.0/EIGHTH_NOTES;
    index = recordedSequence.getNumEvents()-1;
    while (index >= 0 && recordedSequence.getEventTime(index) > startTime){
        
        if (!recordedSequence.getEventPointer(index)->message.isNoteOff()){
        //only do note on's and controlm changes
            MidiMessage m;
            m = recordedSequence.getEventPointer(index)->message;
            int pitch = m.getNoteNumber();
            float timeOn = getLoopPosition(recordedSequence.getEventTime(index));
            m.setTimeStamp(timeOn);
            beatDefinedSequence.addEvent(m, 0.0);
            
            if (m.isNoteOn()){
                float dur = 1;
                if (recordedSequence.getIndexOfMatchingKeyUp(index) != -1){
                    dur = recordedSequence.getEventTime(recordedSequence.getIndexOfMatchingKeyUp(index)) - recordedSequence.getEventTime(index);
                    MidiMessage mOff;
                    mOff = recordedSequence.getEventPointer(recordedSequence.getIndexOfMatchingKeyUp(index))->message;
                    mOff.setTimeStamp(timeOn + dur);
                    beatDefinedSequence.addEvent(mOff, 0.0);
                } else {
                    MidiMessage mOff(128, pitch, 0);
                    mOff.setTimeStamp(timeOn+0.5);
                    beatDefinedSequence.addEvent(mOff, 0.0);
                }

            }
        }
        index--;
    }
    beatDefinedSequence.updateMatchedPairs();
    std::cout << "RESULTING SEQ" << std::endl;
    printSequenceEvents(beatDefinedSequence);
}

void JuceSequenceLoopPlayer::checkNoteOffs(){
    int index = 0;
    juce::MidiMessageSequence::MidiEventHolder* msgPointer;
    
    while (index < scheduledEvents.getNumEvents() && scheduledEvents.getEventTime(index) < *milliscounter){
        if (scheduledEvents.getEventPointer(index) != NULL){
            msgPointer = scheduledEvents.getEventPointer(index);
            sendMessageOut(msgPointer->message);
            if (msgPointer->message.isNoteOnOrOff()){
                std::cout << name << " sending note off " << msgPointer->message.getNoteNumber() << " at " << msgPointer->message.getTimeStamp() << std::endl;
            }
            scheduledEvents.deleteEvent(index, false);
        }
    }
}


void JuceSequenceLoopPlayer::checkLoopRecordingEnded(const float& beatTime){
    if (recordingOn && recordedNoteOffHappened){
        if (beatTime > lastRecordedBeatTime + 1.5){
            std::cout << "END RECORDING?" << std::endl;
            endRecording();
        }
            
    }
}



void JuceSequenceLoopPlayer::checkOutput(float& lastBeatTime, const float& beatTime){
    //std::cout << name << "checkOutput: index " << outputCheckIndex << " check from " << lastBeatTime << " to " << beatTime << std::endl;
    
//    while (outputCheckIndex > 0 && beatDefinedSequence.getEventTime(outputCheckIndex) > lastBeatTime){
//        outputCheckIndex--;
//    }
    
    checkLock = true;
    
    while (outputCheckIndex < beatDefinedSequence.getNumEvents() && beatDefinedSequence.getEventTime(outputCheckIndex) < lastBeatTime){
       // std::cout << name << ": outcheck index skipping " << outputCheckIndex << " event time " << beatDefinedSequence.getEventTime(outputCheckIndex) << " time " << lastBeatTime << " to " << beatTime << std::endl;
        outputCheckIndex++;
    }
 
    
    while (outputCheckIndex < beatDefinedSequence.getNumEvents() && beatDefinedSequence.getEventTime(outputCheckIndex) <beatTime && beatDefinedSequence.getEventTime(outputCheckIndex) < loopEndBeats){//last condition to avoid repetition on loop boundary
        float tmpTime = beatDefinedSequence.getEventTime(outputCheckIndex);
        if (tmpTime >= lastBeatTime && tmpTime < beatTime){
            //output event

            if (!recordingOn && beatDefinedSequence.getEventPointer(outputCheckIndex)->message.isNoteOn()){
                std::cout << name << ": NOTE_ON (" << outputCheckIndex << ")" << " pitch " << beatDefinedSequence.getEventPointer(outputCheckIndex)->message.getNoteNumber() << " time " << tmpTime << std::endl;
                
                int offIndex = beatDefinedSequence.getIndexOfMatchingKeyUp(outputCheckIndex);
                int millisScheduled = 0;
                if (offIndex != -1){
                    std::cout << "Off index " << offIndex << " time " << beatDefinedSequence.getEventTime(offIndex) << std::endl;
                
                    float beatDifference = beatDefinedSequence.getEventTime(offIndex) - beatDefinedSequence.getEventTime(outputCheckIndex);
                    millisScheduled = *milliscounter + beatsToMillis(beatDifference);
                    std::cout << name << "millis now " <<  *milliscounter << ", scheduled off time " << millisScheduled << std::endl;
                    //int channel = beatDefinedSequence.getEventPointer(outputCheckIndex)->message.getChannel();
                }
                
                //add the scheduled note off and send out note on out
                if (offIndex >= 0 && offIndex < beatDefinedSequence.getNumEvents()){
                    MidiMessage m = beatDefinedSequence.getEventPointer(offIndex)->message;
                    addNoteOff(m, millisScheduled);
                } else {
                    std::cout << name << " STRANGE, POINTER DOESNT EXIST FOR index " << offIndex << " events " << beatDefinedSequence.getNumEvents() << std::endl;
                }
                
                MidiMessage outMessage = beatDefinedSequence.getEventPointer(outputCheckIndex)->message;//debug to prevent pointer craching
                sendMessageOut(outMessage);
                    noteOutBeatTime = beatDefinedSequence.getEventTime(outputCheckIndex);
                
            } else if (beatDefinedSequence.getEventPointer(outputCheckIndex)->message.isNoteOff()){
                std::cout << name << ": NOTE_OFF (" << outputCheckIndex << ") pitch " << beatDefinedSequence.getEventPointer(outputCheckIndex)->message.getNoteNumber() << " at " <<  tmpTime << ", millis " << *milliscounter << std::endl;
                
            } else if (!recordingOn){
                //so must be something else
                sendMessageOut(beatDefinedSequence.getEventPointer(outputCheckIndex)->message);
            }
            
        }
        outputCheckIndex++;
    }
    
    checkLock = false;
}

int JuceSequenceLoopPlayer::beatsToMillis(float& beats){
    return round(beats * (*tempoMillis));
}

float millisToBeats(){
    return 0;//((millis - lastBeatPosition)/beatPeriod)+lastAltBeatIndex;
}

//int JuceSequenceLoopPlayer::millisToBeats(float& millis){
//}


void JuceSequenceLoopPlayer::addNoteOff(MidiMessage& message, int millisTime){
    //std::cout << "channel is " <<
    MidiMessage m;
    m = message;
    m.setTimeStamp(millisTime);
    std::cout << name << " note off " << m.getChannel() << " pitch " << m.getNoteNumber() << " time " << m.getTimeStamp() << std::endl;
    scheduledEvents.addEvent(m);
//    MidiMessage(127, pitch, 0);
}

void JuceSequenceLoopPlayer::sendMessageOut(MidiMessage& m){
    if (midiOutDevice != nullptr){// && m.isNoteOn())
        
        midiOutDevice->sendMessageNow(m);
       
        if (m.isNoteOn()){
            noteOnValue = m.getNoteNumber();
            noteOutVelocity = m.getVelocity();
        }
     }
}


#pragma mark MidiMessageIn
void JuceSequenceLoopPlayer::newMidiMessage(const MidiMessage& message, float& beatTime){
    
    
    MidiMessage copyMessage = message;
    copyMessage.setTimeStamp(beatTime);
    midiMessageList.add (copyMessage);
    
    triggerAsyncUpdate();
    
    float tmp = lastMidiMessageInTime();
    
    const uint8* data = message.getRawData();
    std::cout << name << " received " << (int)data[0] << ", " << (int)data[1] << ", " << (int)data[2];
    std::cout << "at time " << beatTime << ", previous midi message time " << tmp << std::endl;
    
    newRecordedMessageIn(message, beatTime);
    
}

float JuceSequenceLoopPlayer::lastMidiMessageInTime(){
    if (midiMessageList.size() > 0){
        return midiMessageList[midiMessageList.size()-1].getTimeStamp();
    } else
        return -1;
}


void JuceSequenceLoopPlayer::newRecordedMessageIn(const MidiMessage& message, float& beatTime){
    if (beatTime >= 0){
        if (message.isNoteOnOrOff()){
        
            if (message.isNoteOn() && !recordingOn){
                //start recording
                recordingOn = true;
                sendAllScheduledNoteOffs();
                std::cout << name << "xsc RECORDING STARTED" << std::endl;
                recordedSequence.clear();
                recordedNoteOffHappened = false;

            } else {
                std::cout << "maybe this is hanging note?" << std::endl;
                //has been crash bug here
                checkHangingNote(message, beatTime);
            }
            
            
            float loopTime = beatTime;
            if (message.isNoteOn())
                loopTime = quantise(beatTime);//getLoopPosition(beatTime));
                
            MidiMessage copyMessage = message;
            copyMessage.setTimeStamp(loopTime);//getLoopPosition(loopTime));
            recordedSequence.addEvent(copyMessage);
            const uint8* data = message.getRawData();
            std::cout << name << " add recorded event " << (int)message.getNoteNumber() << ": midi " << (int)data[0] << ", " << (int)data[1] << ", at time " << beatTime << " loop time " << getLoopPosition(beatTime) << std::endl;
        
            
            if (message.isNoteOff()){
                lastRecordedBeatTime = beatTime;
                recordedNoteOffHappened = true;
                std::cout << "LAST_RECORDED_BEAT_TIME " << beatTime << std::endl;
            }
        } else {
        //not note on or off - recording into the sequence - visualisation??
            MidiMessage copyMessage = message;
            float loopTime = getLoopPosition(beatTime);
            copyMessage.setTimeStamp(getLoopPosition(loopTime));
            recordedSequence.addEvent(copyMessage);
            const uint8* data = message.getRawData();
            std::cout << name << " add NON-NOTE recorded event: " << (int)data[0] << ", " << (int)data[1] << ", at time " << beatTime << " loop time " << loopTime << std::endl;
            
        }
    } else{
        //i.e beattime < 0
        const uint8* data = message.getRawData();
        std::cout << name << " midi " << (int)data[0] << ", " << (int)data[1] << ", at time " << getLoopPosition(beatTime) << std::endl;
        std::cout << "out of time" << std::endl;
    }
}

float JuceSequenceLoopPlayer::quantise(const float& eventTime){
    return (roundf(eventTime*SIXTEENTH_NOTES)/SIXTEENTH_NOTES);
}

void JuceSequenceLoopPlayer::checkHangingNote(const MidiMessage& message, float& beatTime){
    for (int i = beatDefinedSequence.getNumEvents()-1; i >= 0 ; i--){
        if (beatDefinedSequence.getIndexOfMatchingKeyUp(i) == -1 && beatDefinedSequence.getEventPointer(i)->message.isNoteOn() && message.getNoteNumber() == beatDefinedSequence.getEventPointer(i)->message.getNoteNumber()){
            std::cout << "YES, GOT A MATCH for index " << i << " in sequence" << std::endl;
            MidiMessage copyMessage = message;
            copyMessage.setTimeStamp(getLoopPosition(beatTime) + loopEndBeats);//need to put it after end
            beatDefinedSequence.addEvent(copyMessage);
            beatDefinedSequence.updateMatchedPairs();
            originalSequence.addEvent(copyMessage);
            originalSequence.updateMatchedPairs();//add to origanl as well
            break;
           // std::cout << "SEQUENCE UPDATED" << std::endl;
           // printSequenceEvents(beatDefinedSequence, loopEndBeats);
        }
    }
}


void JuceSequenceLoopPlayer::updateMidiPlayPositionToTickPosition(double startTick, double tickPosition){
  //think this may be old???/unused?
    
    int numEvents = transformedSequence.getNumEvents();
    
    
    MidiMessageSequence::MidiEventHolder* outputEvent;
    
    while (midiPlayIndex < numEvents && transformedSequence.getEventTime(midiPlayIndex) < tickPosition){
        //we have caught up to where we are
        //std::cout << "play index " << midiPlayIndex << " at time " << trackSequence.getEventTime(midiPlayIndex) << std::endl;
        
        if (midiPlayIndex >= 0 && transformedSequence.getEventTime(midiPlayIndex) >= startTick){
            //only output when position at least greater than start
            outputEvent = transformedSequence.getEventPointer(midiPlayIndex);
            
            if (outputEvent->message.isNoteOnOrOff()){
                int pitch = outputEvent->message.getNoteNumber();
                
                if (midiOutDevice != NULL){
                    std::cout << name << ": Note OUT " << pitch << std::endl;
                    midiOutDevice->sendMessageNow(outputEvent->message);//..Out(outputEvent->message);
                }
                if (outputEvent->message.isNoteOn()){
                    
                    noteOnValue = pitch;
                    
                    int tmp = transformedSequence.getIndexOfMatchingKeyUp(midiPlayIndex);
                    std::cout << "SequenceLooper " << name <<" NOTE ON  " << outputEvent->message.getNoteNumber();
                    std::cout << " index " << midiPlayIndex << " has up key " << tmp;//<< std::endl;
                    std::cout << ", tick position " << tickPosition << "beattick " << beatTick << std::endl;
                } else {
                    std::cout << "SequenceLooper: " << name << " NOTE Off " << pitch << " index " << midiPlayIndex << std::endl;
                }
            }
            else if (outputEvent->message.isController()){
                if (midiOutDevice != NULL)
                    midiOutDevice->sendMessageNow(outputEvent->message);
                
                std::cout << name << "event " << midiPlayIndex << "is controller " << outputEvent->message.getControllerNumber();
                std::cout << " value " << outputEvent->message.getControllerValue() << std::endl;
                
            } else if (outputEvent->message.isPitchWheel()){
                if (midiOutDevice != NULL)
                    midiOutDevice->sendMessageNow(outputEvent->message);
                
                std::cout << "SequenceLooper: " << name << " PITCH WHEEL,  " << outputEvent->message.getPitchWheelValue();
                std::cout << "tick position " << tickPosition << std::endl;
                
            } else if (outputEvent->message.isAftertouch()){
                if (midiOutDevice != NULL)
                    midiOutDevice->sendMessageNow(outputEvent->message);
                
                std::cout << "SequenceLooper: " << name << " AFTERTOUCH ,  " << outputEvent->message.getAfterTouchValue();
                std::cout << "tick position " << tickPosition << std::endl;
            } else {
                
                const uint8* data = outputEvent->message.getRawData();
                if ((data[0] & 0xf0) == 0xd0)
                    std::cout << name << " channel aftertouch  " << midiPlayIndex << " is not note on/off, data[0] " << (int)data[0] << std::endl;
                else
                    std::cout << name << " event " << midiPlayIndex << " is not note on/off, data[0] " << (int)data[0] << std::endl;
            }
        }//end if midiplayindex
        
        midiPlayIndex++;
        
        //int64 timenow = juce::Time::currentTimeMillis();
       // std::cout << "sequence looper : timer callback " << timenow << std::endl;
        
    }
    
    lastTick = (int)tickPosition;//(int)(millisCounter * playbackSpeed);//relative to 1ms = 1 tick
    
    outputEvent = NULL;
    delete outputEvent;
    
}



#pragma mark FunctionsOnSequences
void JuceSequenceLoopPlayer::printSequenceEvents(const MidiMessageSequence& sequence){
    printSequenceEvents(sequence, (float)sequence.getEndTime());
}

void JuceSequenceLoopPlayer::printSequenceEvents(const MidiMessageSequence& sequence, float maxBeat){
    
    std::cout << name << " print sequence has " << sequence.getNumEvents() << " events, ppq " << ppq << ", max beattime " << maxBeat << std::endl;
    
    MidiMessageSequence::MidiEventHolder* event;//pointer to an individual midi event
    
    for (int i = 0; i < sequence.getNumEvents() && sequence.getEventTime(i) <= maxBeat; i++){
        //std::cout << i << std::endl;
        //or parse through the events themselves
        event = sequence.getEventPointer(i);
        
        //can get this time info from the track sequence
        if (event->message.isNoteOnOrOff()){
            double eventTime = sequence.getEventTime(i);
            std::cout << name << " print seq[" << i << "]: eventtime " << eventTime;// << std::endl;
            
            
            
            double tmp = event->message.getTimeStamp();
            std::cout << ", timestamp " << tmp ;//<< std::endl;
            
            const uint8* data = event->message.getRawData();
            
            std::cout << " (" <<  event->message.getRawDataSize() << ") ";
            
            if (event->message.isNoteOn())
                std::cout << "note on : ";// << std::endl;
            else if (event->message.isNoteOff())
                std::cout << "note off: ";// << std::endl;
            else
                std::cout << "other   : ";//<< std::endl;
            
            std::cout << (int)data[0] << ", " << (int)data[1] << ", " << (int)data[2];

            if (event->message.isNoteOn())
                std::cout << " matches " << sequence.getIndexOfMatchingKeyUp(i) << std::endl;
            else
                std::cout << std::endl;
        
            if (data[0] == 255){
                for (int i = 3; i < event->message.getRawDataSize(); i++){
                    //           std::cout << "character[" << i-3 << "] :'" << data[i] << "'" << std::endl;
                }
            }
        }//end if on or off
        
    }//end for
    event = NULL;
    delete event;
    
}


void JuceSequenceLoopPlayer::revertToOriginal(){
    beatDefinedSequence = originalSequence;
    std::cout << "REVERT TO ORIGINAL" << std::endl;
    beatDefinedSequence.updateMatchedPairs();//why needs doing?
    printSequenceEvents(beatDefinedSequence, loopEndBeats);
    reversedValue = 0;
}


void JuceSequenceLoopPlayer::reverseOriginal(){
    reversedValue = 1;
//  reverseSequence(transformedSequence, originalSequence, loopStartTicks, loopEndTicks);
  
    std::cout << name << " before reverse " << std::endl;
    printSequenceEvents(beatDefinedSequence, loopEndBeats);

    reverseSequence(beatDefinedSequence, originalSequence, loopStartBeats, loopEndBeats);

    std::cout << name << " after reverse " << std::endl;
    printSequenceEvents(beatDefinedSequence, loopEndBeats);
    
}

void JuceSequenceLoopPlayer::reverseSequence(MidiMessageSequence& reversedSequence, const MidiMessageSequence& sequence, float startStamp, float endStamp){
    
    std::cout << name << "REVERSE " << startStamp << " to " << endStamp << std::endl;
    
    //sequence.updateMatchedPairs();//done?
    
    MidiMessageSequence emptyLoop;
    
    MidiMessageSequence::MidiEventHolder* eventHolder;
    MidiMessageSequence::MidiEventHolder* eventOffHolder;
    
    for (int i = 0; i < sequence.getNumEvents() && sequence.getEventTime(i) <= endStamp; i++){
        float tmpTime = sequence.getEventTime(i);
        
        if (tmpTime >= startStamp && sequence.getIndexOfMatchingKeyUp(i) != -1){//i.e. there is a note off
            eventHolder = sequence.getEventPointer(i);
            eventOffHolder = sequence.getEventPointer(sequence.getIndexOfMatchingKeyUp(i));
            
            if (eventHolder->message.isNoteOn()){
                
                //add in if between start and end
                float offTime = sequence.getEventTime(sequence.getIndexOfMatchingKeyUp(i));
                float duration = offTime-tmpTime;
                std::cout << "event index " << i << ", " << "time " << tmpTime;
                std::cout << " has  off index " << sequence.getIndexOfMatchingKeyUp(i) << " time " << offTime << std::endl;
                std::cout << " note on maps to " << endStamp - tmpTime << ", note off to " << endStamp - tmpTime + duration << std::endl;
                
                const uint8* data = eventHolder->message.getRawData();
                //std::cout << "data[0] " << (int)data[0] << " [1]: " << (int)data[1] << " [2]: " <<    (int)data[2] << std::endl;
                MidiMessage newOnMessage((int)data[0], (int)data[1], (int)data[2], (double)(endStamp - tmpTime));
                
                const uint8* dataOff = eventOffHolder->message.getRawData();
                MidiMessage newOffMessage(dataOff[0], dataOff[1], dataOff[2], (double)(endStamp - tmpTime + duration));
                
                //eventHolder->message.setTimeStamp(endStamp - tmpTime);
                //eventOffHolder->message.setTimeStamp(endStamp - tmpTime + duration);
                //would ignores these timestamps anyhow, set above in the message creation
                emptyLoop.addEvent(newOnMessage);
                emptyLoop.addEvent(newOffMessage);
            }//end if note on
            
        }//end if > start
        
        
    }
    
    
    emptyLoop.updateMatchedPairs();
    
    //so you could reverse a sequence and return it to itself
    //overwriting the original after the transformation
    reversedSequence = emptyLoop;
    reversedSequence.updateMatchedPairs();
    
}


int JuceSequenceLoopPlayer::invertPitch(int pitch){
    int startPitch = invertAnchor;
    while (startPitch + 12 <= pitch)
        startPitch += 12;
    
    while (startPitch > pitch)
        startPitch -= 12;
    int i = 0;
    //startpitch now in range
    while (startPitch + invertScale[i] != pitch && i < 7){
        i++;
    }
    std::cout << "start p " << startPitch << " i " << i << std::endl;
    
    int newPitch = startPitch - (pitch - startPitch);
    
    if (startPitch+invertScale[i] == pitch){
        newPitch = (startPitch + invertScale[7-i]);
        std::cout << "return " << newPitch << std::endl;
    }
    else {
        //stick with simple inversion from anchor, not in scale
        std::cout << "note not in scale " << pitch << " return " << newPitch << std::endl;
        return newPitch;
    }
    return newPitch;
}

void JuceSequenceLoopPlayer::invertOriginal(){
    
    invertSequence(beatDefinedSequence, originalSequence, loopStartBeats, loopEndBeats);
    beatDefinedSequence.updateMatchedPairs();
    std::cout << "INVERT ORIGINAL" << std::endl;
    printSequenceEvents(beatDefinedSequence, loopEndBeats);
}

void JuceSequenceLoopPlayer::invertSequence(MidiMessageSequence& invertedSequence, const MidiMessageSequence& sequence, int startStamp, int endStamp){
    
    std::cout << name << "INVERT " << startStamp << " to " << endStamp << std::endl;
    
    //sequence.updateMatchedPairs();//done?
    
    MidiMessageSequence emptyLoop;
    
    MidiMessageSequence::MidiEventHolder* eventHolder;
    MidiMessageSequence::MidiEventHolder* eventOffHolder;
    
    for (int i = 0; i < sequence.getNumEvents() && sequence.getEventTime(i) <= endStamp; i++){
        float tmpTime = sequence.getEventTime(i);
        
        if (tmpTime >= startStamp && sequence.getIndexOfMatchingKeyUp(i) != -1){//i.e. there is a note off
            eventHolder = sequence.getEventPointer(i);
            eventOffHolder = sequence.getEventPointer(sequence.getIndexOfMatchingKeyUp(i));
            
            if (eventHolder->message.isNoteOn()){
                
                //add in if between start and end
                float offTime = sequence.getEventTime(sequence.getIndexOfMatchingKeyUp(i));
                int pitch = eventHolder->message.getNoteNumber();
                std::cout << "pitch " << pitch << std::endl;
                int newPitch = invertPitch(pitch);
                
                //int duration = offTime-tmpTime;
                std::cout << "event index " << i << ", " << "time " << tmpTime;
                std::cout << " pitch " << pitch << " maps to " << newPitch << std::endl;
                
                
                const uint8* data = eventHolder->message.getRawData();
                //std::cout << "data[0] " << (int)data[0] << " [1]: " << (int)data[1] << " [2]: " <<    (int)data[2] << std::endl;
                MidiMessage newOnMessage((int)data[0], newPitch, (int)data[2], tmpTime);
                
                const uint8* dataOff = eventOffHolder->message.getRawData();
                MidiMessage newOffMessage(dataOff[0], newPitch, dataOff[2], offTime);
                
                emptyLoop.addEvent(newOnMessage);
                emptyLoop.addEvent(newOffMessage);
            }//end if note on
            
        }//end if > start
        
        
    }
    
    
    emptyLoop.updateMatchedPairs();
    
    //so you could reverse a sequence and return it to itself
    //overwriting the original after the transformation
    invertedSequence = emptyLoop;
    
}

void JuceSequenceLoopPlayer::transposeSequence(int notesInScale){
    transposeSequence(originalSequence, notesInScale);
    transposeSequence(beatDefinedSequence, notesInScale);
    transposeSequence(transformedSequence, notesInScale);
}


void JuceSequenceLoopPlayer::transposeSequence(MidiMessageSequence& sequence, int notesInScale){
    for (int i = 0; i < sequence.getNumEvents(); i++){
        if (sequence.getEventPointer(i)->message.isNoteOnOrOff()){
            sequence.getEventPointer(i)->message.setNoteNumber(sequence.getEventPointer(i)->message.getNoteNumber()+notesInScale);
        }
    }
    
}


/*

 void JuceSequenceLoopPlayer::updatePlaybackToBeat(int& beatIndex){
 //defunkt
 
 //, int& millisCount){
 // millisCounter = millisCount;
 std::cout << name << " update to beat " << beatIndex << " beatticks " << beatTick << std::endl;
 beatTick = getTicksFromBeat(beatIndex*ppq);
 std::cout << " after beat ticks " << beatTick << std::endl;
 }
 
 double JuceSequenceLoopPlayer::getTicksFromBeat(double beatPositionTicks){
 //defunkt
 double tmp = beatPositionTicks;
 double loopWidthTicks = loopEndTicks - loopStartTicks;
 while ((int)beatPositionTicks > loopEndTicks){//vital to have =?
 beatPositionTicks -= loopWidthTicks;
 }
 
 std::cout << "beat position " << tmp << " == " << beatPositionTicks << " for loop ";
 std::cout << loopStartTicks << " - " << loopEndTicks << std::endl;
 
 return beatPositionTicks;
 }
 
 void JuceSequenceLoopPlayer::updateTicksSinceLastBeat(double ticksSinceBeatTick){
     //defunkt
     
     //std::cout << "ticks since beat " << ticksSinceBeatTick << std::endl;
     if (beatTick + ticksSinceBeatTick <= loopEndTicks){
         //simple update to new ticks
         updateMidiPlayPositionToTickPosition(lastTick, beatTick + ticksSinceBeatTick);
     } else {
         //update to end
         updateMidiPlayPositionToTickPosition(lastTick, loopEndTicks);
         
         //schedule note offs
         //       TO DO
         //reformat beattick to before loop
         beatTick -= (loopEndTicks - loopStartTicks);//put beattick back before loop started
         
         //actually when we get new beatIndex, we calculate from that so this is temporary
         std::cout << "LOOP GOING ROUND" << std::endl;
         std::cout << "NEW BEAT TICK IS " << beatTick << "ticks since " << ticksSinceBeatTick << std::endl;
         
         midiPlayIndex = 0;
         //lastTick = loopStartTicks;
         //update from beginning of loop to where we are now
         
         updateMidiPlayPositionToTickPosition(loopStartTicks, beatTick+ticksSinceBeatTick);//update beginning part of loop
         
     }
 }
 
 void JuceSequenceLoopPlayer::updateToBeatPosition(const float& beatPosition){
 
 if (beatPosition > lastBeatPosition){
 //do update - eg schedule notes
 
 // std::cout << "check " << beatPosition << std::endl;
 //   checkOutput(lastBeatPosition, beatPosition);
 //    lastBeatPosition = beatPosition;
 } else {
 std::cout << "NO UPDATE " << beatPosition << " as last posiution " << lastBeatPosition << std::endl;
 
 }
 
 //it could happen that beat position is ahead - eg predicted from last beat
 //then we find we are behind
 //we cannot unschedule any events that have been played
 //only allow ourselves to catch up
 //in practice this is only around beats moving in tempo
 //not too problematic
 
 //could kalman filter etc but we expect our prediction to be accurate
 
 }
 */
