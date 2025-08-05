#include "pitches.h"

// Pin definitions
const int RED_PIN = 9;    // PWM pin for RGB red, labeled ~9, uses Timer 1
const int GREEN_PIN = 10; // PWM pin for RGB green, labeled ~10, uses Timer 1
const int BLUE_PIN = 5;   // PWM pin for RGB blue, labeled ~5, uses Timer 0 (avoids Timer 2 conflict with tone())
// ^ Avoid 3 & 11 for lights:
// Interference: Pin 11 is controlled by Timer 2 on the ATmega328P,
//   which can be affected by certain libraries or interrupts.
//   The tone() function (used for the buzzer on pin 8) uses Timer 2,
//   which could interfere with pin 11’s PWM.
//   The tone() function disables PWM on pins 3 and 11 when active.
const int BUTTON_PIN = 2; // Button signal pin, labeled 2
const int BUZZER_PIN = 8; // Piezo buzzer I/O pin, labeled 8

// Opcode definitions
const uint16_t READY_ANNOUNCE = 0x1000; // Opcode for ready announcement packet

// LED maximum level for calibration (default halves brightness)
// For ledMaxLevel = 128, the maximum average voltage is approximately 2.51V (5V * 128 / 255)
// For ledMaxLevel = 255, the maximum average voltage would be 5V
const byte ledMaxLevel = 128;

// LED mode states
int ledMode = 0; // 0: last color, 1: white, 2: off
byte lastRed = 255;   // Default last color: red
byte lastGreen = 0;
byte lastBlue = 0;

// Button state
bool lastButtonState = HIGH; // Pull-up, HIGH when not pressed

// Serial packet buffer
String packet = "";
bool contactEstablished = false; // Track if contact has been established

// Error handling
int errorLevel = 0;            // Initialized to 0 in setup
unsigned long lastEventTime = 0;
int flashPhase = 0;
unsigned long eventInterval = 0;
unsigned long lastReadyTime = 0; // Track time of last ready packet

void setup() {
  // Initialize pins
  pinMode(RED_PIN, OUTPUT);
  pinMode(GREEN_PIN, OUTPUT);
  pinMode(BLUE_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP); // Use internal pull-up
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(LED_BUILTIN, OUTPUT);

  // Start serial communication at 9600 bps and wait for port to open
  Serial.begin(9600);
  while (!Serial) {
    ; // Wait for serial port to connect (useful for native USB boards, harmless for UNO)
  }
  delay(2000); // Wait for CH340 reset to complete

  // Set initial LED state to last color (red), scaled by ledMaxLevel
  updateLED();

  // Initialize error level
  errorLevel = 0;

  // Clear serial output buffer
  Serial.flush();
}

void loop() {
  // Send ready packet every 300ms until contact is established
  if (!contactEstablished && millis() - lastReadyTime >= 300) {
    Serial.print(":480100041000;\n"); // Send READY_ANNOUNCE packet
    Serial.flush(); // Ensure output is sent
    lastReadyTime = millis();
  }

  // Handle serial input
  while (Serial.available() > 0) {
    contactEstablished = true; // Stop sending ready packet once data is received
    char c = Serial.read();
    // Serial.print("Received: "); // Debug: uncomment to see each character
    // Serial.println(c);
    if (c == ':') {
      packet = "";
    } else if (c == ';') {
      // Expect newline after semicolon
      if (Serial.available() && Serial.read() == '\n') {
        // Serial.print("Processing packet: "); // Debug: uncomment to see packet
        // Serial.println(packet);
        if (packet.length() > 0) {
          processPacket(packet);
          packet = "";
        }
      } else {
        packet = "";
        errorLevel = 1; // Invalid packet format (missing newline)
        // Serial.println("Error: Missing newline"); // Debug
      }
    } else {
      packet += c;
    }
  }

  // Handle button press
  bool buttonState = digitalRead(BUTTON_PIN);
  if (lastButtonState == HIGH && buttonState == LOW) {
    ledMode = (ledMode + 1) % 3; // Cycle modes
    if (ledMode == 0) {
      tone(BUZZER_PIN, NOTE_C4, 50); // for last color (set using USB)
    } else {
      tone(BUZZER_PIN, NOTE_G3, 20); // for white or off
    }
    updateLED();
    delay(50); // Simple debounce
  }
  lastButtonState = buttonState;

  // Handle error LED flashing
  if (errorLevel > 0) {
    unsigned long currentTime = millis();
    if (currentTime - lastEventTime >= eventInterval) {
      lastEventTime = currentTime;
      if (flashPhase < errorLevel * 2) {
        if (flashPhase % 2 == 0) {
          digitalWrite(LED_BUILTIN, HIGH);
        } else {
          digitalWrite(LED_BUILTIN, LOW);
        }
        flashPhase++;
        eventInterval = 250;
      } else {
        digitalWrite(LED_BUILTIN, LOW);
        eventInterval = 3000;
        flashPhase = 0;
      }
    }
  } else if (errorLevel == 0) {
    digitalWrite(LED_BUILTIN, LOW);
    errorLevel = -1;
  }
}

void processPacket(String packet) {
  // Check minimum length (protocol + payload size + minimum payload)
  if (packet.length() < 8) {
    errorLevel = 3;
    return;
  }

  // Verify protocol
  if (packet.substring(0, 4) != "4801") {
    errorLevel = 1;
    return;
  }

  // Get payload size
  uint16_t payloadSize = strtol(packet.substring(4, 8).c_str(), NULL, 16);
  int dataLength = packet.length() - 8;
  if (dataLength < payloadSize) {
    errorLevel = 3;
    return;
  } else if (dataLength > payloadSize) {
    errorLevel = 4;
    return;
  }

  // Extract payload
  String payload = packet.substring(8, 8 + payloadSize);
  uint16_t operation = strtol(payload.substring(0, 4).c_str(), NULL, 16);
  uint16_t cvNumber = strtol(payload.substring(4, 8).c_str(), NULL, 16);

  // Process operations
  if (operation == 0x0001) { // READ
    if (payloadSize != 8) {
      errorLevel = 3;
      return;
    }
    if (cvNumber == 0x0003) { // Light color
      // Send READ_RESPONSE_GOOD
      char response[32];
      snprintf(response, sizeof(response), ":4801000E01010003%02X%02X%02X;\n",
               lastRed, lastGreen, lastBlue);
      Serial.print(response);
      Serial.flush(); // Ensure response is sent
    } else {
      errorLevel = 2; // Unknown CV
    }
  } else if (operation == 0x0002) { // WRITE
    if (payloadSize != 14) {
      errorLevel = 3;
      return;
    }
    if (cvNumber == 0x0003) { // Light color
      lastRed = strtol(payload.substring(8, 10).c_str(), NULL, 16);
      lastGreen = strtol(payload.substring(10, 12).c_str(), NULL, 16);
      lastBlue = strtol(payload.substring(12, 14).c_str(), NULL, 16);
      if (ledMode == 0) {
        updateLED();
      }
      // Send WRITE_RESPONSE_GOOD
      Serial.print(":4801000801020003;\n");
      Serial.flush(); // Ensure response is sent
    } else {
      errorLevel = 2; // Unknown CV
    }
  } else {
    errorLevel = 2; // Unknown operation
  }
}

void updateLED() {
  if (ledMode == 0) { // Last color
    // Scale the color values to the maximum level
    // The actual value written to analogWrite() will be between 0 and ledMaxLevel
    // Corresponding to an average voltage of 0V to (ledMaxLevel / 255)*5V
    analogWrite(RED_PIN, (static_cast<unsigned int>(lastRed) * ledMaxLevel) / 255);
    analogWrite(GREEN_PIN, (static_cast<unsigned int>(lastGreen) * ledMaxLevel) / 255);
    analogWrite(BLUE_PIN, (static_cast<unsigned int>(lastBlue) * ledMaxLevel) / 255);
  } else if (ledMode == 1) { // White
    analogWrite(RED_PIN, ledMaxLevel);
    analogWrite(GREEN_PIN, ledMaxLevel);
    analogWrite(BLUE_PIN, ledMaxLevel);
  } else { // Off
    analogWrite(RED_PIN, 0);
    analogWrite(GREEN_PIN, 0);
    analogWrite(BLUE_PIN, 0);
  }
}
