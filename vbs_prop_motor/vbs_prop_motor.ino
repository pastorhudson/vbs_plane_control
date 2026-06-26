#include <esp_now.h>
#include <WiFi.h>
#include <Adafruit_NeoPixel.h>

// Pin definitions
const int LEDPIN = 15;
const int propControlPin = 3; // Connected to IRFZ44n gate
const int weightStepPin = 7;
const int weightDirPin = 9;
const int limitLeftPin = 18; // Yellow Wire
const int limitRightPin = 33; //Blue Wire
const int NEOPIXEL_PIN = 5;
const int NEOPIXEL_COUNT = 4;

// NeoPixel setup
Adafruit_NeoPixel pixels(NEOPIXEL_COUNT, NEOPIXEL_PIN, NEO_GRB + NEO_KHZ800);

// Globals
bool propRunning = true;

// Motion control variables
unsigned long lastMoveTime = 0;
unsigned long pauseDuration = 0;
bool movingToEdge = true;
bool movingRight = true;
bool movingToCenter = false;
bool waiting = false;

// Light control variables
bool navigationLights = true;
bool landingLights = true;
unsigned long lastBlinkTime = 0;
bool blinkState = false;
const unsigned long BLINK_INTERVAL = 500; // 500ms blink interval

// Light colors (RGB)
const uint32_t RED = pixels.Color(255, 0, 0);
const uint32_t GREEN = pixels.Color(0, 255, 0);
const uint32_t WHITE = pixels.Color(255, 255, 255);
const uint32_t OFF = pixels.Color(0, 0, 0);

void setup() {
  // Pin setup
  pinMode(LEDPIN, OUTPUT);
  digitalWrite(LEDPIN, HIGH);
  pinMode(propControlPin, OUTPUT);
  digitalWrite(propControlPin, HIGH); // Start with prop on
  pinMode(weightStepPin, OUTPUT);
  pinMode(weightDirPin, OUTPUT);
  pinMode(limitLeftPin, INPUT_PULLUP);
  pinMode(limitRightPin, INPUT_PULLUP);
  
  // Initialize NeoPixels
  pixels.begin();
  pixels.show(); // Initialize all pixels to 'off'
  
  setupESPNow();
  delay(10);
  randomSeed(analogRead(0)); // Seed random with noise
 
  // Serial.begin(115200);
  WiFi.mode(WIFI_STA); // Required for esp_now
  // Serial.println("MAC Address:");
  // Serial.println(WiFi.macAddress());
}

void loop() {
  controlPropeller();
  handleWeightMotion();
  updateNavigationLights();
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
  } else if (command == "NAV_ON") {
    navigationLights = true;
  } else if (command == "NAV_OFF") {
    navigationLights = false;
  } else if (command == "LANDING_ON") {
    landingLights = true;
  } else if (command == "LANDING_OFF") {
    landingLights = false;
  } else if (command == "LIGHTS_ALL_ON") {
    navigationLights = true;
    landingLights = true;
  } else if (command == "LIGHTS_ALL_OFF") {
    navigationLights = false;
    landingLights = false;
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
void controlPropeller() {
  if (propRunning) {
    digitalWrite(propControlPin, HIGH); // Turn on MOSFET (prop on)
  } else {
    digitalWrite(propControlPin, LOW);  // Turn off MOSFET (prop off)
  }
}

// ------------------- LIGHTS -------------------
void updateNavigationLights() {
  // Handle blinking timing
  if (millis() - lastBlinkTime >= BLINK_INTERVAL) {
    lastBlinkTime = millis();
    blinkState = !blinkState;
  }
  
  if (!navigationLights && !landingLights) {
    // All lights off
    pixels.clear();
    pixels.show();
    return;
  }
  
  // Standard aircraft navigation light configuration:
  // Pixel 0: Left wing tip - RED (port side)
  // Pixel 1: Right wing tip - GREEN (starboard side)
  // Pixel 2: Tail - WHITE (anti-collision, blinking)
  // Pixel 3: Landing light - WHITE (steady when on)
  
  if (navigationLights) {
    // Port (left) navigation light - steady red
    pixels.setPixelColor(0, RED);
    
    // Starboard (right) navigation light - steady green
    pixels.setPixelColor(1, GREEN);
    
    // Anti-collision light (tail) - blinking white
    pixels.setPixelColor(2, blinkState ? WHITE : OFF);
  } else {
    // Navigation lights off
    pixels.setPixelColor(0, OFF);
    pixels.setPixelColor(1, OFF);
    pixels.setPixelColor(2, OFF);
  }
  
  if (landingLights) {
    // Landing light - steady white
    pixels.setPixelColor(3, WHITE);
  } else {
    // Landing light off
    pixels.setPixelColor(3, OFF);
  }
  
  
  pixels.show();
}

// ----------------- WEIGHT ------------------
void handleWeightMotion() {
  if (waiting) {
    if (millis() - lastMoveTime >= pauseDuration) {
      waiting = false;
      movingToEdge = !movingToEdge;
      if (!movingToEdge) movingToCenter = true;
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
}