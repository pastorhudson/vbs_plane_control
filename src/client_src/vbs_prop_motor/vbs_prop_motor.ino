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
bool propRunning = false;

// Motion control variables
bool motionEnabled = false;     // Global control for motion system
unsigned long lastMoveTime = 0;
unsigned long pauseDuration = 0;
bool movingToEdge = false;
bool movingRight = true;        // Start by moving right
bool movingToCenter = false;
bool waiting = false;
bool initialMove = true;        // Flag to handle first move to right

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
  // Initialize Serial for debug output
  Serial.begin(115200);
  Serial.println("ESP32 Starting up...");
  
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
  
  // setupESPNow();
  delay(10);
  randomSeed(analogRead(0)); // Seed random with noise
 
  WiFi.mode(WIFI_STA); // Required for esp_now
  Serial.println("MAC Address:");
  Serial.println(WiFi.macAddress());
  Serial.println("Setup complete, waiting for ESP-NOW commands...");
}

void loop() {
  controlPropeller();
  handleWeightMotion();
  updateNavigationLights();
}

void OnDataRecv(const esp_now_recv_info_t *info, const uint8_t *incomingData, int len) {
//   // Debug: Print sender MAC address
//   Serial.print("ESP-NOW message received from: ");
//   for (int i = 0; i < 6; i++) {
//     Serial.printf("%02X", info->src_addr[i]);
//     if (i < 5) Serial.print(":");
//   }
//   Serial.println();
  
//   // Debug: Print message length and raw data
//   Serial.print("Message length: ");
//   Serial.print(len);
//   Serial.print(" bytes, Raw data: ");
//   for (int i = 0; i < len; i++) {
//     Serial.printf("%02X ", incomingData[i]);
//   }
//   Serial.println();
  
//   // Create a properly null-terminated string
//   char buffer[len + 1];
//   memcpy(buffer, incomingData, len);
//   buffer[len] = '\0';  // Null terminate
  
//   String command = String(buffer);
//   Serial.print("Command received: '");
//   Serial.print(command);
//   Serial.println("'");
 
//   // Process commands
//   if (command == "PROP_START") {
//     propRunning = true;
//     Serial.println("-> Propeller started");
//   } else if (command == "PROP_STOP") {
//     propRunning = false;
//     Serial.println("-> Propeller stopped");
//   } else if (command == "WEIGHT_START") {
//     motionEnabled = true;        // Enable motion system
//     initialMove = true;          // Flag for initial move to right
//     movingToEdge = true;         // Start moving to edge
//     movingRight = true;          // Always start by moving right
//     waiting = false;             // Stop waiting
//     movingToCenter = false;      // Clear center flag
//     Serial.println("-> Weight movement started - moving right");
//   } else if (command == "WEIGHT_STOP") {
//     motionEnabled = false;       // Disable motion system
//     waiting = false;             // Clear waiting state
//     movingToEdge = false;        // Stop moving to edge
//     movingToCenter = false;      // Stop moving to center
//     Serial.println("-> Weight movement stopped");
//   } else if (command == "NAV_ON") {
//     navigationLights = true;
//     Serial.println("-> Navigation lights ON");
//   } else if (command == "NAV_OFF") {
//     navigationLights = false;
//     Serial.println("-> Navigation lights OFF");
//   } else if (command == "LANDING_ON") {
//     landingLights = true;
//     Serial.println("-> Landing lights ON");
//   } else if (command == "LANDING_OFF") {
//     landingLights = false;
//     Serial.println("-> Landing lights OFF");
//   } else if (command == "LIGHTS_ALL_ON") {
//     navigationLights = true;
//     landingLights = true;
//     Serial.println("-> All lights ON");
//   } else if (command == "LIGHTS_ALL_OFF") {
//     navigationLights = false;
//     landingLights = false;
//     Serial.println("-> All lights OFF");
//   } else {
//     Serial.print("-> Unknown command: '");
//     Serial.print(command);
//     Serial.println("'");
//   }
  
//   Serial.println("---");
}

void setupESPNow() {
//   WiFi.mode(WIFI_STA);
//   if (esp_now_init() != ESP_OK) {
//     Serial.println("ESP-NOW init failed");
//     return;
//   }
//   Serial.println("ESP-NOW initialized successfully");
//   esp_now_register_recv_cb(OnDataRecv);
//   Serial.println("ESP-NOW receive callback registered");
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
  // Only proceed if motion is enabled
  if (!motionEnabled) {
    return;
  }
  
  if (waiting) {
    if (millis() - lastMoveTime >= pauseDuration) {
      waiting = false;
      
      if (movingToCenter) {
        // After center pause, choose random direction
        movingRight = random(0, 2); // 0 = left, 1 = right
        movingToEdge = true;
        movingToCenter = false;
        Serial.print("-> Moving ");
        Serial.println(movingRight ? "right" : "left");
      } else {
        // After edge pause, move to center
        movingToCenter = true;
        movingToEdge = false;
        Serial.println("-> Moving to center");
      }
    } else {
      return;
    }
  }
 
  if (movingToEdge) {
    if (movingRight) {
      digitalWrite(weightDirPin, HIGH);
      digitalWrite(weightStepPin, HIGH);
      delayMicroseconds(200);
      digitalWrite(weightStepPin, LOW);
      delayMicroseconds(200);
      if (digitalRead(limitRightPin) == LOW) {
        startPause();
        Serial.println("-> Reached right limit");
      }
    } else {
      digitalWrite(weightDirPin, LOW);
      digitalWrite(weightStepPin, HIGH);
      delayMicroseconds(200);
      digitalWrite(weightStepPin, LOW);
      delayMicroseconds(200);
      if (digitalRead(limitLeftPin) == LOW) {
        startPause();
        Serial.println("-> Reached left limit");
      }
    }
  } else if (movingToCenter) {
    // Move to center from either side
    static int centerSteps = 0;
    digitalWrite(weightDirPin, movingRight ? LOW : HIGH); // Reverse direction
    digitalWrite(weightStepPin, HIGH);
    delayMicroseconds(200);
    digitalWrite(weightStepPin, LOW);
    delayMicroseconds(200);
    centerSteps++;
    if (centerSteps >= 42000) { // Adjust to match your track's center
      centerSteps = 0;
      startPause();
      Serial.println("-> Reached center");
    }
  }
}

void startPause() {
  waiting = true;
  lastMoveTime = millis();
  pauseDuration = random(8000, 15000); // 8-15 seconds
}