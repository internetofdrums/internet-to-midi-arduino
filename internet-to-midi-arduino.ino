#include <SPI.h>
#include <Ethernet.h>
#include "libraries/base64_arduino/src/base64.hpp"

const int kLatchPin = 8;
const int kDataPin = 14; // Pin A0
const int kClockPin = 15; // Pin A1
const int kSetUpLightPin = 5;
const int kServerConnectedLightPin = 6;
const int kPatternDownloadedPin = 7;
const int kBLinkDelayInMilliseconds = 150;
const int kRingAnimationDelayInMilliseconds = 75;
const unsigned long kMidiBaudRate = 31250;
const unsigned short int kSecondsPerMinute = 60;
const unsigned short int kMilliSecondsPerSecond = 1000;
const unsigned char kBeatsPerMinute = 120;
const unsigned char kBeatsPerBar = 4;
const unsigned char kSubdivisionsPerBeat = 4;
const unsigned char kSubdivisionsPerBar = kBeatsPerBar * kSubdivisionsPerBeat;
const unsigned char kNumberOfInstruments = 12;
const unsigned char kNumberOfRingLeds = 16;
const signed char kMidiNoteOnCommand = 0x99; // Channel 10 note on
const signed char kMidiNoteOffCommand = 0x89; // Channel 10 note off
const signed char kDrumNotes[] = {
  0x24, // Key 36: Kick
  0x26, // Key 38: Snare
  0x2B, // Key 43: Lo Tom
  0x32, // Key 50: High Tom
  0x2A, // Key 42: Closed Hat
  0x2E, // Key 46: Open Hat
  0x27, // Key 39: Clap
  0x4B, // Key 75: Claves
  0x43, // Key 67: Agogo
  0x31, // Key 49: Crash
  0x45, // Key 69: Cabasa
  0x46, // Key 70: Maracas
  0x47, // Key 71: Short whistle
  0x48, // Key 72: Long whistle
  0x49, // Key 73: Short guiro
  0x4A  // Key 74: Long guiro
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
  0x00, 0x00, 0x00, 0x00,  0x00, 0x00, 0x00, 0x00,  0x00, 0x00, 0x00, 0x00,  0x00, 0x00, 0x00, 0x00
};

byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
char server[] = "api.internetofdrums.com";
IPAddress ip(192, 168, 1, 177);
IPAddress dns(192, 168, 1, 1);
EthernetClient client;

unsigned long lastConnectionTime = 0;
const unsigned long pollingInterval = 8L * 1000L;

unsigned int numberOfControlCharactersInARow = 0;
boolean reachedResponseBody = false;
unsigned int bodyReadIndex = 0;

void setup() {
  Serial.begin(kMidiBaudRate);

  // Setup light outputs
  pinMode(kLatchPin, OUTPUT);
  pinMode(kClockPin, OUTPUT);
  pinMode(kDataPin, OUTPUT);
  pinMode(kSetUpLightPin, OUTPUT);
  pinMode(kServerConnectedLightPin, OUTPUT);
  pinMode(kPatternDownloadedPin, OUTPUT);

  switchSetupLightOn();
  blinkServerConnectedLight(5);

  // Try to connect to internet using DHCP
  if (Ethernet.begin(mac) == 0) {
    if (Ethernet.hardwareStatus() == EthernetNoHardware || Ethernet.linkStatus() == LinkOFF) {
      blinkServerConnectedLight(100);
      exit(0);
    }

    // Failed, try connecting with IP address and DNS
    Ethernet.begin(mac, ip, dns);
  } else {
    switchServerConnectedLightOn();
  }

  // Give the ethernet shield a second to initialize
  showLedRingAnimation();

  // Get the first pattern
  getNewPattern();
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

    switchPatternDownloadedLightOff();
  }

  if (milliSecondsPassed - lastConnectionTime > pollingInterval) {
    switchPatternDownloadedLightOn();
    getNewPattern();
  }
}

void getNewPattern() {
  numberOfControlCharactersInARow = 0;
  reachedResponseBody = false;
  bodyReadIndex = 0;

  client.stop();

  if (client.connect(server, 80)) {
    client.println("DELETE /1.0/patterns/head/pattern HTTP/1.1");
    client.println("Host: api.internetofdrums.com");
    client.println();
  } else {
    blinkPatternDownloadedLight(2);
    getNewPattern();
  }

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

void showLedRingAnimation() {
  unsigned int data = 0;

  for (char ledIndex = 0; ledIndex < kNumberOfRingLeds; ledIndex++) {
    bitSet(data, ledIndex);
    showLeds(data);
    delay(kRingAnimationDelayInMilliseconds);
  }

  showLeds(0);
}

void switchSetupLightOn() {
  digitalWrite(kSetUpLightPin, HIGH);
}

void switchServerConnectedLightOn() {
  digitalWrite(kServerConnectedLightPin, HIGH);
}

void switchPatternDownloadedLightOn() {
  digitalWrite(kPatternDownloadedPin, HIGH);
}

void switchServerConnectedLightOff() {
  digitalWrite(kServerConnectedLightPin, LOW);
}

void switchPatternDownloadedLightOff() {
  digitalWrite(kPatternDownloadedPin, LOW);
}

void blinkServerConnectedLight(int numberOfBlinks) {
  for (char blinkIndex = 0; blinkIndex < numberOfBlinks; blinkIndex++) {
    switchServerConnectedLightOn();
    delay(kBLinkDelayInMilliseconds);
    switchServerConnectedLightOff();
    delay(kBLinkDelayInMilliseconds);
  }
}

void blinkPatternDownloadedLight(int numberOfBlinks) {
  for (char blinkIndex = 0; blinkIndex < numberOfBlinks; blinkIndex++) {
    switchPatternDownloadedLightOn();
    delay(kBLinkDelayInMilliseconds);
    switchPatternDownloadedLightOff();
    delay(kBLinkDelayInMilliseconds);
  }
}
