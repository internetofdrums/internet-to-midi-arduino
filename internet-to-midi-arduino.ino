#include "libraries/base64_arduino/src/base64.hpp"

const unsigned int kLatchPin = 8;
const unsigned int kClockPin = 12;
const unsigned int kDataPin = 11;
const unsigned long kMidiBaudRate = 31250;
const unsigned short int kSecondsPerMinute = 60;
const unsigned short int kMilliSecondsPerSecond = 1000;
const unsigned char kBeatsPerMinute = 120;
const unsigned char kBeatsPerBar = 4;
const unsigned char kSubdivisionsPerBeat = 4;
const unsigned char kSubdivisionsPerBar = kBeatsPerBar * kSubdivisionsPerBeat;
const unsigned char kNumberOfInstruments = 16;
const unsigned char kDataPartsPerBeat = 2;
const unsigned char kLedPins[4] = {13, 12, 8, 7};
const unsigned char kNumberOfLeds = sizeof(kLedPins) / sizeof(*kLedPins);
const signed char kMidiNoteOnCommand = 0x99; // Channel 10 note on
const signed char kMidiNoteOffCommand = 0x89; // Channel 10 note off
const signed char kLowestNote = 0x24; // Key 36 / C1 / Bass Drum 1

unsigned short int kMilliSecondsPerSubdivision = (((float) kSecondsPerMinute / (float) kBeatsPerMinute) * kMilliSecondsPerSecond) / 4;
unsigned int kMilliSecondsPerBar = kSubdivisionsPerBar * kMilliSecondsPerSubdivision;

unsigned long milliSecondsPassed = 0;
unsigned char lastPlayedSubdivision = kSubdivisionsPerBar - 1; // The last subdivision of the bar

unsigned char encodedPattern[] = "f38AAH9/AAAAAH9/AAAAAH9/AAB/fwAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAf38AAAAAAAAAAAAAAAAAAH9/AAAAAAAAAAAAAAAAAAB/fwAAAAAAAAAAAAAAAAAAf38AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAf38AAH9/AAB/fwAAf38AAH9/AAB/fwAAf38AAH9/AAAAAAAAAAB/fwAAAAAAAH9/AAAAAAAAf38AAAAAf38AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA=";
signed char pattern[512];

void setup() {
  Serial.begin(kMidiBaudRate);

  decode_base64(encodedPattern, pattern);
  
  //pinMode(kLatchPin, OUTPUT);
  //pinMode(kClockPin, OUTPUT);
  //pinMode(kDataPin, OUTPUT);
}

void loop() {
  milliSecondsPassed = millis();
  
  if (shouldPlaySubdivision()) {
    playSubdivision();
  }
}

bool shouldPlaySubdivision() {
  unsigned char closestPassedSubdivision = floor((milliSecondsPassed % kMilliSecondsPerBar) / (float) kMilliSecondsPerSubdivision);

  return closestPassedSubdivision != lastPlayedSubdivision;
}

bool shouldShowLed(char velocity) {
  return velocity > 0;
}

void playSubdivision() {
  unsigned char currentSubdivision = (lastPlayedSubdivision + 1) % kSubdivisionsPerBar; 
  unsigned int data = 0;
  
  // Send note-on message for each note, collect LED information
  for (char instrumentIndex = 0; instrumentIndex < kNumberOfInstruments; instrumentIndex++) {
    unsigned int patternIndex = calculatePatternIndex(currentSubdivision, instrumentIndex);
    signed char noteLength = pattern[patternIndex];
    signed char velocity = pattern[patternIndex + 1];
    
    sendMidiNoteOn(instrumentIndex, velocity);
    
    if (shouldShowLed(velocity)) {
      bitSet(data, instrumentIndex);
    }
  }
  
  //showLeds(data);
  
  delay(kMilliSecondsPerSubdivision / 2);
  
  // Send note off message for each note
  for (char instrumentIndex = 0; instrumentIndex < kNumberOfInstruments; instrumentIndex++) {
    unsigned int patternIndex = calculatePatternIndex(currentSubdivision, instrumentIndex);
    signed char noteLength = pattern[patternIndex];
    signed char velocity = pattern[patternIndex + 1];
    
    sendMidiNoteOff(instrumentIndex, velocity);
  }
  
  lastPlayedSubdivision = currentSubdivision;
}

unsigned int calculatePatternIndex(unsigned char currentSubdivision, char instrumentIndex) {
  return (currentSubdivision * kDataPartsPerBeat) + (instrumentIndex * (kNumberOfInstruments * kDataPartsPerBeat));
}

void sendMidiNoteOn(signed char instrumentIndex, signed char velocity) {
  sendMidiMessage(kMidiNoteOnCommand, instrumentIndex, velocity);
}

void sendMidiNoteOff(signed char instrumentIndex, signed char velocity) {
  sendMidiMessage(kMidiNoteOffCommand, instrumentIndex, velocity);
}

void sendMidiMessage(signed char command, signed char instrumentIndex, signed char velocity) {
  Serial.write(command);
  Serial.write(kLowestNote + instrumentIndex);
  Serial.write(velocity);
}

void showLeds(unsigned int data) {
  // Get the first byte of the data
  byte data01 = data & 0xff;
  
  // Get the second byte of the data
  byte data02 = (data >> 8) & 0xff;
  
  digitalWrite(kLatchPin, LOW);
  shiftOut(kDataPin, kClockPin, MSBFIRST, data02);
  shiftOut(kDataPin, kClockPin, MSBFIRST, data01);
  digitalWrite(kLatchPin, HIGH);
}

