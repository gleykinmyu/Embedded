#ifndef CONSOLE_H
#define CONSOLE_H

#include "ComponentClasses.hpp"
#include "ConsoleManagers.hpp"
#include "System.hpp"

class TouchConsole : private BaseConsole {
public:
    TouchConsole(BIF::IHardwareSerial& serial, MillisFn millisFn, DelayFn delayFn, WinchSD* SD,
                 const uint8_t BtnQt, const uint8_t BtnSelectedMaxQt)
        : BaseConsole(serial, millisFn, delayFn),
          SceneMngr(&ShowFile, WButtons, this),
          FileMngr(SD, &ShowFile, this),
          tStatus("tSt", this),
          keybd(this),
          MsgBox(this),
          tmrBlock("tmrBlock", this),
          sBlock("sBlock", this)
    {
        _BtnQt = BtnQt;
        WinchButton::InitStatic(BtnSelectedMaxQt);

        for (uint8_t i = 0; i < SysWinchMaxQt; i++)
            WButtons[i].Init(this, i);
    }

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

    enum ECmdSys {
        CSYS_WAIT_BTN       = 0x01,
        CSYS_KEYBOARD_RTN   = 0x21,
        CSYS_KEYBOARD_CLOSE = 0x22,

        CSYS_MSGBOX_OK     = CMsgBox::MB_OK,
        CSYS_MSGBOX_YES    = CMsgBox::MB_YES,
        CSYS_MSGBOX_NO     = CMsgBox::MB_NO,
        CSYS_MSGBOX_CANCEL = CMsgBox::MB_CANCEL
    };

    enum EWinchBtnCmd {
        CW_CLEAR      = 0x30,
        CW_SELECT_ALL = 0x31,
        CW_BLOCK      = 0x32,
    };

    void RefreshWinchButtons();
    void ClearSelection();
    void SetUDBtnColor(EUDBtn btn, EUDBtnClr Color);

    void SetState(ECState vState);
    void SetStateText();

    void begin(unsigned long baud);
    void Processing();

    WinchShowFile ShowFile;

    WinchButton   WButtons[SysWinchMaxQt];
    SceneManager  SceneMngr;
    FileManager   FileMngr;

    CTextBox    tStatus;
    CKeyboardPage keybd;
    CMsgBox       MsgBox;

    CTimer    tmrBlock;
    CBaseButton sBlock;

    void ShowMsgBox(uint8_t Buttons, const char* Msg, uint8_t RtnCode) { MsgBox.Show((CMsgBox::EMsgBtnCfg)Buttons, Msg, RtnCode); }
    void ShowMsgBoxID(uint8_t Buttons, EFlashStrID Msg, uint8_t RtnCode) { MsgBox.Show((CMsgBox::EMsgBtnCfg)Buttons, GetFlashStr(Msg), RtnCode); }

private:
    void ReadCustomCMD() override;

    unsigned long _WorkPageTime = 0;
    uint8_t       _BtnQt       = 0;
    uint8_t       _ScnBtnRenameID = 0;
    char          _ScnBtnRenameText[SysMaxNameLen] = "";

    const static uint8_t _UDBtnColorBitMask[8][3];
};

#endif
