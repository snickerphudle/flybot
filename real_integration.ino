#include "arduino_secrets.h"

//import libraries
#include <FastLED.h>
#include <Adafruit_GFX.h>
#include <Adafruit_NeoMatrix.h>
#include <Adafruit_NeoPixel.h>
#include <SoftwareSerial.h>
#include <SD.h>
#include <pcmConfig.h>
#include <pcmRF.h>
#include <SD.h>
#include <TMRpcm.h>
#include <SPI.h>
#ifndef PSTR
#define PSTR
#endif
FASTLED_USING_NAMESPACE

//constants
#define DATA_PIN    13
#define LED_TYPE    WS2812B
#define COLOR_ORDER GRB
#define NUM_LEDS    256
#define VOLTS           4
#define MAX_MA        400
#define BRIGHTNESS         5
#define FRAMES_PER_SECOND  90
#define UPDATES_PER_SECOND 90
#define ARRAY_SIZE(A) (sizeof(A) / sizeof((A)[0]))
#define SD_ChipSelectPin 53

CRGB leds[NUM_LEDS];

//establish matrix width and height
const uint8_t kMatrixWidth = 16;
const uint8_t kMatrixHeight = 16;

// Param for different pixel layouts
const bool    kMatrixSerpentineLayout = true;
const bool    kMatrixVertical = false;
bool isFastLED = false;

//Initialization for Adafruit Matrix
Adafruit_NeoMatrix matrix = Adafruit_NeoMatrix(32, 8, DATA_PIN,
  NEO_MATRIX_TOP     + NEO_MATRIX_LEFT +
  NEO_MATRIX_COLUMNS + NEO_MATRIX_ZIGZAG,
  NEO_GRB            + NEO_KHZ800);

//Red and Gold, Theta Tau's colors
const uint16_t colors[] = {
  matrix.Color(215,156,0), matrix.Color(103,0,1)
};

//Motion sensor pins
long duration;
long distance;
int safetyDistance;
const int trigPin = 24;
const int echoPin = 26;
bool isCongrats = false;
int prevTime;
int currTime;

//Set up sound
TMRpcm left;
TMRpcm right;
const char* songs[8] = {"abba.wav", "zedd.wav", "shut_up.wav", "down.wav", "katy.wav", "hold.wav", "kanye.wav", "wap.wav"};
const char* soundEffects[7] = {"cele.wav", "slot.wav", "cheer.wav", "rick.wav", "trumpet.wav", "star.wav", "bark.wav"};
int currSoundEffect = 0;
int currSong = 0;

//setup bluetooth
SoftwareSerial mySerial(11, 10);
String previousCommand = "";

void setup() {
  delay(3000); // 3 second delay for recovery
  
  //begin serial communication
  Serial.begin(9600);
  mySerial.begin(9600);
  
  // initialize matrix
  matrix.begin();
  matrix.setTextWrap(false);
  matrix.setBrightness(50);
  matrix.setTextColor(colors[0]);
  
  // tell FastLED about the LED strip configuration
  FastLED.addLeds<LED_TYPE,DATA_PIN,COLOR_ORDER>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip);
  
  // Set max voltage and current
  FastLED.setMaxPowerInVoltsAndMilliamps( VOLTS, MAX_MA);
  
  // set master brightness control
  FastLED.setBrightness(BRIGHTNESS);
  
  // Initialize sensor pins
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);
  
  //Initialize speaker pins
  pinMode(5, OUTPUT);
  pinMode(6, OUTPUT);
  
  //assigns pin 5 and 6 to the speaker
  left.speakerPin=5;
  right.speakerPin=6;
  
  //test SD card communications
  if(!SD.begin(SD_ChipSelectPin)) {
    Serial.println("SD fail");
    return;
  }
  
  //if SD card connection works, print success
  Serial.println("Success");
  
  //establishes the volume for the speaker
  left.setVolume(5);
  right.setVolume(5);
}

//Initialize variables for LED matrix
int x    = matrix.width(); //used to control LED text printing
int pass = 0; //used to control LED text printing
uint8_t gCurrentPatternNumber = 0; // Index number of which pattern is current
uint8_t gHue = 0; // rotating "base color" used by many of the patterns

// List of patterns to cycle through.  Each is defined as a separate function below. cylon
typedef void (*SimplePatternList[])();
SimplePatternList gPatterns = {theta_tau, rainbow_rave, confetti, sinelon, juggle, bpm};
// SimplePatternList gPatterns = {theta_tau, rainbow_rave};
  
void loop()
{
  
  action(readBluetooth());
  delay(0.05);
  
  if (isMotion()) {
    congrats();
    isCongrats = true;
  } else if (isCongrats) {
      x = matrix.width();
      isCongrats = false;
  } else {
    // Call the current pattern function once, updating the 'leds' array, || gCurrentPatternNumber == ARRAY_SIZE(gPatterns)
    if (gCurrentPatternNumber == 0) {
      if (isFastLED) {
        FastLED.clear();
        isFastLED = false;
      }
      theta_tau();
    } else {
      gPatterns[gCurrentPatternNumber]();
      
      // send the 'leds' array out to the actual LED strip
      FastLED.show();  
      
      // insert a delay to keep the framerate modest
      FastLED.delay(1000/FRAMES_PER_SECOND); 
  
      // do some periodic updates
      EVERY_N_MILLISECONDS( 20 ) { gHue++; } // slowly cycle the "base color" through the rainbow
      isFastLED = true;
    }
    EVERY_N_SECONDS(11) { nextPattern(); } // change patterns periodically
  }
}

//plays the next pattern on the LED strip
void nextPattern()
{
  // add one to the current pattern number, and wrap around at the end
  gCurrentPatternNumber = (gCurrentPatternNumber + 1) % ARRAY_SIZE(gPatterns);
  if (gCurrentPatternNumber == ARRAY_SIZE(gPatterns) - 1) {
    FastLED.clear(true);
  }
}

void confetti() 
{
  // random colored speckles that blink in and fade smoothly
  fadeToBlackBy( leds, NUM_LEDS, 10);
  int pos = random16(NUM_LEDS);
  leds[pos] += CHSV( gHue + random8(64), 200, 255);
}

void sinelon()
{
  // a colored dot sweeping back and forth, with fading trails
  fadeToBlackBy( leds, NUM_LEDS, 20);
  int pos = beatsin16( 13, 0, NUM_LEDS-1 );
  leds[pos] += CHSV( gHue, 255, 192);
}

void bpm()
{
  // colored stripes pulsing at a defined Beats-Per-Minute (BPM)
  uint8_t BeatsPerMinute = 62;
  CRGBPalette16 palette = PartyColors_p;
  uint8_t beat = beatsin8( BeatsPerMinute, 64, 255);
  for( int i = 0; i < NUM_LEDS; i++) { //9948
    leds[i] = ColorFromPalette(palette, gHue+(i*2), beat-gHue+(i*10));
  }
}

void juggle() {
  // eight colored dots, weaving in and out of sync with each other
  fadeToBlackBy( leds, NUM_LEDS, 20);
  uint8_t dothue = 0;
  for( int i = 0; i < 8; i++) {
    leds[beatsin16( i+7, 0, NUM_LEDS-1 )] |= CHSV(dothue, 200, 255);
    dothue += 32;
  }
}

void fadeall() { for(int i = 0; i < NUM_LEDS; i++) { leds[i].nscale8(250); } }

uint16_t XY( uint8_t x, uint8_t y)
{
  uint16_t i;
  
  if( kMatrixSerpentineLayout == false) {
    if (kMatrixVertical == false) {
      i = (y * kMatrixWidth) + x;
    } else {
      i = kMatrixHeight * (kMatrixWidth - (x+1))+y;
    }
  }

  if( kMatrixSerpentineLayout == true) {
    if (kMatrixVertical == false) {
      if( y & 0x01) {
        // Odd rows run backwards
        uint8_t reverseX = (kMatrixWidth - 1) - x;
        i = (y * kMatrixWidth) + reverseX;
      } else {
        // Even rows run forwards
        i = (y * kMatrixWidth) + x;
      }
    } else { // vertical positioning
      if ( x & 0x01) {
        i = kMatrixHeight * (kMatrixWidth - (x+1))+y;
      } else {
        i = kMatrixHeight * (kMatrixWidth - x) - (y+1);
      }
    }
  }
  
  return i;
}

void DrawOneFrame( uint8_t startHue8, int8_t yHueDelta8, int8_t xHueDelta8)
{
  uint8_t lineStartHue = startHue8;
  for( uint8_t y = 0; y < kMatrixHeight; y++) {
    lineStartHue += yHueDelta8;
    uint8_t pixelHue = lineStartHue;      
    for( uint8_t x = 0; x < kMatrixWidth; x++) {
      pixelHue += xHueDelta8;
      leds[ XY(x, y)]  = CHSV( pixelHue, 255, 255);
    }
  }
}

void rainbow_rave()
{
    uint32_t ms = millis();
    int32_t yHueDelta32 = ((int32_t)cos16( ms * (27/1) ) * (350 / kMatrixWidth));
    int32_t xHueDelta32 = ((int32_t)cos16( ms * (39/1) ) * (310 / kMatrixHeight));
    DrawOneFrame( ms / 65536, yHueDelta32 / 32768, xHueDelta32 / 32768);
    if( ms < 5000 ) {
      FastLED.setBrightness( scale8( BRIGHTNESS, (ms * 256) / 5000));
    } else {
      FastLED.setBrightness(BRIGHTNESS);
    }
    FastLED.show();
}

//Displays Rush Theta Tau
void theta_tau()
{
  matrix.fillScreen(0);
  matrix.setCursor(x, 0);
  uint16_t otherColor;
  if (pass == 0) {
    otherColor = colors[1];
  } else {
    otherColor = colors[0];
  }
  matrix.fillScreen(otherColor);
  matrix.print(F("Rush Theta Tau!"));
  if(--x < -60) {
    x = matrix.width();
    if(++pass >= 2) pass = 0;
    matrix.setTextColor(colors[pass]);
  }
  matrix.show();
  delay(30);
}

//returns true if motion is detected on the ultrasonic sensor
bool isMotion() {
  // Clears the trigPin
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  
  // Sets the trigPin on HIGH state for 15 micro seconds
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);
  
  // Reads the echoPin, returns the sound wave travel time in microseconds
  duration = pulseIn(echoPin, HIGH);
  
  // Calculate the distance
  distance= duration*0.034/2;
  
  safetyDistance = distance;
  if (safetyDistance <= 12){
    return true;
  } else {
    return false;
  }
}

//displays 'You made it!' on the LED matrix
void congrats() {
  playSoundEffect();
  FastLED.clear();
  x = matrix.width();
  for (int i = 0; i < 160; i++) {
    matrix.fillScreen(0);
    matrix.setCursor(x, 0);
    uint16_t otherColor;
    if (pass == 0) {
      otherColor = colors[1];
    } else {
      otherColor = colors[0];
    }
    matrix.fillScreen(otherColor);
    matrix.print(F("Congrats! You made it!"));
    if(--x < -160) {
      x = matrix.width();
      if(++pass >= 2) pass = 0;
      matrix.setTextColor(colors[pass]);
    }
    matrix.show();
    delay(30);
  }
}

//displays text on LED matrix
void displayText(String text) {
  FastLED.clear();
  int tileLength = text.length() * 8 + 32;
  x = matrix.width();
  
  for (int i = 0; i < tileLength; i++) {
    matrix.fillScreen(0);
    matrix.setCursor(x, 0);
    uint16_t otherColor;
    if (pass == 0) {
      otherColor = colors[1];
    } else {
      otherColor = colors[0];
    }
    matrix.fillScreen(otherColor);
    matrix.print(text);
    if(--x < -tileLength) {
      x = matrix.width();
      if(++pass >= 2) pass = 0;
      matrix.setTextColor(colors[pass]);
    }
    matrix.show();
    delay(50);
  }
}

//Plays the given audio file. Does not iterate the current sound cycle.
void playAudio(char* fileName) {
  left.play(fileName);
  right.play(fileName);
}

//plays the current sound effect and increments the sound counter to cycle to the next sound
void playSoundEffect() {
  left.play(soundEffects[currSoundEffect]);
  right.play(soundEffects[currSoundEffect]);
  currSoundEffect = (1 + currSoundEffect) % ARRAY_SIZE(soundEffects);
}

//Checks the bluetooth device for a signal and returns a string based on the value entered
String readBluetooth() {
  String newCommand = "";
  char c;
  
  while (mySerial.available()) {
    c = mySerial.read();
    newCommand += c;
  }
  
  if (previousCommand != newCommand) {
    previousCommand = newCommand;
    return newCommand;
  }
  
  return "";
}

void action(String command) {
  Serial.println(command);
  if (command == "") {
    // Serial.println("No change. Exiting.");
    return;
  } else if (command == "s0") {
    left.disable();
    right.disable();
  } else if (command == "s1") {
    playAudio(songs[0]);
  } else if (command == "s2") {
    playAudio(songs[1]);
  } else if (command == "s3") {
    playAudio(songs[2]);
  } else if (command == "s4") {
    playAudio(songs[3]);
  } else if (command == "s5") {
    playAudio(songs[4]);
  } else if (command == "s6") {
    playAudio(songs[5]);
  } else if (command == "s7") {
    playAudio(songs[6]);
  } else if (command == "e1") {
    playAudio(soundEffects[0]);
  } else if (command == "e2") {
    playAudio(soundEffects[1]);
  } else if (command == "e3") {
    playAudio(soundEffects[2]);
  } else if (command == "e4") {
    playAudio(soundEffects[3]);
  } else if (command == "e5") {
    playAudio(soundEffects[4]);
  } else if (command == "e6") {
    playAudio(soundEffects[5]);
  } else if (command == "e7") {
    playAudio(soundEffects[6]);
  } else {
    displayText(command);
    return;
  }
}