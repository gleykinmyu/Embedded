#ifndef SYSTEM_H
#define SYSTEM_H

#include <cstring>

#include "../../lib/EasyNextionLibrary/EasyNextionLibrary.hpp"
#include "FlashConsts.hpp"

static const uint8_t SysWinchMaxQt  = 24;
static const uint8_t SysSceneMaxQt = 32;
static const uint8_t SysFileMaxQt   = 64;
static const uint8_t SysMaxNameLen  = 17 + 1;
static const unsigned SysFileNamesStrLen = (SysMaxNameLen + 2) * (SysFileMaxQt + 1);
static const char SysTemplateFileName[] = {(char)0xFB, (char)0xE1, (char)0xE2, (char)0xEC, (char)0xEF, (char)0xEE, (char)0x00};

class BaseConsole : public EasyNex {
public:
    explicit BaseConsole(BIF::IHardwareSerial& Console, MillisFn millisFn, DelayFn delayFn)
        : EasyNex(Console, millisFn, delayFn)
    {
    }

    enum ECState {
        CS_STARTUP,
        CS_IDLE,
        CS_WORK,
        CS_BLOCK,
        CS_EDIT,
        CS_FILE,
        CS_MSGBOX,
        CS_MSGBOX_PROC
    };

    virtual void ShowMsgBox(uint8_t Buttons, const char* Msg, uint8_t RtnCode) {}
    virtual void ShowMsgBoxID(uint8_t Buttons, EFlashStrID Msg, uint8_t RtnCode) {}

    virtual void SetState(ECState vState) { _State = vState; }
    ECState      GetState() { return _State; }

protected:
    ECState _State = CS_STARTUP;
};

class WinchScene {
public:
    uint8_t Version = 0;
    char    Name[SysMaxNameLen] = "-";
    bool    WinchSelect[SysWinchMaxQt] = {};
    uint8_t Speed = 255;

    void SetName(const char* SceneName)
    {
        strncpy(Name, SceneName, SysMaxNameLen - 1U);
        Name[SysMaxNameLen - 1U] = '\0';
    }
    bool isEmpty() { return Name[0] == '-'; }
    void Clear()
    {
        for (uint8_t i = 0; i < SysWinchMaxQt; i++)
            WinchSelect[i] = false;
        SetName("-");
    }
};

class WinchShowFile {
public:
    uint8_t    Version = 0;
    char       Name[SysMaxNameLen] = "-";
    WinchScene Scenes[SysSceneMaxQt];
};

class CWinch {
public:
    enum class EType {
        WT_STEEL_ROPE,
        WT_CHAIN
    };

    enum class EState {
        WS_UNCONNECTED,
        WS_WORK,
        WS_UP_LIMIT,
        WS_DOWN_LIMIT,
        WS_FAULT,
        WS_BLOCKED
    };
    EType  Type  = EType::WT_STEEL_ROPE;
    EState State = EState::WS_UNCONNECTED;
};

typedef struct _WinchControllerStruct {
    CWinch        Winch[SysWinchMaxQt] = {};
    const static uint8_t WinchMaxQt = 3;
} WinchController_t;

enum ECmdGroups {
    CGR_SYS       = 'S',
    CGR_PAGE      = 'P',
    CGR_FILE      = 'F',
    CGR_SCENE     = 'C',
    CGR_WINCH_BTN = 'W'
};

enum EMsgBoxTextID {
    MBT_FILE_SAVED          = 11,
    MBT_FILE_EXIST          = 12,
    MBT_FILE_EXIST_REWRITE  = 13,
    MBT_FILE_NOT_EXISTS     = 14,
    MBT_FILENAME_TEMPLATE   = 15,
    MBT_FILENAME_EMPTY      = 16,
    MBT_FILE_DELETE_ACK     = 21,
    MBT_FILE_DELETE_OPEN    = 22,
    MBT_FILE_DELETED        = 23,
    MBT_SCENE_CLEAR_ACK     = 31
};

#endif
