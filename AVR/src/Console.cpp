#include <EasyNextionLibrary.h>
#include "Console.h"

void TouchConsole::RefreshWorkPage()
{
    for (int i = 0; i < 24; i++)
        SetWinchBtnColor(i);
}

void TouchConsole::SetWinchBtnColor(uint8_t btn) {
  String numAttr;
  numAttr = "b";
  numAttr += btn;
  numAttr += ".bco";
  if (WinchBtnState[btn]) writeNum(numAttr, 1440);
  else writeNum(numAttr, 21162);
}

void TouchConsole::SetUDBtnColor(EUDBtn btn, EUDBtnClr Color) {
  switch (btn) {
    case BTN_UP:
      writeNum("pio0", UDBtnColorBitMask[Color][0]);
      writeNum("pio1", UDBtnColorBitMask[Color][1]);
      writeNum("pio2", UDBtnColorBitMask[Color][2]);
      break;
    case BTN_DOWN:
      writeNum("pio3", UDBtnColorBitMask[Color][0]);
      writeNum("pio4", UDBtnColorBitMask[Color][1]);
      writeNum("pio5", UDBtnColorBitMask[Color][2]);
      break;
  }
}

void TouchConsole::WinchBtnPress(uint8_t BTN) {
  if (WinchBtnState[BTN] == false) {
    if (_WinchBtnPressedQt < _BtnPressedMaxQt) {
      WinchBtnState[BTN] = true;
      _WinchBtnPressedQt++;
      SetWinchBtnColor(BTN);
    }
  } else {
    WinchBtnState[BTN] = false;
    _WinchBtnPressedQt--;
    SetWinchBtnColor(BTN);
  }
}

void TouchConsole::begin(unsigned long baud) {
    EasyNex::begin(baud);
    writeStr("page 0");
    SetUDBtnColor(BTN_DOWN, UDC_GREEN);
    SetUDBtnColor(BTN_UP, UDC_RED);
}

void TouchConsole::Processing() {
    NextionListen();

    //Отслеживание бездействия пользователя
    if ((currentPageId == 1) & ((millis() - _WorkPageTime) > 10000)) {
        writeStr("page 0");
        SetUDBtnColor(BTN_DOWN, UDC_BLACK);
        SetUDBtnColor(BTN_UP, UDC_BLACK);
        State = S_IDLE;
  }
}

/*
  Protocol FORMAT
  <#> <len> <Packet>
  # = 23 in HEX

  CMD Groups
  <S> - system messages
  <B> - main button press
*/
void TouchConsole::easyNexReadCustomCommand() {
  _WorkPageTime = millis();  //Обнуление таймера пинга от экрана
  State = S_WORK;
  uint8_t value;

  switch (cmdGroup) {
    case 'S':  //System messages
      value = readByte();
      if (value == 1) {  //Нажатие на начальной странице на кнопку "Нажмите для продолжения"
        writeStr("page 1");
        SetUDBtnColor(BTN_DOWN, UDC_GREEN);
        SetUDBtnColor(BTN_UP, UDC_RED);
        delay(100);
        RefreshWorkPage();
      }
    break;

    case 'B':  //Button winch press
      value = readByte();
      if (value < 24) WinchBtnPress(value);
      else if (value == 0x64) {
        for (int i = 0; i < 24; i++) {
          WinchBtnState[i] = false;
          SetWinchBtnColor(i);
        }
        _WinchBtnPressedQt = 0;
      }
      break;
  }
}