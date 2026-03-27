#include <Arduino.h>
#include <EasyNextionLibrary.h>
#include <SdFat.h>
#include "Console.h"


//===========================================================================================================
/*
void sendWinchPacket() {
  uint8_t packet[5] = { 0xFF, 0, 0, 0, 0 };
  uint8_t Pj = 0;

  if (MainConsole.currentPageId == 1) {  //Лебедки работают только при нахождении
    for (int i = 0; i < 24; i++) {
      if ((WinchBtnState[i] == true) & (Pj < WinchMaxQt)) {
        packet[Pj + 2] = i + 1;
        Pj++;
      }
    }
    packet[1] = Pj;
    Serial2.write(packet, 5);

    //for (int i = 0; i < 5; i++) Serial.println(packet[i]);
  }
}
*/

WinchController_t WController;
TouchConsole MainConsole(Serial1, 24, 4);
unsigned long sendPacketTime = 0;

void setup() {
  Serial.begin(9600);
  //Serial2.begin(115200);
  MainConsole.begin(115200);
  delay(500);
}

void loop() {
  MainConsole.Processing();

  if (((millis() - sendPacketTime) > 300) & ((MainConsole.State == TouchConsole::S_WORK))) {
    //sendWinchPacket();
    sendPacketTime = millis();
  }
}