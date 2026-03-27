#include <Arduino.h>
#include <EasyNextionLibrary.h>
#include "System.h"

class TouchConsole : private EasyNex {
public:
    TouchConsole(HardwareSerial& serial, const uint8_t BtnQt, const uint8_t BtnPressedMaxQt) : EasyNex(serial) {
        _BtnPressedMaxQt = BtnPressedMaxQt;
        _BtnQt = BtnQt;
    };

    enum EState {
        S_IDLE,
        S_WORK,
        S_FILE
    };

    enum EUDBtnClr {
        UDC_BLACK,
        UDC_RED,
        UDC_YELLOW,
        UDC_GREEN,
        UDC_CYAN,
        UDC_BLUE,
        UDC_PURPLE,
        UDC_WHITE
    };
    
    enum EUDBtn {
        BTN_UP,
        BTN_DOWN
    };

    void RefreshWorkPage();
    void SetWinchBtnColor(uint8_t btn);
    void SetUDBtnColor(EUDBtn btn, EUDBtnClr Color);
    void WinchBtnPress(uint8_t BTN);
    void begin(unsigned long baud);
    void Processing();
    bool WinchBtnState[SysWinchMaxQt];

    EState State = S_IDLE;

private:
    void TouchConsole::easyNexReadCustomCommand();
    uint8_t _WinchBtnPressedQt = 0;
    unsigned long _WorkPageTime = 0;
    uint8_t _BtnPressedMaxQt;
    uint8_t _BtnQt;
    
    const static inline uint8_t UDBtnColorBitMask[8][3] = {
        { 0, 0, 0 },
        { 1, 0, 0 },
        { 1, 1, 0 },
        { 0, 1, 0 },
        { 0, 1, 1 },
        { 0, 0, 1 },
        { 1, 0, 1 },
        { 1, 1, 1 }
    };
};