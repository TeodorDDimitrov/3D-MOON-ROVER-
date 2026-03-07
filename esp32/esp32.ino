#include <Bluepad32.h>

GamepadPtr myGamepad;

String lastLeftCmd = "ls";
String lastRightCmd = "rs";
String lastDpadCmd = "";

void onConnectedGamepad(GamepadPtr gp) {
  if (myGamepad == nullptr) {
    Serial.println("PS5 CONNECTED!");
    myGamepad = gp;
  }
}

void onDisconnectedGamepad(GamepadPtr gp) {
  if (myGamepad == gp) {
    Serial.println("PS5 DISCONNECTED!");
    myGamepad = nullptr;
  }
}

void setup() {
  Serial.begin(115200);
  Serial2.begin(115200);
  delay(500);

  Serial.println("4x4 ROVER - PS5 Control");
  Serial.println("Waiting for controller...");

  BP32.setup(&onConnectedGamepad, &onDisconnectedGamepad);
  BP32.forgetBluetoothKeys();
  BP32.enableNewBluetoothConnections(true);
}

void loop() {
  BP32.update();

  if (myGamepad && myGamepad->isConnected()) {
    int leftY = myGamepad->axisY();
    int rightY = myGamepad->axisRY();
    uint8_t dpad = myGamepad->dpad();

    int deadZone = 20;
    int centerMin = 15 - deadZone;
    int centerMax = 15 + deadZone;

    String currentLeftCmd = "";
    String currentRightCmd = "";
    String currentDpadCmd = "";

    // LEFT STICK - Controls left motors
    if (leftY >= centerMin && leftY <= centerMax) {
      currentLeftCmd = "ls";  // Stop
    } else if (leftY < centerMin) {
      currentLeftCmd = "lf";  // Forward
    } else {
      currentLeftCmd = "lb";  // Backward
    }

    // RIGHT STICK - Controls right motors
    if (rightY >= centerMin && rightY <= centerMax) {
      currentRightCmd = "rs";  // Stop
    } else if (rightY < centerMin) {
      currentRightCmd = "rf";  // Forward
    } else {
      currentRightCmd = "rb";  // Backward
    }

    // D-PAD - Controls servos
    if (dpad & 0x01) {
      currentDpadCmd = "up";  // D-pad UP
    } else if (dpad & 0x02) {
      currentDpadCmd = "d";  // D-pad DOWN
    } else if (dpad & 0x04) {
      currentDpadCmd = "l";  // D-pad LEFT
    } else if (dpad & 0x08) {
      currentDpadCmd = "r";  // D-pad RIGHT
    } else {
      currentDpadCmd = "";
    }

    // Send LEFT motor commands
    if (currentLeftCmd != lastLeftCmd) {
      Serial2.println(currentLeftCmd); 
      Serial.print("LEFT: ");
      Serial.println(currentLeftCmd);
      lastLeftCmd = currentLeftCmd;
    }

    // Send RIGHT motor commands
    if (currentRightCmd != lastRightCmd) {
      Serial2.println(currentRightCmd); // Send to Mega
      Serial.print("RIGHT: ");
      Serial.println(currentRightCmd);
      lastRightCmd = currentRightCmd;
    }

    // Send D-PAD servo commands
    if (currentDpadCmd != lastDpadCmd) {
      if (currentDpadCmd != "") {
        Serial2.println(currentDpadCmd);  // Send to Mega
        Serial.print("SERVO: ");
        Serial.println(currentDpadCmd);
      }
      lastDpadCmd = currentDpadCmd;
    }
  }

  delay(50);
}