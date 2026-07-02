#include <IRremote.h>

#define IR_RECEIVE_PIN 2     // Arduino
// #define IR_RECEIVE_PIN 15  // ESP32

#define RELAY_PIN 7

bool relayState = false;
unsigned long lastTriggerTime = 0;
const unsigned long debounceDelay = 400; // ms

void setup() {
  Serial.begin(9600);

  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, HIGH); // relay OFF

  IrReceiver.begin(IR_RECEIVE_PIN, ENABLE_LED_FEEDBACK);
  Serial.println("IR Relay Controller Ready (Debounced)");
}

void loop() {
  if (IrReceiver.decode()) {

    uint8_t protocol = IrReceiver.decodedIRData.protocol;
    uint16_t address = IrReceiver.decodedIRData.address;
    uint8_t command = IrReceiver.decodedIRData.command;

    unsigned long now = millis();

    if (protocol == 8 && address == 0x80 && command == 0x09) {
      if (now - lastTriggerTime > debounceDelay) {
        relayState = !relayState;
        digitalWrite(RELAY_PIN, relayState ? LOW : HIGH);
        Serial.println("Relay TOGGLED");
        lastTriggerTime = now;
      }
    }

    IrReceiver.resume();
  }
}


IR LIGHT CODE