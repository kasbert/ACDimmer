#include <ACDimmer.h>
#include <Wire.h>

const byte ledPin = 13;

// zero cross detect pin = 8, cmd pin 6
ACDimmer acdimmer(6);

void setup() {
  pinMode(ledPin, OUTPUT);
  acdimmer.begin();
  Wire.begin(8);                // join i2c bus with address #8
  Wire.onRequest(requestEvent); // register event
  Wire.onReceive(receiveEvent); // register event
  acdimmer.setDimming(0);
}

void loop() {
  delay(100);
}

volatile static char cmd;

void receiveEvent(int howMany) {
  char buf[4], count = 0;
  static char c;
  digitalWrite(ledPin, ++c & 1);
  while (0 < Wire.available()) { // loop through all
    char c = Wire.read(); // receive byte as a character
    buf[count] = c;
    count = (count + 1) % 3;
  }
  cmd = buf[0] &= 7;
  if (cmd == 1) {
    acdimmer.setOn();
  }
  if (cmd == 2) {
    acdimmer.setOff();
  }
  if (cmd == 3) {
    acdimmer.setDimming(buf[1]);
  }
  if (cmd == 4) {
  }
  if (cmd == 5) {
  }
}


// function that executes whenever data is requested by master
void requestEvent() {
  static char c;
  char buf[4];
  digitalWrite(ledPin, ++c & 1);
  if (cmd == 4) {
    buf[0] = acdimmer.getPulseWidth();
    Wire.write(buf, 1);
  }
  if (cmd == 5) {
    buf[0] = acdimmer.getSec() & 0xff;
    buf[1] = acdimmer.getSec() >> 8;
    buf[2] = acdimmer.getSec() >> 16;
    buf[3] = acdimmer.getSec() >> 24;
    Wire.write(buf, 4);
  }
}
