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
const uint16_t READY_ANNOUNCE = 0x3000; // Ready announcement packet
const uint16_t READ = 0x0001;           // Read operation
const uint16_t WRITE = 0x0002;          // Write operation
const uint16_t READ_RESPONSE_GOOD = 0x0101; // Read response success
const uint16_t WRITE_RESPONSE_GOOD = 0x0102; // Write response success
const uint16_t ERROR_ANNOUNCE = 0x3FFF; // General error announcement
const uint16_t ERROR_NACK = 0x2FFF;    // not acknowledged, as in errors stemming from requests

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

void sendNACK(String message) {
  // Calculate payload size: message length + 4 (for 2FFF opcode)
  uint16_t payloadSize = message.length() + 4;
  char response[64];
  snprintf(response, sizeof(response), ":4801%04X%04X%s;\n",
           payloadSize, ERROR_NACK, message.c_str());
  Serial.print(response);
  Serial.flush(); // Ensure error message is sent
  tone(BUZZER_PIN, NOTE_FS3, 1000); // Play NOTE_FS3 for 1 second
}

void loop() {
  // Send ready packet every 300ms until contact is established
  if (!contactEstablished && millis() - lastReadyTime >= 300) {
    Serial.print(":480100043000;\n"); // Send READY_ANNOUNCE packet
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
      if (Serial.available() && Serial.read() == '\n') {
        // Serial.print("Processing packet: "); // Debug: uncomment to see packet
        // Serial.print(packet);
        // Serial.print(" ledMode: "); // Debug
        // Serial.println(ledMode);
        if (packet.length() > 0) {
          processPacket(packet);
          packet = "";
        }
      } else {
        // errorLevel = 1; // Invalid packet format (missing newline)
        // Serial.println("Error: Missing newline"); // Debug
        packet = "";
        sendNACK("Missing newline");
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
    // Serial.print("Button pressed, ledMode: "); // Debug
    // Serial.println(ledMode);
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
    sendNACK("Packet too short");
    errorLevel = 3;
    return;
  }

  // Verify protocol (4 hex digits)
  if (packet.substring(0, 4) != "4801") {
    sendNACK("Invalid protocol");
    errorLevel = 1;
    return;
  }

  // Get payload size (4 hex digits)
  uint16_t payloadSize = strtol(packet.substring(4, 8).c_str(), NULL, 16);
  int dataLength = packet.length() - 8;
  if (dataLength < payloadSize) {
    sendNACK("Payload too short");
    errorLevel = 3;
    return;
  } else if (dataLength > payloadSize) {
    sendNACK("Payload too long");
    errorLevel = 4;
    return;
  }

  // Extract payload
  String payload = packet.substring(8, 8 + payloadSize);
  uint16_t operation = strtol(payload.substring(0, 4).c_str(), NULL, 16);
  uint16_t cvNumber = strtol(payload.substring(4, 8).c_str(), NULL, 16);

  // Process operations
  if (operation == READ) { // READ (0x0001)
    if (payloadSize != 8) {
      sendNACK("Invalid READ payload size");
      errorLevel = 3;
      return;
    }
    if (cvNumber == 0x0003) { // Light color
      // Send READ_RESPONSE_GOOD (4 hex digits for opcode and cvNumber, 6 hex digits for color)
      char response[32];
      snprintf(response, sizeof(response), ":4801000E%04X%04X%02X%02X%02X;\n",
               READ_RESPONSE_GOOD, cvNumber, lastRed, lastGreen, lastBlue);
      Serial.print(response);
      Serial.flush(); // Ensure response is sent
      if (ledMode == 0) {
        updateLED(); // Ensure LED reflects current color
      }
      // Serial.print("READ: R="); // Debug
      // Serial.print(lastRed, HEX);
      // Serial.print(" G=");
      // Serial.print(lastGreen, HEX);
      // Serial.print(" B=");
      // Serial.println(lastBlue, HEX);
    } else {
      sendNACK("Unknown CV");
      errorLevel = 2;
    }
  } else if (operation == WRITE) { // WRITE (0x0002)
    if (payloadSize != 14) {
      sendNACK("Invalid WRITE payload size");
      errorLevel = 3;
      return;
    }
    if (cvNumber == 0x0003) { // Light color
      // Expect 6 hex digits for color (RRGGBB)
      lastRed = strtol(payload.substring(8, 10).c_str(), NULL, 16);
      lastGreen = strtol(payload.substring(10, 12).c_str(), NULL, 16);
      lastBlue = strtol(payload.substring(12, 14).c_str(), NULL, 16);
      if (ledMode == 0) {
        updateLED();
      }
      // Send WRITE_RESPONSE_GOOD (4 hex digits for opcode and cvNumber)
      char response[32];
      snprintf(response, sizeof(response), ":48010008%04X%04X;\n",
               WRITE_RESPONSE_GOOD, cvNumber);
      Serial.print(response);
      Serial.flush(); // Ensure response is sent
      // Serial.print("WRITE: R="); // Debug
      // Serial.print(lastRed, HEX);
      // Serial.print(" G=");
      // Serial.print(lastGreen, HEX);
      // Serial.print(" B=");
      // Serial.println(lastBlue, HEX);
    } else {
      sendNACK("Unknown CV");
      errorLevel = 2;
    }
  } else {
    sendNACK("Unknown operation");
    errorLevel = 2;
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
