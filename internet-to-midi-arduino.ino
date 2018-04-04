#include <SPI.h>
#include <Ethernet.h>
#include "libraries/base64_arduino/src/base64.hpp"

const int kLatchPin = 8;
const int kClockPin = 12;
const int kDataPin = 11;
const unsigned long kMidiBaudRate = 31250;
const unsigned short int kSecondsPerMinute = 60;
const unsigned short int kMilliSecondsPerSecond = 1000;
const unsigned char kBeatsPerMinute = 117;
const unsigned char kBeatsPerBar = 4;
const unsigned char kSubdivisionsPerBeat = 4;
const unsigned char kSubdivisionsPerBar = kBeatsPerBar * kSubdivisionsPerBeat;
const unsigned char kNumberOfInstruments = 12;
const unsigned char kLedPins[4] = {13, 12, 8, 7};
const unsigned char kNumberOfLeds = sizeof(kLedPins) / sizeof(*kLedPins);
const signed char kMidiNoteOnCommand = 0x99; // Channel 10 note on
const signed char kMidiNoteOffCommand = 0x89; // Channel 10 note off
const signed char kLowestNote = 0x24; // Key 36 / C1 / Bass Drum 1

unsigned short int kMilliSecondsPerSubdivision = (((float) kSecondsPerMinute / (float) kBeatsPerMinute) * kMilliSecondsPerSecond) / 4;
unsigned int kMilliSecondsPerBar = kSubdivisionsPerBar * kMilliSecondsPerSubdivision;

unsigned long milliSecondsPassed = 0;
unsigned char lastPlayedSubdivision = kSubdivisionsPerBar - 1; // The last subdivision of the bar

unsigned int encodedPatternLength = 256;
char encodedPattern[256];
signed char pattern[192];

byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
char server[] = "api.internetofdrums.com";
IPAddress ip(192, 168, 1, 177);
EthernetClient client;

unsigned long lastConnectionTime = 0;
const unsigned long postingInterval = 5L * 1000L;

unsigned int numberOfControlCharactersInARow = 0;
boolean reachedResponseBody = false;
unsigned int bodyReadIndex = 0;

void setup() {
  Serial.begin(kMidiBaudRate);

  pinMode(kLatchPin, OUTPUT);
  pinMode(kClockPin, OUTPUT);
  pinMode(kDataPin, OUTPUT);

  if (Ethernet.begin(mac) == 0) {
    Ethernet.begin(mac, ip);
  }

  delay(1000);

  client.stop();

  if (client.connect(server, 80)) {
    getNewPattern();
  }
}

void loop() {
  milliSecondsPassed = millis();

  if (shouldPlaySubdivision()) {
    playSubdivision();
  }

  if (client.available()) {
    char c = client.read();

    if (reachedResponseBody) {
      encodedPattern[bodyReadIndex] = c;
      bodyReadIndex++;
    }

    if (c == '\r' || c == '\n') {
      numberOfControlCharactersInARow++;
    } else {
      numberOfControlCharactersInARow = 0;
    }

    if (numberOfControlCharactersInARow == 4) {
      reachedResponseBody = true;
    }

    if (bodyReadIndex == encodedPatternLength) {
      decode_base64(encodedPattern, pattern);
    }
  }

  if (milliSecondsPassed - lastConnectionTime > postingInterval) {
    getNewPattern();
  }
}

void getNewPattern() {
  numberOfControlCharactersInARow = 0;
  reachedResponseBody = false;
  bodyReadIndex = 0;

  client.println("DELETE /1.0/patterns/head/pattern HTTP/1.1");
  client.println("Host: api.internetofdrums.com");
  client.println();

  lastConnectionTime = millis();
}

bool shouldPlaySubdivision() {
  unsigned char closestPassedSubdivision = floor((milliSecondsPassed % kMilliSecondsPerBar) / (float) kMilliSecondsPerSubdivision);

  return closestPassedSubdivision != lastPlayedSubdivision;
}

void playSubdivision() {
  unsigned char currentSubdivision = (lastPlayedSubdivision + 1) % kSubdivisionsPerBar;
  unsigned int data = 0;

  // Send note-on message for each note, collect LED information
  for (char instrumentIndex = 0; instrumentIndex < kNumberOfInstruments; instrumentIndex++) {
    unsigned int patternIndex = (kSubdivisionsPerBar * instrumentIndex) + currentSubdivision;
    signed char velocity = pattern[patternIndex];

    sendMidiNoteOn(instrumentIndex, velocity);

    if (shouldShowLed(velocity)) {
      bitSet(data, instrumentIndex);
    }
  }

  showLeds(data);

  delay(kMilliSecondsPerSubdivision / 8);

  // Send note off message for each note
  for (char instrumentIndex = 0; instrumentIndex < kNumberOfInstruments; instrumentIndex++) {
    unsigned int patternIndex = (kSubdivisionsPerBar * instrumentIndex) + currentSubdivision;
    signed char velocity = pattern[patternIndex];

    sendMidiNoteOff(instrumentIndex, velocity);
  }

  lastPlayedSubdivision = currentSubdivision;
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

bool shouldShowLed(char velocity) {
  return velocity > 0;
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
