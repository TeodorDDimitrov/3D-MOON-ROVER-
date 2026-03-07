#include <avr/wdt.h>
#include <Servo.h>
#include <EEPROM.h>
  
   
// EEPROM addresses
#define EEPROM_UP_DOWN 0
#define EEPROM_R_L 1

//Servo
#define SERVO_UP_DOWN_PIN 6
#define SERVO_R_L_PIN 7

Servo SERVO_UP_DOWN;
Servo SERVO_R_L;

int posUpDown = 90;
int posRL = 90;

const int STEP_RL = 90;
const int STEP_UD = 90;
const int MIN_POS = 0;
const int MAX_POS = 180;

// Driver 1 - RIGHT SIDE MOTORS
#define AIN1_R 22//
#define AIN2_R 23//
#define PWMA_R 44//

#define BIN1_R 24//
#define BIN2_R 25//
#define PWMB_R 45

// Driver 2 - LEFT SIDE MOTORS
#define AIN1_L 26
#define AIN2_L 27
#define PWMA_L 46

#define BIN1_L 28
#define BIN2_L 29
#define PWMB_L 2

// EEPROM save management
unsigned long lastSave = 0;
bool positionsChanged = false;

void ServoUp() {
  if (posUpDown + STEP_UD <= MAX_POS) {
    posUpDown += STEP_UD;
  } else {
    posUpDown = MAX_POS;
  }
  SERVO_UP_DOWN.write(posUpDown);
  positionsChanged = true;
  Serial.print("Up: ");
  Serial.println(posUpDown);
}

void ServoDown() {
  if (posUpDown - STEP_UD >= MIN_POS) {
    posUpDown -= STEP_UD;
  } else {
    posUpDown = MIN_POS;
  }
  SERVO_UP_DOWN.write(posUpDown);
  positionsChanged = true;
  Serial.print("Down: ");
  Serial.println(posUpDown);
}

void ServoRight() {
  if (posRL + STEP_RL <= MAX_POS) {
    posRL += STEP_RL;
  } else {
    posRL = MAX_POS;
  }
  SERVO_R_L.write(posRL);
  positionsChanged = true;
  Serial.print("Right: ");
  Serial.println(posRL);
}

void ServoLeft() {
  if (posRL - STEP_RL >= MIN_POS) {
    posRL -= STEP_RL;
  } else {
    posRL = MIN_POS;
  }
  SERVO_R_L.write(posRL);
  positionsChanged = true;
  Serial.print("Left: ");
  Serial.println(posRL);
}

void rightForward(int speed) {
  digitalWrite(AIN1_R, HIGH);
  digitalWrite(AIN2_R, LOW);
  analogWrite(PWMA_R, speed);
  
  digitalWrite(BIN1_R, HIGH);
  digitalWrite(BIN2_R, LOW);
  analogWrite(PWMB_R, speed);
}

void rightBackward(int speed) {
  digitalWrite(AIN1_R, LOW);
  digitalWrite(AIN2_R, HIGH);
  analogWrite(PWMA_R, speed);
  
  digitalWrite(BIN1_R, LOW);
  digitalWrite(BIN2_R, HIGH);
  analogWrite(PWMB_R, speed);
}

void leftForward(int speed) {
  digitalWrite(AIN1_L, HIGH);
  digitalWrite(AIN2_L, LOW);
  analogWrite(PWMA_L, speed);
  
  digitalWrite(BIN1_L, HIGH);
  digitalWrite(BIN2_L, LOW);
  analogWrite(PWMB_L, speed);
}

void leftBackward(int speed) {
  digitalWrite(AIN1_L, LOW);
  digitalWrite(AIN2_L, HIGH);
  analogWrite(PWMA_L, speed);
  
  digitalWrite(BIN1_L, LOW);
  digitalWrite(BIN2_L, HIGH);
  analogWrite(PWMB_L, speed);
}

void stopLMotors() {
  digitalWrite(AIN1_L, LOW);
  digitalWrite(AIN2_L, LOW);
  analogWrite(PWMA_L, 0);
  
  digitalWrite(BIN1_L, LOW);
  digitalWrite(BIN2_L, LOW);
  analogWrite(PWMB_L, 0);
}

void stopRMotors() {
  digitalWrite(AIN1_R, LOW);
  digitalWrite(AIN2_R, LOW);
  analogWrite(PWMA_R, 0);
  
  digitalWrite(BIN1_R, LOW);
  digitalWrite(BIN2_R, LOW);
  analogWrite(PWMB_R, 0);
}

void savePositionsToEEPROM() {
  EEPROM.write(EEPROM_UP_DOWN, posUpDown);
  EEPROM.write(EEPROM_R_L, posRL);
  positionsChanged = false;
  lastSave = millis();
  Serial.println("Positions saved to EEPROM");
}

void setup() {
  wdt_enable(WDTO_2S);
  
  Serial.begin(115200);
  Serial1.begin(115200);
  
  posUpDown = EEPROM.read(EEPROM_UP_DOWN);
  posRL = EEPROM.read(EEPROM_R_L);

  if (posUpDown > MAX_POS || posUpDown < MIN_POS) {
    posUpDown = 90;
    EEPROM.write(EEPROM_UP_DOWN, posUpDown);
  }
  if (posRL > MAX_POS || posRL < MIN_POS) {
    posRL = 90;
    EEPROM.write(EEPROM_R_L, posRL);
  }
  
  SERVO_UP_DOWN.attach(SERVO_UP_DOWN_PIN);
  SERVO_R_L.attach(SERVO_R_L_PIN);
  SERVO_UP_DOWN.write(posUpDown);
  SERVO_R_L.write(posRL);
  
  pinMode(AIN1_R, OUTPUT);
  pinMode(AIN2_R, OUTPUT);
  pinMode(PWMA_R, OUTPUT);
  pinMode(BIN1_R, OUTPUT);
  pinMode(BIN2_R, OUTPUT);
  pinMode(PWMB_R, OUTPUT);
  
  pinMode(AIN1_L, OUTPUT);
  pinMode(AIN2_L, OUTPUT);
  pinMode(PWMA_L, OUTPUT);
  pinMode(BIN1_L, OUTPUT);
  pinMode(BIN2_L, OUTPUT);
  pinMode(PWMB_L, OUTPUT);
  
  Serial.println("Mega Ready - Waiting for commands...");
  
  wdt_reset();
}

void loop() {
  wdt_reset();
  
  // Save positions to EEPROM every 30 seconds if changed
  if (positionsChanged && (millis() - lastSave > 30000)) {
    savePositionsToEEPROM();
  }
  
  if (Serial1.available() > 0) {
    char command[10];
    int len = Serial1.readBytesUntil('\n', command, sizeof(command) - 1);
    command[len] = '\0';
  
    while (len > 0 && (command[len-1] == '\r' || command[len-1] == ' ' || command[len-1] == '\n')) {
      command[--len] = '\0';
    }
    
    // Process commands
    if (strcmp(command, "lf") == 0) {
      Serial.println("-> LF");
      leftForward(255);
    }
    else if (strcmp(command, "lb") == 0) {
      Serial.println("-> LB");
      leftBackward(255);
    }
    else if (strcmp(command, "ls") == 0) {
      Serial.println("-> LS");
      stopLMotors();
    }
    else if (strcmp(command, "rf") == 0) {
      Serial.println("-> RF");
      rightForward(255);
    }
    else if (strcmp(command, "rb") == 0) {
      Serial.println("-> RB");
      rightBackward(255);
    }
    else if (strcmp(command, "rs") == 0) {
      Serial.println("-> RS");
      stopRMotors();
    }
    else if (strcmp(command, "l") == 0) {
      ServoLeft();
    }
    else if (strcmp(command, "r") == 0) {
      ServoRight();
    }
    else if (strcmp(command, "up") == 0) {
      ServoUp();
    }
    else if (strcmp(command, "d") == 0) {
      ServoDown();
    }
    else if (len > 0) {
      Serial.print("Unknown command: ");
      Serial.println(command);
    }
  }
}