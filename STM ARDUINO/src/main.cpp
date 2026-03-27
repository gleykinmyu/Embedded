#include <Arduino.h>
#include <HardwareSerial.h>
#include "startup_stm32f407xx.s"


// put function declarations here:
int myFunction(int, int);

void setup() {
  // put your setup code here, to run once:
  int result = myFunction(2, 3);
  Serial1.begin(9600);
  //Serial2.begin(9600);
  //Serial3.begin(9600);
  //Serial4.begin(9600);
  //Serial5.begin(9600);
  //Serial6.begin(9600);
}

void loop() {
  // put your main code here, to run repeatedly:
  Serial1.print("Hello world");
  delay(1000);
}

// put function definitions here:
int myFunction(int x, int y) {
  return x + y;
}