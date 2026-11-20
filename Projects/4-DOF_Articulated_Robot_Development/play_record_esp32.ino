#include <ESP32Servo.h>
#include "BluetoothSerial.h"

BluetoothSerial SerialBT;

// ====== Servo Pins ======
#define SERVO1_PIN 5
#define SERVO2_PIN 18
#define SERVO3_PIN 19
#define SERVO4_PIN 21

// ====== Button Pins ======
#define RECORD_BTN 22
#define PLAY_BTN   23

// ====== Configuration ======
#define MAX_STEPS 10
#define SERVO_COUNT 4
#define SMOOTH_DELAY 15  // smaller = smoother, but slower

Servo servos[SERVO_COUNT];
int currentPos[SERVO_COUNT] = {90, 90, 90, 90};    // current servo angles
int recordedSteps[MAX_STEPS][SERVO_COUNT];
int stepCount = 0;

bool playing = false;

// ====== Setup ======
void setup() {
  Serial.begin(115200);
  SerialBT.begin("ESP32_RobotArm");  // Bluetooth device name
  Serial.println("🤖 ESP32 Robot Arm Ready!");
  Serial.println("Bluetooth Device Name: ESP32_RobotArm");
  Serial.println("Use format: S1 120, S2 90, etc. (via Bluetooth or Serial)");

  // Attach servos
  servos[0].attach(SERVO1_PIN);
  servos[1].attach(SERVO2_PIN);
  servos[2].attach(SERVO3_PIN);
  servos[3].attach(SERVO4_PIN);

  // Initialize all servos to 90°
  for (int i = 0; i < SERVO_COUNT; i++) {
    servos[i].write(90);
  }

  // Buttons
  pinMode(RECORD_BTN, INPUT_PULLUP);
  pinMode(PLAY_BTN, INPUT_PULLUP);
}

// ====== Main Loop ======
void loop() {
  handleBluetooth();
  handleSerial();
  handleButtons();
}

// ====== Handle Bluetooth Commands ======
void handleBluetooth() {
  if (SerialBT.available()) {
    String cmd = SerialBT.readStringUntil('\n');
    cmd.trim();
    processCommand(cmd);
  }
}

// ====== Handle Serial Monitor Input ======
void handleSerial() {
  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    processCommand(cmd);
  }
}

// ====== Parse and Execute Command ======
void processCommand(String cmd) {
  cmd.toUpperCase();

  // Replace '-' with ' ' (so both formats work)
  cmd.replace("-", " ");

  if (cmd.startsWith("S1 ")) setServo(0, cmd);
  else if (cmd.startsWith("S2 ")) setServo(1, cmd);
  else if (cmd.startsWith("S3 ")) setServo(2, cmd);
  else if (cmd.startsWith("S4 ")) setServo(3, cmd);
  else Serial.println("⚠ Invalid command!");
}
// ====== Set Servo Angle Smoothly ======
void setServo(int index, String cmd) {
  int angle = cmd.substring(3).toInt();
  angle = constrain(angle, 0, 180);

  int current = currentPos[index];
  if (angle != current) {
    if (angle > current) {
      for (int pos = current; pos <= angle; pos++) {
        servos[index].write(pos);
        delay(SMOOTH_DELAY);
      }
    } else {
      for (int pos = current; pos >= angle; pos--) {
        servos[index].write(pos);
        delay(SMOOTH_DELAY);
      }
    }
    currentPos[index] = angle;
  }

  Serial.printf("Servo %d → %d°\n", index + 1, angle);
}

// ====== Handle Record & Play Buttons ======
void handleButtons() {
  static bool recordPrev = HIGH;
  static bool playPrev = HIGH;

  bool recordState = digitalRead(RECORD_BTN);
  bool playState = digitalRead(PLAY_BTN);

  // Record button (on press)
  if (recordPrev == HIGH && recordState == LOW) {
    recordStep();
    delay(200);
  }

  // Play button (on press)
  if (playPrev == HIGH && playState == LOW) {
    if (!playing) {
      playing = true;
      playSteps();
    }
    delay(200);
  }

  recordPrev = recordState;
  playPrev = playState;
}

// ====== Record Current Servo Positions ======
void recordStep() {
  if (stepCount < MAX_STEPS) {
    for (int i = 0; i < SERVO_COUNT; i++) {
      recordedSteps[stepCount][i] = currentPos[i];
    }
    stepCount++;
    Serial.printf("📌 Recorded step #%d\n", stepCount);
  } else {
    Serial.println("⚠ Memory full (10 steps max)");
  }
}

// ====== Smooth Multi-Servo Movement ======
void moveServosSmooth(int targets[]) {
  int maxDiff = 0;
  for (int i = 0; i < SERVO_COUNT; i++) {
    int diff = abs(targets[i] - currentPos[i]);
    if (diff > maxDiff) maxDiff = diff;
  }

  for (int step = 0; step <= maxDiff; step++) {
    for (int i = 0; i < SERVO_COUNT; i++) {
      int start = currentPos[i];
      int end = targets[i];
      int pos = start + (step * (end - start)) / maxDiff;
      servos[i].write(pos);
    }
    delay(SMOOTH_DELAY);
  }

  for (int i = 0; i < SERVO_COUNT; i++) {
    currentPos[i] = targets[i];
  }
}

// ====== Play Recorded Steps in Loop ======
void playSteps() {
  if (stepCount == 0) {
    Serial.println("⚠ No recorded steps to play.");
    playing = false;
    return;
  }

  Serial.println("▶ Loop playback started...");

  while (playing) {
    for (int s = 0; s < stepCount; s++) {
      Serial.printf(" Step %d\n", s + 1);
      int targets[SERVO_COUNT];
      for (int i = 0; i < SERVO_COUNT; i++) targets[i] = recordedSteps[s][i];
      moveServosSmooth(targets);
      delay(2000);  // 2-second delay between poses
    }
    Serial.println("🔁 Replaying sequence...");
  }
}