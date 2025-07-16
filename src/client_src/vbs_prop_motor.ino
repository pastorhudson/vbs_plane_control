#include <Adafruit_NeoPixel.h>
#include <esp_now.h>
#include <WiFi.h>

// Pin definitions
const int LEDPIN = 15;
const int propStepPin = 3;
const int propDirPin = 6;
const int weightStepPin = 7;
const int weightDirPin = 9;
const int limitLeftPin = 18; // Yellow Wire
const int limitRightPin = 33; //Blue Wire
const int neoPixelPin = 5; // Change this to your preferred pin

// Globals
bool propRunning = true;
int currentRPM = 30; // Default RPM


// NeoPixel setup
const int numPixels = 4;
Adafruit_NeoPixel strip(numPixels, neoPixelPin, NEO_GRB + NEO_KHZ800);

// Motor settings
const int stepsPerRevolution = 1600; // Full steps for propeller
const int rpm = 30;

// Motion control variables
unsigned long lastMoveTime = 0;
unsigned long pauseDuration = 0;
bool movingToEdge = true;
bool movingRight = true;
bool movingToCenter = false;
bool waiting = false;

// NeoPixel control variables
unsigned long lastPixelUpdate = 0;
int currentPattern = 0;
int patternStep = 0;
const int pixelUpdateInterval = 150; // Slower for auditorium viewing
const int strobeInterval = 80; // Slightly slower strobe for better visibility
const int navigationInterval = 1000; // Navigation light timing



void setup() {
  // Pin setup
  pinMode(LEDPIN, OUTPUT);
  digitalWrite(LEDPIN, HIGH);
  pinMode(propStepPin, OUTPUT);
  pinMode(propDirPin, OUTPUT);
  pinMode(weightStepPin, OUTPUT);
  pinMode(weightDirPin, OUTPUT);
  pinMode(limitLeftPin, INPUT_PULLUP);
  pinMode(limitRightPin, INPUT_PULLUP);
  setupESPNow();
  digitalWrite(propDirPin, HIGH); // Clockwise
  delay(10);
  randomSeed(analogRead(0)); // Seed random with noise
  
  // NeoPixel setup
  strip.begin();
  strip.show(); // Initialize all pixels to 'off'
  strip.setBrightness(80); // Higher brightness for auditorium visibility
  // Serial.begin(115200);
  WiFi.mode(WIFI_STA); // Required for esp_now
  // Serial.println("MAC Address:");
  // Serial.println(WiFi.macAddress());

}

void loop() {
  spinPropeller();
  handleWeightMotion();
  updateNeoPixels();
}

void OnDataRecv(const esp_now_recv_info_t *info, const uint8_t *incomingData, int len) {
  String command = String((char*)incomingData);
  
  if (command == "PROP_START") {
    propRunning = true;
  } else if (command == "PROP_STOP") {
    propRunning = false;
  } else if (command == "WEIGHT_START") {
    waiting = false;
    movingToEdge = true;
  } else if (command == "WEIGHT_STOP") {
    waiting = true;
  } else if (command.startsWith("RPM:")) {
    int newRPM = command.substring(4).toInt();
    if (newRPM > 0 && newRPM <= 300) {
      currentRPM = newRPM;
    }
  } else if (command.startsWith("PATTERN:")) {
    currentPattern = command.substring(8).toInt();
    patternStep = 0;
  }
}


void setupESPNow() {
  WiFi.mode(WIFI_STA);
  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW init failed");
    return;
  }
  esp_now_register_recv_cb(OnDataRecv);
}


// ------------------- PROP -------------------
void spinPropeller() {
  if (!propRunning) return;

  float stepsPerSecond = (stepsPerRevolution * currentRPM) / 60.0;
  float delayMicros = 1e6 / stepsPerSecond;
  digitalWrite(propStepPin, HIGH);
  delayMicroseconds(delayMicros / 2);
  digitalWrite(propStepPin, LOW);
  delayMicroseconds(delayMicros / 2);
}


// ----------------- WEIGHT ------------------
void handleWeightMotion() {
  if (waiting) {
    if (millis() - lastMoveTime >= pauseDuration) {
      waiting = false;
      movingToEdge = !movingToEdge;
      if (!movingToEdge) movingToCenter = true;
      // Change pattern when motion changes
      currentPattern = random(0, 9); // Updated for airplane patterns
      patternStep = 0;
    } else {
      return;
    }
  }
  
  if (movingToEdge) {
    if (movingRight) {
      digitalWrite(weightDirPin, HIGH);
      digitalWrite(weightStepPin, HIGH);
      delayMicroseconds(25);
      digitalWrite(weightStepPin, LOW);
      delayMicroseconds(25);
      if (digitalRead(limitRightPin) == LOW) {
        startPause();
      }
    } else {
      digitalWrite(weightDirPin, LOW);
      digitalWrite(weightStepPin, HIGH);
      delayMicroseconds(25);
      digitalWrite(weightStepPin, LOW);
      delayMicroseconds(25);
      if (digitalRead(limitLeftPin) == LOW) {
        startPause();
      }
    }
  } else if (movingToCenter) {
    // Always move to center from either side: assume ~100 steps
    static int centerSteps = 0;
    digitalWrite(weightDirPin, movingRight ? LOW : HIGH); // Reverse direction
    digitalWrite(weightStepPin, HIGH);
    delayMicroseconds(25);
    digitalWrite(weightStepPin, LOW);
    delayMicroseconds(25);
    centerSteps++;
    if (centerSteps >= 42000) { // Adjust to match your track's center
      centerSteps = 0;
      movingToCenter = false;
      movingRight = random(0, 2); // Random next direction
      startPause();
    }
  }
}

void startPause() {
  waiting = true;
  lastMoveTime = millis();
  pauseDuration = random(3000, 8000); // 3–8 seconds
  // Use special patterns during pause - navigation or beacon
  currentPattern = random(0, 2) == 0 ? 0 : 7; // Navigation or beacon
  patternStep = 0;
}

// ----------------- NEOPIXELS ------------------
void updateNeoPixels() {
  // Use different update intervals for different patterns
  int updateInterval = pixelUpdateInterval;
  if (currentPattern == 5 || currentPattern == 6) updateInterval = strobeInterval;
  if (currentPattern == 7 || currentPattern == 8) updateInterval = navigationInterval;
  
  if (millis() - lastPixelUpdate < updateInterval) {
    return; // Don't update too frequently
  }
  
  lastPixelUpdate = millis();
  
  switch (currentPattern) {
    case 0:
      airplaneNavigationLights();
      break;
    case 1:
      landingLights(); // Bright white forward lights
      break;
    case 2:
      wingTipStrobes(); // Alternating wing tip strobes
      break;
    case 3:
      slowPulseBlue(); // Calm blue pulse
      break;
    case 4:
      redGreenAlternate(); // Traditional nav colors
      break;
    case 5:
      emergencyStrobe(); // Bright white emergency strobe
      break;
    case 6:
      colorfulStrobe(); // Party strobe for VBS fun
      break;
    case 7:
      beaconRotation(); // Rotating beacon effect
      break;
    case 8:
      runwayLights(); // Sequential runway-style lights
      break;
    default:
      airplaneNavigationLights();
      break;
  }
  
  patternStep++;
  if (patternStep > 255) patternStep = 0; // Reset for cycling patterns
}

// Rainbow cycle pattern
void rainbowCycle() {
  for (int i = 0; i < numPixels; i++) {
    strip.setPixelColor(i, wheel(((i * 256 / numPixels) + patternStep) & 255));
  }
  strip.show();
}

// Theater chase pattern
void theaterChase(uint32_t color) {
  for (int i = 0; i < numPixels; i++) {
    if ((i + patternStep) % 3 == 0) {
      strip.setPixelColor(i, color);
    } else {
      strip.setPixelColor(i, 0);
    }
  }
  strip.show();
}

// Color wipe pattern
void colorWipe(uint32_t color) {
  int pixelOn = patternStep % (numPixels * 2);
  if (pixelOn < numPixels) {
    strip.setPixelColor(pixelOn, color);
  } else {
    strip.setPixelColor((numPixels * 2 - 1) - pixelOn, 0);
  }
  strip.show();
}

// Breathing effect
void breathingEffect(uint32_t color) {
  float brightness = (sin(patternStep * 0.1) + 1.0) / 2.0; // 0.0 to 1.0
  uint8_t r = (uint8_t)((color >> 16 & 0xFF) * brightness);
  uint8_t g = (uint8_t)((color >> 8 & 0xFF) * brightness);
  uint8_t b = (uint8_t)((color & 0xFF) * brightness);
  
  for (int i = 0; i < numPixels; i++) {
    strip.setPixelColor(i, strip.Color(r, g, b));
  }
  strip.show();
}

// Solid color
void solidColor(uint32_t color) {
  for (int i = 0; i < numPixels; i++) {
    strip.setPixelColor(i, color);
  }
  strip.show();
}

// ----------------- AIRPLANE THEMED NEOPIXELS ------------------

// Realistic airplane navigation lights (red/green/white)
void airplaneNavigationLights() {
  // Wing tips: Red on left (pixel 0), Green on right (pixel 1)
  // Tail: White strobe (pixel 2), Belly: White steady (pixel 3)
  strip.setPixelColor(0, strip.Color(255, 0, 0));   // Left wing - Red
  strip.setPixelColor(1, strip.Color(0, 255, 0));   // Right wing - Green
  strip.setPixelColor(3, strip.Color(255, 255, 255)); // Belly - White steady
  
  // Tail beacon - white flash every 2 seconds
  if ((patternStep / 10) % 2 == 0) {
    strip.setPixelColor(2, strip.Color(255, 255, 255)); // Tail - White flash
  } else {
    strip.setPixelColor(2, strip.Color(0, 0, 0));
  }
  strip.show();
}

// Bright landing lights
void landingLights() {
  uint32_t white = strip.Color(255, 255, 255);
  for (int i = 0; i < numPixels; i++) {
    strip.setPixelColor(i, white);
  }
  strip.show();
}

// Wing tip strobes (alternating)
void wingTipStrobes() {
  if (patternStep % 4 < 2) {
    // Flash wing tips
    strip.setPixelColor(0, strip.Color(255, 255, 255)); // Left wing
    strip.setPixelColor(1, strip.Color(255, 255, 255)); // Right wing
    strip.setPixelColor(2, strip.Color(0, 0, 0));       // Tail off
    strip.setPixelColor(3, strip.Color(0, 0, 0));       // Belly off
  } else {
    // Flash tail and belly
    strip.setPixelColor(0, strip.Color(0, 0, 0));       // Wings off
    strip.setPixelColor(1, strip.Color(0, 0, 0));
    strip.setPixelColor(2, strip.Color(255, 255, 255)); // Tail
    strip.setPixelColor(3, strip.Color(255, 255, 255)); // Belly
  }
  strip.show();
}

// Slow blue pulse (like altitude indicator)
void slowPulseBlue() {
  float brightness = (sin(patternStep * 0.05) + 1.0) / 2.0;
  uint8_t blue = (uint8_t)(255 * brightness);
  for (int i = 0; i < numPixels; i++) {
    strip.setPixelColor(i, strip.Color(0, 0, blue));
  }
  strip.show();
}

// Traditional red/green alternating
void redGreenAlternate() {
  if ((patternStep / 5) % 2 == 0) {
    for (int i = 0; i < numPixels; i++) {
      strip.setPixelColor(i, strip.Color(255, 0, 0)); // Red
    }
  } else {
    for (int i = 0; i < numPixels; i++) {
      strip.setPixelColor(i, strip.Color(0, 255, 0)); // Green
    }
  }
  strip.show();
}

// Emergency strobe - bright white
void emergencyStrobe() {
  if (patternStep % 2 == 0) {
    for (int i = 0; i < numPixels; i++) {
      strip.setPixelColor(i, strip.Color(255, 255, 255));
    }
  } else {
    for (int i = 0; i < numPixels; i++) {
      strip.setPixelColor(i, strip.Color(0, 0, 0));
    }
  }
  strip.show();
}

// Colorful strobe for VBS fun
void colorfulStrobe() {
  if (patternStep % 2 == 0) {
    // Cycle through fun colors
    uint32_t colors[] = {
      strip.Color(255, 0, 0),   // Red
      strip.Color(0, 255, 0),   // Green
      strip.Color(0, 0, 255),   // Blue
      strip.Color(255, 255, 0), // Yellow
      strip.Color(255, 0, 255), // Magenta
      strip.Color(0, 255, 255)  // Cyan
    };
    uint32_t color = colors[(patternStep / 2) % 6];
    for (int i = 0; i < numPixels; i++) {
      strip.setPixelColor(i, color);
    }
  } else {
    for (int i = 0; i < numPixels; i++) {
      strip.setPixelColor(i, strip.Color(0, 0, 0));
    }
  }
  strip.show();
}

// Rotating beacon effect
void beaconRotation() {
  // Clear all pixels
  for (int i = 0; i < numPixels; i++) {
    strip.setPixelColor(i, strip.Color(0, 0, 0));
  }
  
  // Light up one pixel at a time in rotation
  int activePixel = (patternStep / 3) % numPixels;
  strip.setPixelColor(activePixel, strip.Color(255, 100, 0)); // Orange beacon
  strip.show();
}

// Sequential runway lights
void runwayLights() {
  // Clear all first
  for (int i = 0; i < numPixels; i++) {
    strip.setPixelColor(i, strip.Color(0, 0, 0));
  }
  
  // Light up pixels in sequence
  int sequence = (patternStep / 4) % (numPixels + 2);
  if (sequence < numPixels) {
    strip.setPixelColor(sequence, strip.Color(255, 255, 255));
    if (sequence > 0) {
      strip.setPixelColor(sequence - 1, strip.Color(100, 100, 100)); // Dim trail
    }
  }
  strip.show();
}

// Keep original utility functions for compatibility

// Utility function to create rainbow colors
uint32_t wheel(byte wheelPos) {
  wheelPos = 255 - wheelPos;
  if (wheelPos < 85) {
    return strip.Color(255 - wheelPos * 3, 0, wheelPos * 3);
  }
  if (wheelPos < 170) {
    wheelPos -= 85;
    return strip.Color(0, wheelPos * 3, 255 - wheelPos * 3);
  }
  wheelPos -= 170;
  return strip.Color(wheelPos * 3, 255 - wheelPos * 3, 0);
}