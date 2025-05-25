// Pin definitions
const int RED_PIN = 9;    // PWM pin for RGB red
const int GREEN_PIN = 10; // PWM pin for RGB green
const int BLUE_PIN = 11;  // PWM pin for RGB blue
const int BUTTON_PIN = 2; // Button signal pin
const int BUZZER_PIN = 8; // Piezo buzzer I/O pin

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

// Error handling
int errorLevel = 0;            // Initialized to 0 in setup
unsigned long lastEventTime = 0;
int flashPhase = 0;
unsigned long eventInterval = 0;

void setup() {
  // Initialize pins
  pinMode(RED_PIN, OUTPUT);
  pinMode(GREEN_PIN, OUTPUT);
  pinMode(BLUE_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP); // Use internal pull-up
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(LED_BUILTIN, OUTPUT);

  // Start serial communication
  Serial.begin(9600);

  // Set initial LED state to last color (red), scaled by ledMaxLevel
  updateLED();

  // Initialize error level
  errorLevel = 0;
}

void loop() {
  // Handle serial input
  while (Serial.available()) {
    char c = Serial.read();
    if (c == ':') {
      packet = "";
    } else if (c == ';') {
      if (packet.length() > 0) {
        processPacket(packet);
        packet = "";
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
      tone(BUZZER_PIN, 440, 100); // A440 for last color
    } else {
      tone(BUZZER_PIN, 523, 100); // C5 for white or off
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
  // Check minimum length
  if (packet.length() < 6) {
    errorLevel = 3;
    return;
  }

  // Verify protocol
  if (packet.substring(0, 4) != "4801") {
    errorLevel = 1;
    return;
  }

  // Get payload size
  int payloadSize = strtol(packet.substring(4, 6).c_str(), NULL, 16);
  int dataLength = packet.length() - 6;
  if (dataLength < payloadSize) {
    errorLevel = 3;
    return;
  } else if (dataLength > payloadSize) {
    errorLevel = 4;
    return;
  }

  // Extract payload
  String payload = packet.substring(6, 6 + payloadSize);
  uint16_t command = strtol(payload.substring(0, 4).c_str(), NULL, 16);

  // Process commands
  if (command == 3) { // Set color
    if (payloadSize != 10) {
      errorLevel = 3;
      return;
    }
    lastRed = strtol(payload.substring(4, 6).c_str(), NULL, 16);
    lastGreen = strtol(payload.substring(6, 8).c_str(), NULL, 16);
    lastBlue = strtol(payload.substring(8, 10).c_str(), NULL, 16);
    if (ledMode == 0) {
      updateLED();
    }
  } else if (command == 1) { // Beep
    if (payloadSize != 12) {
      errorLevel = 3;
      return;
    }
    uint16_t frequency = strtol(payload.substring(4, 8).c_str(), NULL, 16);
    uint16_t duration = strtol(payload.substring(8, 12).c_str(), NULL, 16);
    tone(BUZZER_PIN, frequency, duration);
  } else {
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