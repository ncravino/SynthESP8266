//    SynthESP8266
//    Copyright (C) 2025 ncravino
//
//    This program is free software: you can redistribute it and/or modify
//    it under the terms of the GNU Affero General Public License as published by
//    the Free Software Foundation, either version 3 of the License, or
//    (at your option) any later version.
//
//    This program is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU Affero General Public License for more details.
//
//    You should have received a copy of the GNU Affero General Public License
//    along with this program.  If not, see <https://www.gnu.org/licenses/>.
 
#include "MozziConfigValues.h"  
#define MOZZI_OUTPUT_MODE  MOZZI_OUTPUT_PDM_VIA_SERIAL
#define MOZZI_ANALOG_READ MOZZI_ANALOG_READ_NONE
#define MOZZI_AUDIO_RATE 32768  
#define MOZZI_CONTROL_RATE 128 

#include <Mozzi.h>
#include <Oscil.h>

#pragma GCC diagnostic ignored "-Wno-expansion-to-defined"
 
#include <MozziGuts.h>
#include <Oscil.h>
#include <tables/saw2048_int8.h> 
#include <tables/smoothsquare8192_int8.h> 
#include <tables/triangle2048_int8.h>
#include <tables/brownnoise8192_int8.h>
#include <ResonantFilter.h>
#include <ReverbTank.h>
#include <ADSR.h>
#include <mozzi_rand.h> 
#include <mozzi_midi.h>  
#include <ESP8266WiFi.h>
#include <EventDelay.h>

Oscil<TRIANGLE2048_NUM_CELLS, AUDIO_RATE> drone; 
ADSR<CONTROL_RATE, AUDIO_RATE> drone_env; 

Oscil<SMOOTHSQUARE8192_NUM_CELLS, AUDIO_RATE> synth1;
ADSR<CONTROL_RATE, AUDIO_RATE> synth1_env; 
ResonantFilter<LOWPASS, uint16_t> synth1_filt;

Oscil<SAW2048_NUM_CELLS, AUDIO_RATE> synth2;
ADSR<CONTROL_RATE, AUDIO_RATE> synth2_env; 

Oscil<SMOOTHSQUARE8192_NUM_CELLS, AUDIO_RATE> kicks; 
ADSR<CONTROL_RATE, AUDIO_RATE> kick_env; 

Oscil<BROWNNOISE8192_NUM_CELLS, AUDIO_RATE> hats; 
ADSR<CONTROL_RATE, AUDIO_RATE> hat_env; 

uint16_t last_a0_val=0;

void readA0(){
  int read_val = analogRead(A0);
  last_a0_val = (read_val*64)-1;
}

EventDelay seq_event; 

#define DRONETRACK 0
#define KICKTRACK 1
#define HATTRACK 2
#define SYNTH1TRACK 3
#define SYNTH2TRACK 4

#define NOTE_START 1
#define NOTE_STOP 2
#define NO_ACTION 0

char sequences[5][8] = {
  {NO_ACTION, NO_ACTION, NO_ACTION, NO_ACTION, NOTE_START, NO_ACTION, NO_ACTION, NOTE_STOP},//drone
  {NOTE_START, NOTE_STOP, NO_ACTION, NO_ACTION, NOTE_START, NOTE_STOP, NO_ACTION, NO_ACTION },//kicks
  {NOTE_STOP, NOTE_START, NOTE_STOP, NOTE_START, NOTE_STOP, NOTE_START, NOTE_START, NOTE_START },//hats
  {NO_ACTION, NOTE_START, NO_ACTION, NOTE_STOP, NO_ACTION, NOTE_START, NO_ACTION, NOTE_STOP }, //synth1
  {NO_ACTION, NO_ACTION, NO_ACTION, NOTE_START, NOTE_STOP, NO_ACTION, NO_ACTION, NO_ACTION}, //synth2
};

void setup() {
  WiFi.mode(WIFI_OFF);
  pinMode(A0, INPUT);
  Serial.begin(115200);

  seq_event.set(125); // 8 bars per sec

  drone_env.setADLevels(220,10);
  drone_env.setTimes(10,100,10,10);
  drone.setTable(TRIANGLE2048_DATA);
  
  synth1_env.setADLevels(220,10);
  synth1_env.setTimes(100,100,100,50);
  synth1.setTable(SMOOTHSQUARE8192_DATA);  
  synth1_filt.setCutoffFreqAndResonance(40,3000);
  
  synth2_env.setADLevels(220,10);
  synth2_env.setTimes(10,50,10,10);
  synth2.setTable(SAW2048_DATA);  

  kick_env.setADLevels(250,10);
  kick_env.setTimes(0,30,20,10);
  kicks.setTable(SMOOTHSQUARE8192_DATA);
  
  hat_env.setADLevels(240,20);
  hat_env.setTimes(0,40,50,50);
  hats.setTable(BROWNNOISE8192_DATA);
  startMozzi(CONTROL_RATE);
}


void playDrone(){
    drone.setFreq(mtof(32+24));
}

uint8_t synth1_notes[] = {44, 52, 32, 37}; 
int last_synth1_note = 0;
void playSynth1(){
    int f = mtof(synth1_notes[last_synth1_note]);    
    synth1.setFreq(f);    
    
    last_synth1_note = (last_synth1_note + 1)%4;
}


uint8_t synth2_notes[] = {52, 67}; 
int last_synth2_note = 0;
void playSynth2(){
    int f = mtof(synth2_notes[last_synth2_note]);    
    synth2.setFreq(f);    
    
    last_synth2_note = (last_synth2_note + 1)%2;
}

void playKick(){
    kicks.setFreq(40);
}

void playHat(){
    hats.setFreq(15000);
}

int curr_seq = 0;
void updateControl() {
  readA0();
  synth1_filt.setCutoffFreqAndResonance(last_a0_val,48000);

  drone_env.update();
  kick_env.update();
  hat_env.update();
  synth1_env.update();
  synth2_env.update();
  
  if(seq_event.ready()){
    if(sequences[DRONETRACK][curr_seq]==NOTE_START){
      playDrone();
      drone_env.noteOn();
    }else if(sequences[DRONETRACK][curr_seq]==NOTE_STOP){
      drone_env.noteOff();
    }

    if(sequences[KICKTRACK][curr_seq]==NOTE_START){
      playKick();
      kick_env.noteOn();
    }else if(sequences[KICKTRACK][curr_seq]==NOTE_STOP){
      kick_env.noteOff();
    }

    if(sequences[HATTRACK][curr_seq]==NOTE_START){
      playHat();
      hat_env.noteOn();
    }else if(sequences[HATTRACK][curr_seq]==NOTE_STOP){
      hat_env.noteOff();
    }

    if(sequences[SYNTH1TRACK][curr_seq]==NOTE_START){
      playSynth1();
      synth1_env.noteOn();
    }else if(sequences[SYNTH1TRACK][curr_seq]==NOTE_STOP){
      synth1_env.noteOff();
    }

    if(sequences[SYNTH2TRACK][curr_seq]==NOTE_START){
      playSynth2();
      synth2_env.noteOn();
    }else if(sequences[SYNTH2TRACK][curr_seq]==NOTE_STOP){
      synth2_env.noteOff();
    }    
    curr_seq=(curr_seq+1)%8;
    seq_event.start();
  }
}


AudioOutput updateAudio(){
  int16_t sample = 0;

  int16_t s1sig = synth1_filt.next(synth1.next()*synth1_env.next())>>2;
  int16_t s2sig = (synth2.next()*synth2_env.next())>>2;
  int16_t dronsig = (drone_env.next() * drone.next())>>2; 
  int16_t dsig = (kick_env.next() * kicks.next())>>2;
  int16_t hsig = (hat_env.next() * hats.next())>>2;
  sample = (dsig + hsig + dronsig + s1sig + s2sig);
  
  return MonoOutput::from16Bit(sample).clip();
}


void loop() {
  audioHook();
}