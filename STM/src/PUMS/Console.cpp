#include "Console.hpp"
#include "FlashConsts.hpp"

#include <stm32f4xx_hal.h>

#include <cstdio>

const uint8_t TouchConsole::_UDBtnColorBitMask[8][3] = {
    {0, 0, 0},
    {1, 0, 0},
    {1, 1, 0},
    {0, 1, 0},
    {0, 1, 1},
    {0, 0, 1},
    {1, 0, 1},
    {1, 1, 1},
};

void TouchConsole::RefreshWinchButtons()
{
    for (int i = 0; i < _BtnQt; i++)
        WButtons[i].Refresh();
}

void TouchConsole::ClearSelection()
{
    for (int i = 0; i < _BtnQt; i++)
        WButtons[i].UnPress();
}

void TouchConsole::SetUDBtnColor(EUDBtn btn, EUDBtnClr Color)
{
    switch (btn) {
    case BTN_UP:
        writeNum("pio0", _UDBtnColorBitMask[Color][0]);
        writeNum("pio1", _UDBtnColorBitMask[Color][1]);
        writeNum("pio2", _UDBtnColorBitMask[Color][2]);
        break;

    case BTN_DOWN:
        writeNum("pio3", _UDBtnColorBitMask[Color][0]);
        writeNum("pio4", _UDBtnColorBitMask[Color][1]);
        writeNum("pio5", _UDBtnColorBitMask[Color][2]);
        break;
    }
}

void TouchConsole::SetState(ECState vState)
{
    if (_State == vState)
        return;
    if (_State == CS_MSGBOX && vState != CS_MSGBOX_PROC)
        return;

    switch (vState) {
    case CS_STARTUP: break;

    case CS_IDLE:
        writeStr("page pgWait");
        SetUDBtnColor(BTN_DOWN, UDC_BLACK);
        SetUDBtnColor(BTN_UP, UDC_BLACK);
        _State = vState;
        break;

    case CS_WORK:
        if (_State == CS_BLOCK) {
            tmrBlock.Disable();
            sBlock.Refresh(CBtnColor(CLR_B_RED, CLR_B_RED, CLR_B_RED, CLR_B_RED, CLR_B_RED), CBaseButton::CM_BCO);
        } else {
            writeStr("page pgWork");
        }
        SceneMngr.Refresh();
        _State = vState;
        break;

    case CS_BLOCK:
        if (_State == CS_WORK) {
            tmrBlock.Enable();
            _State = vState;
        }
        break;

    case CS_EDIT:
        writeStr("page mScn");
        _State = vState;
        break;

    case CS_FILE:
        writeStr("page pgFiles");
        _State = vState;
        break;

    case CS_MSGBOX:
    case CS_MSGBOX_PROC:
        _State = vState;
        break;
    }

    SetStateText();
}

void TouchConsole::SetStateText()
{
    char line[256];
    const char* st = "";
    switch (_State) {
    case CS_IDLE:
        st = "IDLE  | ";
        break;
    case CS_WORK:
        st = "WORK  | ";
        break;
    case CS_BLOCK:
        st = "BLOCK | ";
        break;
    case CS_EDIT:
        st = "EDIT  | ";
        break;
    case CS_FILE:
        st = "FILE  | ";
        break;
    case CS_MSGBOX:
        st = "MSGBOX| ";
        break;
    default:
        st = "      | ";
        break;
    }
    snprintf(line, sizeof line, "%sShow: %s |", st, FileMngr.GetCurFileName());
    tStatus.SetText(line);
}

void TouchConsole::begin(unsigned long baud)
{
    EasyNex::begin(static_cast<uint32_t>(baud));
    SetState(CS_IDLE);
}

void TouchConsole::Processing()
{
    NextionListen();
    if ((currentPageId == 1) & ((HAL_GetTick() - _WorkPageTime) > 60000U))
        SetState(CS_IDLE);
}

void TouchConsole::ReadCustomCMD()
{
    _WorkPageTime = HAL_GetTick();
    uint8_t CMD      = 0;
    uint8_t SceneRtn = 0;

    switch (cmdGroup) {
    case CGR_SYS:
        CMD = static_cast<uint8_t>(readByte());

        switch (CMD) {
        case CSYS_WAIT_BTN:
            writeStr("page pgWork");
            SetState(CS_WORK);

            SetUDBtnColor(BTN_DOWN, UDC_GREEN);
            SetUDBtnColor(BTN_UP, UDC_RED);
            HAL_Delay(100);
            RefreshWinchButtons();
            SceneMngr.Refresh();
            break;

        case CSYS_KEYBOARD_RTN:
            if (GetState() == CS_EDIT) {
                HAL_Delay(200);
                (void)readStr("pgWork.varScnName.txt", _ScnBtnRenameText, sizeof _ScnBtnRenameText);
                ShowFile.Scenes[SceneMngr.GetSceneID(_ScnBtnRenameID)].SetName(_ScnBtnRenameText);
                SceneMngr.SButtons[_ScnBtnRenameID].SetText(_ScnBtnRenameText);
                SetState(CS_WORK);
                SceneMngr.Refresh();
            }
            break;
        case CSYS_KEYBOARD_CLOSE:
            if (GetState() == CS_EDIT) {
                SetState(CS_WORK);
                SceneMngr.Refresh();
            }
            break;
        case CSYS_MSGBOX_OK:
        case CSYS_MSGBOX_YES:
        case CSYS_MSGBOX_NO:
        case CSYS_MSGBOX_CANCEL:
            MsgBox.Processing(cmdGroup, CMD, 0xFF);

            if (SceneMngr.Processing(cmdGroup, CMD, MsgBox.GetRtnCode()) == SceneManager::EState::S_MENU_OUT) {
                SetState(CS_WORK);
                SceneMngr.Refresh();
            }
            if (FileMngr.Processing(cmdGroup, CMD, MsgBox.GetRtnCode()))
                SetState(CS_FILE);
            else
                SetState(CS_WORK);
            break;
        }
        break;

    case CGR_WINCH_BTN:
        CMD = static_cast<uint8_t>(readByte());
        if (CMD < _BtnQt) {
            WButtons[CMD].Press();
        } else if (CMD == CW_CLEAR) {
            ClearSelection();
        } else if (CMD == CW_BLOCK) {
            if (GetState() == CS_WORK)
                SetState(CS_BLOCK);
            else if (GetState() == CS_BLOCK)
                SetState(CS_WORK);
        }
        break;

    case CGR_SCENE:
        CMD      = static_cast<uint8_t>(readByte());
        SceneRtn = SceneMngr.Processing(cmdGroup, CMD, 0xFF);

        if (SceneRtn == SceneManager::EState::S_MENU_IN)
            SetState(CS_EDIT);
        else if (SceneRtn == SceneManager::EState::S_MENU_OUT)
            SetState(CS_WORK);
        else if (SceneRtn == SceneManager::EState::S_MENU_KEY_BOX) {
            _ScnBtnRenameID = CMD;
            writeStr("pgWork.varScnName.txt", ShowFile.Scenes[SceneMngr.GetSceneID(CMD)].Name);
            keybd.Open(1, 45);
            break;
        }
        SceneMngr.Refresh();
        break;

    case CGR_FILE:
        CMD = static_cast<uint8_t>(readByte());
        if (FileMngr.Processing(cmdGroup, CMD, 0xFF) > FileManager::S_IDLE)
            SetState(CS_FILE);
        else
            SetState(CS_WORK);
        break;
    }
}

static char _FlashStr[FLASH_STR_LENGTH] = {};

const char* GetFlashStr(EFlashStrID ID)
{
    if (ID >= 0 && ID < FSTR_QT) {
        strncpy(_FlashStr, _PGM_FlashStr[ID], FLASH_STR_LENGTH - 1U);
        _FlashStr[FLASH_STR_LENGTH - 1U] = '\0';
    } else {
        _FlashStr[0] = '\0';
    }
    return _FlashStr;
}