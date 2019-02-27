
#include <ACDimmer.h>

const byte ledPin = 13;

// zero cross detect pin = 8
ACDimmer acdimmer(6);

void setup() {
  //Serial.begin(9600);
  pinMode(ledPin, OUTPUT);
  acdimmer.begin();
}

void loop() {
  if (acdimmer.isEdgeDetected()) {
    digitalWrite(ledPin, HIGH);
    uint8_t dim = analogRead(A0) / 10;
    static uint8_t dim1;
    if (dim < dim1) {
      dim1--;
    }
    if (dim > dim1) {
      dim1++;
    }
    if (dim1 <= 100) {
      acdimmer.setDimming(dim1);
    } else {
      acdimmer.setOn();
    }
  }
  delay(10);
  digitalWrite(ledPin, LOW);

}
