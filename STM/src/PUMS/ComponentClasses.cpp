#include "ComponentClasses.hpp"
#include "../../lib/debug/Debug.h"
#include <cstdio>

static void appendObjBase(char* buf, size_t cap, const char* name, int nameId)
{
    if (nameId >= 0)
        snprintf(buf, cap, "%s%d", name, nameId);
    else
        snprintf(buf, cap, "%s", name);
}

uint32_t CObject::GetCompID()
{
    char base[40];
    appendObjBase(base, sizeof base, _Name, _NameID);
    char line[48];
    snprintf(line, sizeof line, "%s.id", base);
    return _Console->readNumber(line);
}

void CTextBox::SetText(const char* Text)
{
    char comp[48];
    appendObjBase(comp, sizeof comp, _Name, _NameID);
    strncat(comp, ".txt", sizeof comp - strlen(comp) - 1U);
    _Console->writeStr(comp, Text);
}

void CTextBox::GetText(char* Text, unsigned Size)
{
    char comp[48];
    appendObjBase(comp, sizeof comp, _Name, _NameID);
    strncat(comp, ".txt", sizeof comp - strlen(comp) - 1U);
    (void)_Console->readStr(comp, Text, Size);
}

void CTextBox::SetPos(NexPos Pos)
{
    char comp[40];
    appendObjBase(comp, sizeof comp, _Name, _NameID);

    char line[48];
    snprintf(line, sizeof line, "%s.x", comp);
    _Console->writeNum(line, Pos.x);

    snprintf(line, sizeof line, "%s.y", comp);
    _Console->writeNum(line, Pos.y);
}

void CBaseButton::Refresh(const CBtnColor& Color, uint8_t Mask)
{
    char full[40];
    appendObjBase(full, sizeof full, _Name, _NameID);

    char line[48];
    if (Mask & CM_BORDER) {
        snprintf(line, sizeof line, "%s.borderc", full);
        _Console->writeNum(line, Color.borderc);
    }
    if (Mask & CM_BCO) {
        snprintf(line, sizeof line, "%s.bco", full);
        _Console->writeNum(line, Color.bco);
    }
    if (Mask & CM_BCO2) {
        snprintf(line, sizeof line, "%s.bco2", full);
        _Console->writeNum(line, Color.bco2);
    }
    if (Mask & CM_PCO) {
        snprintf(line, sizeof line, "%s.pco", full);
        _Console->writeNum(line, Color.pco);
    }
    if (Mask & CM_PCO2) {
        snprintf(line, sizeof line, "%s.pco2", full);
        _Console->writeNum(line, Color.pco2);
    }
}

void CTextSelect::SetPath(const char* Path)
{
    checkPTR(Path);
    if (strlen(Path) > _PathLen) {
        debugMSG("Length of file name list more than need");
        return;
    }

    char comp[48];
    appendObjBase(comp, sizeof comp, _Name, _NameID);
    strncat(comp, ".path", sizeof comp - strlen(comp) - 1U);
    _Console->writeStr(comp, Path);
}

void CTextSelect::SetVal(uint8_t Val)
{
    char comp[48];
    appendObjBase(comp, sizeof comp, _Name, _NameID);
    strncat(comp, ".val", sizeof comp - strlen(comp) - 1U);
    _Console->writeNum(comp, Val);
}

uint8_t CTextSelect::GetVal()
{
    char comp[48];
    appendObjBase(comp, sizeof comp, _Name, _NameID);
    strncat(comp, ".val", sizeof comp - strlen(comp) - 1U);
    return static_cast<uint8_t>(_Console->readNumber(comp) % 0x100U);
}

void CTimer::Enable()
{
    char comp[48];
    appendObjBase(comp, sizeof comp, _Name, _NameID);
    strncat(comp, ".en", sizeof comp - strlen(comp) - 1U);
    _Console->writeNum(comp, 1);
}

void CTimer::Disable()
{
    char comp[48];
    appendObjBase(comp, sizeof comp, _Name, _NameID);
    strncat(comp, ".en", sizeof comp - strlen(comp) - 1U);
    _Console->writeNum(comp, 0);
}

const NexPos CMsgBox::_BtnPositions[4] = {
    NexPos(240, 525),
    NexPos(165, 525),
    NexPos(315, 525),
    NexPos(1030, 525),
};

void CMsgBox::Show(EMsgBtnCfg Buttons, const char* Msg, uint8_t RtnCode)
{
    checkPTR(Msg);
    _RtnCode = RtnCode;
    _PrepareButtons(Buttons);
    _Console->SetState(BaseConsole::CS_MSGBOX);

    _tMsg.SetText(Msg);
    _Console->writeStr("page MsgBox");
}

void CMsgBox::_PrepareButtons(EMsgBtnCfg Buttons)
{
    switch (Buttons) {
    case MBC_OK:
        _bOK.SetPos(_BtnPositions[BP_CENTER]);
        _bYes.SetPos(_BtnPositions[BP_NOT_USED]);
        _bNo.SetPos(_BtnPositions[BP_NOT_USED]);
        _bCancel.SetPos(_BtnPositions[BP_NOT_USED]);
        break;

    case MBC_OK_CANCEL:
        _bOK.SetPos(_BtnPositions[BP_LEFT]);
        _bYes.SetPos(_BtnPositions[BP_NOT_USED]);
        _bNo.SetPos(_BtnPositions[BP_NOT_USED]);
        _bCancel.SetPos(_BtnPositions[BP_RIGHT]);
        break;

    case MBC_YES_NO:
        _bOK.SetPos(_BtnPositions[BP_NOT_USED]);
        _bYes.SetPos(_BtnPositions[BP_LEFT]);
        _bNo.SetPos(_BtnPositions[BP_RIGHT]);
        _bCancel.SetPos(_BtnPositions[BP_NOT_USED]);
        break;
    }
}
