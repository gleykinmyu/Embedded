#ifndef CONSOLE_MANAGERS_H
#define CONSOLE_MANAGERS_H

#include "SDCard.hpp"
#include "System.hpp"
#include "ComponentClasses.hpp"

class WinchButton : public CBaseButton {
public:
    WinchButton() : CBaseButton("b") { };

    enum EState {
        S_DISABLED,
        S_ENABLED,
        S_SELECTED,
        S_BLOCKED
    };

    void Press();
    void UnPress();

    void Refresh(uint8_t Mask = CM_BCO | CM_BCO2 | CM_PCO | CM_PCO2) { CBaseButton::Refresh(_BtnColors[_State], Mask); };


    void Select();
    void UnSelect();

    void SetState(EState State);
    EState GetState();

    static void InitStatic(uint8_t BtnSelectedMaxQt) {_BtnSelectedMaxQt = BtnSelectedMaxQt; };

private:
    EState _State = S_ENABLED;
    static uint8_t _WinchBtnSelectedQt;
    static uint8_t _BtnSelectedMaxQt;
    const static CBtnColor _BtnColors[4];
};

class SceneButton : public CBaseButton {
public:
    SceneButton() : CBaseButton("bS"), _SceneEditBtn("mScn.bS") { };
    void Init(BaseConsole* Console, int NameID) { CBaseButton::Init(Console, NameID); _SceneEditBtn.Init(Console, NameID); }
    void SetText(const char* Text) {CBaseButton::SetText(Text); _SceneEditBtn.SetText(Text); };

    enum EState {
        S_EMPTY,
        S_NONEMPTY
    };

    void Refresh() { CBaseButton::Refresh(_BtnColors[State], CM_BCO | CM_BCO2 | CM_PCO | CM_PCO2); _SceneEditBtn.Refresh(_BtnColors[State], CM_BCO | CM_BCO2 | CM_PCO | CM_PCO2); }
    EState State = S_EMPTY;

private:
    const static CBtnColor _BtnColors[2];
    CBaseButton _SceneEditBtn;
};

class MenuButton : public CBaseButton {
public: 
    MenuButton(const char* MenuName, const char * TimerName, BaseConsole* Console) : CBaseButton(MenuName, Console), _tmr(TimerName, Console) { };

    enum EState {
        S_DISABLED,
        S_ENABLED,
        S_BLINKING
    };

    void SetState(EState State);
    EState GetState() { return _State; };
    void Refresh() { CBaseButton::Refresh(_BtnColors[_State], CM_BCO | CM_BCO2 | CM_PCO | CM_PCO2); }

private:
    EState _State = S_ENABLED;
    CTimer _tmr;
    const static CBtnColor _BtnColors[2];
};

class SceneManager {
public:
    SceneManager(WinchShowFile* ShowFile, WinchButton * WinchBtns, BaseConsole* Console) : 
    mScene("bsmScene", "", Console), mClear("bsmClear", "tmClear", Console), 
    mRecord("bsmRecord", "tmRecord", Console), mRename("bsmRename", "tmRename", Console), 
    tPage("tPg", Console)
    {
        for (uint8_t i = 0; i < ScBtnQt; i++) {
            SButtons[i].Init(Console, i);
        }

        _ShowFile = ShowFile;
        _WButtons = WinchBtns;
        _Console = Console;
    };

    enum ESceneCmd {
        CSCENE_SCENE  = 0x10,
        CSCENE_CLEAR  = 0x11,
        CSCENE_REC    = 0x12,
        CSCENE_RENAME = 0x13,
        CSCENE_PREV   = 0x30,
        CSCENE_NEXT   = 0x31
    };

    enum EState {
        S_IDLE,
        S_MENU,
        S_CLEAR,
        S_CLEAR_ACK,
        S_RECORD,
        S_RENAME,
        S_MENU_IN,
        S_MENU_OUT,
        S_MENU_KEY_BOX
    };

    MenuButton mScene;
    MenuButton mClear;
    MenuButton mRecord;
    MenuButton mRename;
    CTextBox tPage;

    const static uint8_t ScBtnQt = 8;
    SceneButton SButtons[ScBtnQt];
    
    void Refresh();
    void NextPage();
    void PrevPage();
    void ClearSceneNotAck() { _State = S_IDLE; }
    uint8_t GetSceneID(uint8_t localID) { return localID + _ScenePage * ScBtnQt; }

    EState Processing(uint8_t CmdGroup, uint8_t CMD, uint8_t Arg);

    private:
    BaseConsole* _Console;
    WinchShowFile* _ShowFile;
    WinchButton* _WButtons;
    uint8_t _ScenePage = 0;
    EState _State = S_IDLE;

    EState _SceneMenuBtnPrepearing(uint8_t CMD);
    EState _SceneButtonsRecall(uint8_t CMD);
};

class FileManager {
public:  
    FileManager(WinchSD * SD, WinchShowFile* ShowFile, BaseConsole* Console) : 
    fSelect(1280, "pgFiles.fSelect", Console), fFileName("pgFiles.FileName", Console),
    _SD(SD), _Console(Console), _ShowFile(ShowFile) {};

    enum EFileCmd {
        CFILE_NEW     = 0x01,
        CFILE_OPEN    = 0x02,
        CFILE_SAVE    = 0x03,
        CFILE_SAVE_AS = 0x04,
        CFILE_DEL     = 0x05,
        CFILE_CMD     = 0x06,
        CFILE_CANCEL  = 0x07
    };   

    enum EState {
        S_IDLE,
        S_NEW,
        S_OPEN,
        S_SAVE,
        S_SAVE_AS,
        S_SAVE_AS_REWRITE_ACK,
        S_DELETE,
        S_DELETE_ACK
    };

    CTextSelect fSelect;
    CTextBox fFileName;

    EState Processing(uint8_t CmdGroup, uint8_t CMD, uint8_t Arg);
    void Refresh();
    const char * GetCurFileName() { return _CurFName; };

private:
    char _CurFName[SysMaxNameLen] = {};
    char _TempFName[SysMaxNameLen] = {};
    WinchSD * _SD;
    BaseConsole* _Console;
    WinchShowFile* _ShowFile;
    bool _FileSaved = false;
    EState _State = S_IDLE;

    inline bool _FNameEmpty(const char * Name) {return (strncmp(Name, "", SysMaxNameLen) == 0 ? true : false); };
    inline bool _FNameTemplate(const char * Name) {return (strncmp(Name, SysTemplateFileName, SysMaxNameLen) == 0 ? true : false); };
    inline bool _FNameCmp(const char * cstr1, const char * cstr2) {return (strncmp(cstr1, cstr2, SysMaxNameLen) == 0 ? true : false); };
};

#endif