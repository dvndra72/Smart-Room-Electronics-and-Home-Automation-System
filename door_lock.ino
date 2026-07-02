#include <ESP32Servo.h>
#include <Keypad.h>
#include <IRremote.h>
#include "BluetoothSerial.h"
#include <SPI.h>
#include <MFRC522.h>

BluetoothSerial SerialBT;
Servo lockServo;

// ================= SERVO =================
#define SERVO_PIN 13
#define SERVO_OPEN_ANGLE 30
#define SERVO_CLOSE_ANGLE 0

bool doorOpen = false;
bool servoAttached = false;

void attachServo() {
  if (!servoAttached) {
    lockServo.attach(SERVO_PIN);
    servoAttached = true;
  }
}

void detachServo() {
  if (servoAttached) {
    lockServo.detach();
    servoAttached = false;
  }
}

void moveServo(int angle) {
  attachServo();
  lockServo.write(angle);
  delay(300);
  detachServo();
}

void toggleServo() {
  if (doorOpen) {
    moveServo(SERVO_CLOSE_ANGLE);
    doorOpen = false;
  } else {
    moveServo(SERVO_OPEN_ANGLE);
    doorOpen = true;
  }
}

// ================= PUSH BUTTON =================
#define BUTTON_PIN 27
unsigned long lastButtonTime = 0;
const unsigned long buttonDebounce = 300;

// ================= KEYPAD =================
const byte ROWS = 4;
const byte COLS = 4;

char keys[ROWS][COLS] = {
  {'1','2','3','A'},
  {'4','5','6','B'},
  {'7','8','9','C'},
  {'*','0','#','D'}
};

byte rowPins[ROWS] = {32, 33, 25, 26};
byte colPins[COLS] = {12, 4, 14, 21};

Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

String enteredCode = "";
const String password = "1474";

// ================= IR =================
#define IR_PIN 15
unsigned long lastIRTime = 0;
const unsigned long irDebounce = 1000;

// ================= RFID =================
#define RFID_SS 5
#define RFID_RST 22

MFRC522 rfid(RFID_SS, RFID_RST);

// Allowed UIDs
byte uid1[] = {0xEB, 0xD8, 0x5A, 0x8F};
byte uid2[] = {0x14, 0x0F, 0xD6, 0x6F};
byte uid3[] = {0xB4, 0x6B, 0xEF, 0x6F};

bool checkUID(byte *uid, byte size) {
  if (size != 4) return false;
  if (memcmp(uid, uid1, 4) == 0) return true;
  if (memcmp(uid, uid2, 4) == 0) return true;
  if (memcmp(uid, uid3, 4) == 0) return true;
  return false;
}

// ================= SETUP =================
void setup() {
  Serial.begin(115200);
  SerialBT.begin("ESP32_DOOR_LOCK");

  pinMode(BUTTON_PIN, INPUT_PULLUP);

  attachServo();
  lockServo.write(SERVO_CLOSE_ANGLE);
  delay(300);
  detachServo();

  IrReceiver.begin(IR_PIN, ENABLE_LED_FEEDBACK);

  SPI.begin();
  rfid.PCD_Init();

  Serial.println("System Ready (Servo Safe Mode)");
}

// ================= LOOP =================
void loop() {

  // -------- PUSH BUTTON --------
  if (digitalRead(BUTTON_PIN) == LOW) {
    if (millis() - lastButtonTime > buttonDebounce) {
      toggleServo();
      Serial.println("Button Toggled");
      lastButtonTime = millis();
    }
  }

  // -------- KEYPAD --------
  char key = keypad.getKey();
  if (key) {
    if (key >= '0' && key <= '9') {
      enteredCode += key;
      Serial.print("*");
    }

    if (key == '*') {
      if (enteredCode == password) {
        toggleServo();
        Serial.println("\nPassword Correct");
      } else {
        Serial.println("\nWrong Password");
      }
      enteredCode = "";
    }

    if (key == 'D') {
      enteredCode = "";
      Serial.println("\nCleared");
    }
  }

  // -------- IR REMOTE --------
  if (IrReceiver.decode()) {
    unsigned long now = millis();
    if (now - lastIRTime > irDebounce) {
      if (IrReceiver.decodedIRData.protocol == 8 &&
          IrReceiver.decodedIRData.address == 0x80 &&
          IrReceiver.decodedIRData.command == 0x09) {

        toggleServo();
        Serial.println("IR Toggle");
        lastIRTime = now;
      }
    }
    IrReceiver.resume();
  }

  // -------- BLUETOOTH --------
  if (SerialBT.available()) {
    char bt = SerialBT.read();

    if (bt == 'X' || bt == 'x') {
      moveServo(SERVO_OPEN_ANGLE);
      doorOpen = true;
    }

    if (bt == 'Y' || bt == 'y') {
      moveServo(SERVO_CLOSE_ANGLE);
      doorOpen = false;
    }
  }

  // -------- RFID --------
  if (rfid.PICC_IsNewCardPresent() && rfid.PICC_ReadCardSerial()) {

    if (checkUID(rfid.uid.uidByte, rfid.uid.size)) {
      toggleServo();
      Serial.println("RFID Access Granted");
    } else {
      Serial.println("RFID Access Denied");
    }

    rfid.PICC_HaltA();
    rfid.PCD_StopCrypto1();
  }
}