#include <SPI.h>
#include <Ethernet.h>
#include "libraries/base64_arduino/src/base64.hpp"

const int kLatchPin = 8;
const int kDataPin = 14; // Pin A0
const int kClockPin = 15; // Pin A1
const int kSetUpLightPin = 5;
const int kServerConnectedLightPin = 6;
const int kPatternDownloadedPin = 7;
const unsigned long kMidiBaudRate = 31250;
const unsigned short int kSecondsPerMinute = 60;
const unsigned short int kMilliSecondsPerSecond = 1000;
const unsigned char kBeatsPerMinute = 120;
const unsigned char kBeatsPerBar = 4;
const unsigned char kSubdivisionsPerBeat = 4;
const unsigned char kSubdivisionsPerBar = kBeatsPerBar * kSubdivisionsPerBeat;
const unsigned char kNumberOfInstruments = 12;
const signed char kMidiNoteOnCommand = 0x99; // Channel 10 note on
const signed char kMidiNoteOffCommand = 0x89; // Channel 10 note off
const signed char kDrumNotes[] = {
  0x24, // Key 36   0x24   Kick
  0x26, // Key 38   0x26   Snare
  0x2B, // Key 43   0x2B   Lo Tom
  0x32, // Key 50   0x32   High Tom
  0x2A, // Key 42   0x2A   Closed Hat
  0x2E, // Key 46   0x2E   Open Hat
  0x27, // Key 39   0x27   Clap
  0x4B, // Key 75   0x4B   Claves
  0x43, // Key 67   0x43   Agogo
  0x31, // Key 49   0x31   Crash
  0x45, // Key 69   0x45   Cabasa
  0x46, // Key 70   0x46   Maracas
  0x47, // Key 71   0x47   Short whistle
  0x48, // Key 72   0x48   Long whistle
  0x49, // Key 73   0x49   Short guiro
  0x4A  // Key 74   0x4A   Long guiro
};
const unsigned int encodedPatternLength = 256;

unsigned short int kMilliSecondsPerSubdivision = (((float) kSecondsPerMinute / (float) kBeatsPerMinute) * kMilliSecondsPerSecond) / 4;
unsigned int kMilliSecondsPerBar = kSubdivisionsPerBar * kMilliSecondsPerSubdivision;

unsigned long milliSecondsPassed = 0;
unsigned char lastPlayedSubdivision = kSubdivisionsPerBar - 1; // The last subdivision of the bar

// Buffer for the encoded pattern
char encodedPattern[encodedPatternLength];

// Buffer for the decoded pattern, initialized with a default pattern
signed char pattern[192] = {
  0x00, 0x00, 0x00, 0x00,  0x00, 0x00, 0x00, 0x00,  0x00, 0x00, 0x00, 0x00,  0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00,  0x00, 0x00, 0x00, 0x00,  0x00, 0x00, 0x00, 0x00,  0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00,  0x00, 0x00, 0x00, 0x00,  0x00, 0x00, 0x00, 0x00,  0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00,  0x00, 0x00, 0x00, 0x00,  0x00, 0x00, 0x00, 0x00,  0x00, 0x00, 0x00, 0x00,
  0x7F, 0x00, 0x00, 0x00,  0x7F, 0x00, 0x00, 0x00,  0x7F, 0x00, 0x00, 0x00,  0x7F, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00,  0x00, 0x00, 0x00, 0x00,  0x00, 0x00, 0x00, 0x00,  0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00,  0x00, 0x00, 0x00, 0x00,  0x00, 0x00, 0x00, 0x00,  0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00,  0x00, 0x00, 0x00, 0x00,  0x00, 0x00, 0x00, 0x00,  0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00,  0x00, 0x00, 0x00, 0x00,  0x00, 0x00, 0x00, 0x00,  0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00,  0x00, 0x00, 0x00, 0x00,  0x00, 0x00, 0x00, 0x00,  0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00,  0x00, 0x00, 0x00, 0x00,  0x00, 0x00, 0x00, 0x00,  0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00,  0x00, 0x00, 0x00, 0x00,  0x00, 0x00, 0x00, 0x00,  0x00, 0x00, 0x00, 0x00,
};

byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
char server[] = "api.internetofdrums.com";
IPAddress ip(192, 168, 1, 177);
IPAddress dns(192, 168, 1, 1);
EthernetClient client;

unsigned long lastConnectionTime = 0;
const unsigned long postingInterval = 5L * 1000L;

unsigned int numberOfControlCharactersInARow = 0;
boolean reachedResponseBody = false;
unsigned int bodyReadIndex = 0;

void setup() {
  Serial.begin(kMidiBaudRate);

  // Set light output ports
  pinMode(kLatchPin, OUTPUT);
  pinMode(kClockPin, OUTPUT);
  pinMode(kDataPin, OUTPUT);

  // Set LED output ports
  pinMode(kSetUpLightPin, OUTPUT);
  pinMode(kServerConnectedLightPin, OUTPUT);
  pinMode(kPatternDownloadedPin, OUTPUT);

  // Show device is setting up
  digitalWrite(kSetUpLightPin, HIGH);

  // Try to connect to internet using DHCP
  if (Ethernet.begin(mac) == 0) {
    // Failed, try connecting with IP address and DNS
    Ethernet.begin(mac, ip, dns);
  }

  // Give the ethernet shield a second to initialize
  delay(1000);

  client.stop();

  if (client.connect(server, 80)) {
    // Show device has connected to server
    digitalWrite(kServerConnectedLightPin, HIGH);
    getNewPattern();
  } else {
    // Show device did not get connection to server
    digitalWrite(kServerConnectedLightPin, LOW);
  }
}

void loop() {
  milliSecondsPassed = millis();

  if (shouldPlaySubdivision()) {
    playSubdivision();
  }

  if (client.available()) {
    // Show device has gotten pattern
    digitalWrite(kPatternDownloadedPin, HIGH);

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
    // Show device is getting pattern
    digitalWrite(kPatternDownloadedPin, LOW);

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
  Serial.write(kDrumNotes[instrumentIndex]);
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
