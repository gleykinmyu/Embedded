#ifndef COMPONENT_CLASSES_H
#define COMPONENT_CLASSES_H

#include <cstring>

#include "System.hpp"

#define FLASH_NEXPARAM_LIST \
    FLASH_STRING(VALUE, ".val") \
    FLASH_STRING(TEXT, ".txt") \
    FLASH_STRING(ENABLE, ".en") \
    FLASH_STRING(TIME, ".tim") \
    FLASH_STRING(BACK_CLR, ".bco") \
    FLASH_STRING(BACK_CLR2, ".bco2") \
    FLASH_STRING(FONT_CLR, ".pco") \
    FLASH_STRING(FONT_CLR2, ".pco2") \
    FLASH_STRING(PASSWORD, ".pw") \
    FLASH_STRING(FONT, ".font") \
    FLASH_STRING(PATH, ".path") \

enum EFlashParamID {
#define FLASH_STRING(m, s) FPARAM_##m,
    FLASH_NEXPARAM_LIST FPARAM_QT
#undef FLASH_STRING
};

static const char _PGM_NexParamStr[][8] = {
#define FLASH_STRING(m, s) s,
    FLASH_NEXPARAM_LIST "End"
#undef FLASH_STRING
};

enum EColors {
    CLR_BLACK      = 0,
    CLR_GRAY       = 21162,
    CLR_LIGHT_GRAY = 50712,
    CLR_B_PURPLE   = 8201,
    CLR_GREEN      = 1440,
    CLR_B_RED      = 18432,
    CLR_B_YELLOW   = 31428,
    CLR_WHITE      = 65535
};

typedef const char* ObjNameF;

class CBaseObject {
public:
    CBaseObject(ObjNameF Name) : _Name(Name), _Console(nullptr) {}
    CBaseObject(ObjNameF Name, BaseConsole* Console) : _Name(Name), _Console(Console) {}

    void Init(BaseConsole* Console) { _Console = Console; }

protected:
    ObjNameF      _Name;
    BaseConsole* _Console;
};

class CNexParam {
public:
    CNexParam(EFlashParamID ID, CBaseObject* Parent) : _ParamID(ID), _Parent(Parent) {}
    void getName(char* Str) { strncpy(Str, _PGM_NexParamStr[_ParamID], 7); Str[7] = '\0'; }

protected:
    EFlashParamID _ParamID;
    CBaseObject*  _Parent;
};

class CNexNumParam : public CNexParam {
public:
    CNexNumParam(EFlashParamID ID, CBaseObject* Parent) : CNexParam(ID, Parent) {}
    uint32_t get();
    void     set(uint32_t val);
};

class CNexStrParam : public CNexParam {
public:
    CNexStrParam(EFlashParamID ID, CBaseObject* Parent) : CNexParam(ID, Parent) {}
    void get(char* Str);
    void set(const char* Str);
};

class CNexColorParam : public CNexNumParam {
public:
    CNexColorParam(EFlashParamID ID, CBaseObject* Parent) : CNexNumParam(ID, Parent) {}

    EColors get();
    void    set(EColors clr);
};

class CBtnColor {
public:
    CBtnColor(EColors t_borderc, EColors t_bco, EColors t_bco2, EColors t_pco, EColors t_pco2)
        : borderc(t_borderc), bco(t_bco), bco2(t_bco2), pco(t_pco), pco2(t_pco2)
    {
    }

    EColors borderc = CLR_WHITE;
    EColors bco     = CLR_GRAY;
    EColors bco2    = CLR_GREEN;
    EColors pco     = CLR_WHITE;
    EColors pco2    = CLR_WHITE;
};

class NexPos {
public:
    NexPos(unsigned X, unsigned Y) : x(X), y(Y) {}
    unsigned x;
    unsigned y;
};

class CObject {
public:
    CObject(const char* Name) : _Name(Name), _Console(nullptr) {}
    CObject(const char* Name, BaseConsole* Console) : _Name(Name), _Console(Console) {}

    void Init(BaseConsole* Console, int NameID = -1)
    {
        _Console = Console;
        _NameID  = NameID;
    }

    uint32_t    GetCompID();
    const char* GetName() { return _Name; }

protected:
    const char*   _Name;
    int           _NameID = -1;
    BaseConsole* _Console;
};

class CTextBox : public CObject {
public:
    CTextBox(const char* Name) : CObject(Name) {}
    CTextBox(const char* Name, BaseConsole* Console) : CObject(Name, Console) {}

    void SetText(const char* Text);
    void GetText(char* Text, unsigned Size);

    void SetPos(NexPos Pos);
};

class CKeyboardPage : public CObject {
public:
    explicit CKeyboardPage(BaseConsole* Console) : CObject("keybdA", Console) {}

    void Open(uint32_t loadPageID, uint32_t loadCmpID)
    {
        _Console->writeNum("keybdA.loadpageid.val", loadPageID);
        _Console->writeNum("keybdA.loadcmpid.val", loadCmpID);
        _Console->writeStr("page keybdA");
    }
};

class CTimer : public CObject {
public:
    CTimer(const char* Name, BaseConsole* Console) : CObject(Name, Console) {}

    void Enable();
    void Disable();
};

class CBaseButton : public CTextBox {
public:
    CBaseButton(const char* Name) : CTextBox(Name) {}
    CBaseButton(const char* Name, BaseConsole* Console) : CTextBox(Name, Console) {}

    enum EColorMask {
        CM_BORDER = 0b00001,
        CM_BCO    = 0b00010,
        CM_BCO2   = 0b00100,
        CM_PCO    = 0b01000,
        CM_PCO2   = 0b10000
    };

    void Refresh(const CBtnColor& Color, uint8_t Mask = CM_BCO | CM_BCO2 | CM_PCO);
};

class CTextSelect : public CTextBox {
public:
    CTextSelect(unsigned PathLen, const char* Name, BaseConsole* Console)
        : CTextBox(Name, Console), _PathLen(PathLen)
    {
    }

    void     SetPath(const char* Path);
    void     SetVal(uint8_t Val);
    uint8_t  GetVal();

private:
    const unsigned _PathLen;
};

class CMsgBox : public CObject {
public:
    explicit CMsgBox(BaseConsole* Console)
        : CObject("", Console), _bOK("MsgBox.bOK", Console), _bYes("MsgBox.bYes", Console),
          _bNo("MsgBox.bNo", Console), _bCancel("MsgBox.bCancel", Console), _tMsg("MsgBox.tMsg", Console)
    {
    }

    enum EMsgBtn {
        MB_OK     = 0xE1,
        MB_YES    = 0xE2,
        MB_NO     = 0xE3,
        MB_CANCEL = 0xE4
    };

    enum EMsgBtnCfg {
        MBC_OK,
        MBC_OK_CANCEL,
        MBC_YES_NO
    };

    enum EBtnPos {
        BP_CENTER,
        BP_LEFT,
        BP_RIGHT,
        BP_NOT_USED
    };

    void Processing(uint8_t CmdGroup, uint8_t CMD, uint8_t Arg)
    {
        _BtnPressed = (EMsgBtn)CMD;
        _Console->SetState(BaseConsole::CS_MSGBOX_PROC);
    }

    void Show(EMsgBtnCfg Buttons, const char* Msg, uint8_t RtnCode = 0);

    uint8_t  GetRtnCode() { return _RtnCode; }
    EMsgBtn  GetBtnPressed() { return _BtnPressed; }

private:
    CBaseButton _bOK;
    CBaseButton _bYes;
    CBaseButton _bNo;
    CBaseButton _bCancel;
    CTextBox    _tMsg;

    EMsgBtn _BtnPressed;
    uint8_t _RtnCode;

    static const NexPos _BtnPositions[4];

    void _PrepareButtons(EMsgBtnCfg Buttons);
};

#endif
