#include "ConsoleManagers.hpp"
#include "FlashConsts.hpp"


uint8_t WinchButton::_WinchBtnSelectedQt = 0;
uint8_t WinchButton::_BtnSelectedMaxQt = 0;

const CBtnColor WinchButton::_BtnColors[4] = {
    {CLR_WHITE, CLR_BLACK, CLR_BLACK,  CLR_GRAY,  CLR_GRAY},
    {CLR_WHITE, CLR_GRAY,  CLR_GREEN, CLR_WHITE, CLR_WHITE},
    {CLR_WHITE, CLR_GREEN, CLR_GREEN, CLR_WHITE, CLR_WHITE},
    {CLR_WHITE, CLR_B_RED, CLR_B_RED, CLR_GRAY,  CLR_GRAY}
};

void WinchButton::SetState(EState State) {
    switch (State) {
        case S_ENABLED:
        case S_DISABLED:
        case S_BLOCKED: 
            if (_State == S_SELECTED) _WinchBtnSelectedQt--;
            _State = State;
            Refresh(CM_BCO | CM_BCO2 | CM_PCO | CM_PCO2);
        break;

        case S_SELECTED:
            if (_State == S_ENABLED && _WinchBtnSelectedQt < _BtnSelectedMaxQt) {
                _State = S_SELECTED;
                _WinchBtnSelectedQt++;
            }
            Refresh(CM_BCO | CM_BCO2);
        break; 
    }
}

WinchButton::EState WinchButton::GetState() {
    return _State;
}

void WinchButton::Press()
{
  if (_Console->GetState() == BaseConsole::CS_WORK)
  {
    if (GetState() == S_ENABLED)
      SetState(S_SELECTED);
    else if (GetState() == S_SELECTED)
      SetState(S_ENABLED);
  }
  else if (_Console->GetState() == BaseConsole::CS_BLOCK)
  {
    if (GetState() != S_BLOCKED)
      SetState(S_BLOCKED);
    else if (GetState() == S_BLOCKED)
      SetState(S_ENABLED);
    Refresh(CM_BCO | CM_BCO2 | CM_PCO | CM_PCO2);
  }
}

void WinchButton::UnPress()
{
  if (_Console->GetState() == BaseConsole::CS_WORK)
  {
    if (GetState() == S_SELECTED) SetState(S_ENABLED);  
  }
  else if (_Console->GetState() == BaseConsole::CS_BLOCK)
  {
    if (GetState() == S_BLOCKED) {
      SetState(S_ENABLED);
      Refresh(CM_BCO | CM_BCO2 | CM_PCO | CM_PCO2);
    } 
  }
}

void WinchButton::Select() {
    if (GetState() == S_ENABLED)
      SetState(S_SELECTED);
}

void WinchButton::UnSelect() {
  if (GetState() == S_SELECTED)
    SetState(S_ENABLED);
}

//====================================================================================================================
//SCENE MANAGER
//====================================================================================================================

const CBtnColor MenuButton::_BtnColors[2] = {
    {CLR_WHITE, CLR_BLACK,     CLR_BLACK,  CLR_GRAY,  CLR_GRAY},
    {CLR_WHITE, CLR_B_YELLOW,  CLR_GREEN, CLR_WHITE, CLR_WHITE}
};

void MenuButton::SetState(EState State)
{
    if (State == S_BLINKING)
    {
        _tmr.Enable();
        _State = State;
        return;
    }
    else if (_State == S_BLINKING && State == S_ENABLED)
    {
        _tmr.Disable();
    }
    _State = State;
    Refresh();
};

const CBtnColor SceneButton::_BtnColors[2] = {
    {CLR_WHITE, CLR_BLACK,     CLR_BLACK,  CLR_GRAY,  CLR_GRAY},
    {CLR_WHITE, CLR_B_PURPLE,  CLR_GREEN, CLR_WHITE, CLR_WHITE}
};

void SceneManager::Refresh() {
  tPage.SetText(GetFlashStr(static_cast<EFlashStrID>(static_cast<int>(FSTR_CONST_SCENE_PAGE_1) + static_cast<int>(_ScenePage))));
  for (int i = 0; i < ScBtnQt; i++) {
    if (_ShowFile->Scenes[i + 8*_ScenePage].isEmpty())
      SButtons[i].State = SceneButton::S_EMPTY;
    else 
      SButtons[i].State = SceneButton::S_NONEMPTY;

    SButtons[i].SetText(_ShowFile->Scenes[i + 8*_ScenePage].Name);
    SButtons[i].Refresh();
  }
}

void SceneManager::NextPage() {
  _ScenePage = (_ScenePage + 1) % 4;
  Refresh();
}

void SceneManager::PrevPage() {
  _ScenePage = (_ScenePage + ScBtnQt - 1) % 4;
  Refresh();
}

SceneManager::EState SceneManager::_SceneMenuBtnPrepearing(uint8_t CMD)
{
  switch (CMD)
    {
//===============================================НАЖАТИЕ НА КНОПКИ: "СЛЕДУЩАЯ, ПРЕДЫДУЩАЯ СЦЕНА"======================
    case CSCENE_PREV: PrevPage(); break;
    case CSCENE_NEXT: NextPage(); break;

//===============================================НАЖАТИЕ НА КНОПКУ "СЦЕНА", ВХОД В МЕНЮ===============================
    case CSCENE_SCENE:
      if (_State == S_IDLE)
      {
        _State = S_MENU;
        return S_MENU_IN;
      }
      else if (_State == S_MENU)
      {
        _State = S_IDLE;
        return S_MENU_OUT;
      }
      break;

//===============================================НАЖАТИЕ НА КНОПКУ: "СТЕРЕТЬ"===========================================
    case CSCENE_CLEAR:
      if (_State == S_MENU)
      {
        _State = S_CLEAR;

        mClear.SetState(MenuButton::S_BLINKING);
        mRecord.SetState(MenuButton::S_DISABLED);
        mRename.SetState(MenuButton::S_DISABLED);
        mScene.SetState(MenuButton::S_DISABLED);
      }
      else if (_State == S_CLEAR)
      {
        _State = S_MENU;

        mClear.SetState(MenuButton::S_ENABLED);
        mRecord.SetState(MenuButton::S_ENABLED);
        mRename.SetState(MenuButton::S_ENABLED);
        mScene.SetState(MenuButton::S_ENABLED);
      }
      break;

//===============================================НАЖАТИЕ НА КНОПКУ: "ЗАПИСАТЬ"===========================================      
    case CSCENE_REC:
      if (_State == S_MENU)
      {
        _State = S_RECORD;

        mRecord.SetState(MenuButton::S_BLINKING);
        mClear.SetState(MenuButton::S_DISABLED);
        mRename.SetState(MenuButton::S_DISABLED);
        mScene.SetState(MenuButton::S_DISABLED);
      }
      else if (_State == S_RECORD)
      {
        _State = S_MENU;

        mRecord.SetState(MenuButton::S_ENABLED);
        mClear.SetState(MenuButton::S_ENABLED);
        mRename.SetState(MenuButton::S_ENABLED);
        mScene.SetState(MenuButton::S_ENABLED);
      }
      break;

//===============================================НАЖАТИЕ НА КНОПКУ: "ПЕРЕИМЕНОВАТЬ"===========================================
    case CSCENE_RENAME:
      if (_State == S_MENU)
      {
        _State = S_RENAME;

        mRename.SetState(MenuButton::S_BLINKING);
        mRecord.SetState(MenuButton::S_DISABLED);
        mClear.SetState(MenuButton::S_DISABLED);
        mScene.SetState(MenuButton::S_DISABLED);
      }
      else if (_State == S_RENAME)
      {
        _State = S_MENU;

        mRename.SetState(MenuButton::S_ENABLED);
        mRecord.SetState(MenuButton::S_ENABLED);
        mClear.SetState(MenuButton::S_ENABLED);
        mScene.SetState(MenuButton::S_ENABLED);
      }
      break;
    }
  
  return _State;
}

//=======================================================================================================================
//CLICK SCENE BUTTONS IN WORK STATE OF CONSOLE
//======================================================================================================================= 
SceneManager::EState SceneManager::_SceneButtonsRecall(uint8_t CMD)
{
  if (_ShowFile->Scenes[GetSceneID(CMD)].isEmpty())
    return S_IDLE;

  if (_Console->GetState() == BaseConsole::CS_WORK)
  {
    for (int i = 0; i < SysWinchMaxQt; i++)
      _WButtons[i].UnSelect();

    for (uint8_t i = 0; i < SysWinchMaxQt; i++)
      if (_ShowFile->Scenes[GetSceneID(CMD)].WinchSelect[i])
        _WButtons[i].Select();
  }
  else if (_Console->GetState() == BaseConsole::CS_BLOCK)
  {
    for (uint8_t i = 0; i < SysWinchMaxQt; i++)
      if (_ShowFile->Scenes[GetSceneID(CMD)].WinchSelect[i])
        _WButtons[i].Press();
  }
  return S_IDLE;
}

SceneManager::EState SceneManager::Processing(uint8_t CmdGroup, uint8_t CMD, uint8_t Arg)
{
  if (CmdGroup == CGR_SCENE && CMD >= ScBtnQt) return _SceneMenuBtnPrepearing(CMD);
  
  //=======================================================================================================================
  //Нажатие на кнопку сцены в разных режимах работы
  //======================================================================================================================= 
  if (CmdGroup == CGR_SCENE && CMD < ScBtnQt)
  {
    switch (_State)
    {
    case S_IDLE: return _SceneButtonsRecall(CMD);

    case S_RECORD:
      for (uint8_t i = 0; i < SysWinchMaxQt; i++)
      {
        bool RecVar = false;
        if (_WButtons[i].GetState() == WinchButton::S_SELECTED) RecVar = true;
        _ShowFile->Scenes[GetSceneID(CMD)].WinchSelect[i] = RecVar;
      }
      if (_ShowFile->Scenes[GetSceneID(CMD)].isEmpty())
        _ShowFile->Scenes[GetSceneID(CMD)].SetName(GetFlashStr(FSTR_CONST_SCENE_NEWNAME));

      _State = S_IDLE;
      return S_MENU_KEY_BOX;
    
    case S_CLEAR:
      if (_ShowFile->Scenes[GetSceneID(CMD)].isEmpty() == false) {
        _State = S_CLEAR_ACK;
        _Console->ShowMsgBoxID(CMsgBox::MBC_YES_NO, FSTR_MSG_SCENE_CLEAR_ACK, CMD);
        return S_CLEAR_ACK;
      }
    break;

    case S_RENAME:
      if (_ShowFile->Scenes[GetSceneID(CMD)].isEmpty() == false) {
        _State = S_IDLE;
        return S_MENU_KEY_BOX;
      }
    break;

    default: break;
    };
  }

  if (CmdGroup == CGR_SYS)
    switch (_State)
    {
    case S_CLEAR_ACK:
      if (CMD == CMsgBox::MB_YES)
      {
        _State = S_IDLE;
        _ShowFile->Scenes[GetSceneID(Arg)].Clear();
        return S_MENU_OUT;
      }
      else if (CMD == CMsgBox::MB_NO)
      {
        _State = S_IDLE;
        return S_MENU_OUT;
      }
    break;

    default: break;
    }

  return _State;
}

//====================================================================================================================
//FILE MANAGER
//====================================================================================================================

void FileManager::Refresh()
{
  char Str[(SysMaxNameLen + 2) * (SysFileMaxQt + 1)] = {};
  char * StrPtr = Str;
  *StrPtr = '\\';
  StrPtr++;
  *StrPtr = 'r';
  StrPtr++;

  for (uint8_t i = 0; i < SysFileMaxQt; i++) {
    strncpy(StrPtr, _SD->FileList.Files[i].Name, SysMaxNameLen);
    StrPtr += _SD->FileList.Files[i].Length;
    *StrPtr = '\\';
    StrPtr++;
    *StrPtr = 'r';
    StrPtr++;
  }
  fSelect.SetPath(Str);
}

#define SD_IF_ERROR(m)              \
    do {                             \
        if ((m) != SDE_OK) {         \
            _State = S_IDLE;         \
            _Console->ShowMsgBox(CMsgBox::MBC_OK, _SD->GetErrorText(), 0); \
            return _State;           \
        }                            \
    } while (0)

FileManager::EState FileManager::Processing(uint8_t CmdGroup, uint8_t CMD, uint8_t Arg)
{
  if (CmdGroup == CGR_FILE) {
  switch (CMD)
  {

//=================================================================================================================================
//НАЖАТИЕ НА КНОПКИ В МЕНЮ ФАЙЛОВ
//=================================================================================================================================
  case CFILE_NEW:
    _State = S_NEW;
    SD_IF_ERROR(_SD->FileList.Refresh());
    Refresh();
    return _State;
  
  case CFILE_OPEN:
    _State = S_OPEN;
    SD_IF_ERROR(_SD->FileList.Refresh());
    Refresh();
    return _State;
  
  case CFILE_SAVE:
    if (_FNameEmpty(_CurFName)) {
      _Console->ShowMsgBoxID(CMsgBox::MBC_OK, FSTR_MSG_FILENAME_EMPTY, 0);
      return _State;
    }
    if (_FNameTemplate(_CurFName)) {
      _Console->ShowMsgBoxID(CMsgBox::MBC_OK, FSTR_MSG_FILENAME_TEMPLATE, 0);
      return _State;        
    }

    SD_IF_ERROR(_SD->FileSave(_CurFName, _ShowFile));
    _Console->ShowMsgBoxID(CMsgBox::MBC_OK, FSTR_MSG_FILE_SAVED, 0);
    return _State;
  
  case CFILE_SAVE_AS:
    _State = S_SAVE_AS;
    SD_IF_ERROR(_SD->FileList.Refresh());
    Refresh();  
    return _State;
  
  case CFILE_DEL:
    _State = S_DELETE;
    SD_IF_ERROR(_SD->FileList.Refresh());
    Refresh();  
    return _State;

//=================================================================================================================================
//НАЖАТИЕ НА КНОПКУ "КОМАНДА" В БРАУЗЕРЕ ФАЙЛОВ
//=================================================================================================================================

  case CFILE_CMD:
    switch (_State)
    {
    case S_NEW:
      fFileName.GetText(_TempFName, SysMaxNameLen);
      if (_FNameEmpty(_TempFName)) {
        _Console->ShowMsgBoxID(CMsgBox::MBC_OK, FSTR_MSG_FILENAME_EMPTY, 0);
        return _State;
      }
      if (_FNameTemplate(_TempFName)) {
        _Console->ShowMsgBoxID(CMsgBox::MBC_OK, FSTR_MSG_FILENAME_TEMPLATE, 0);
        return _State;        
      }
      if (_FNameCmp(_TempFName, _CurFName)) {
        _Console->ShowMsgBoxID(CMsgBox::MBC_OK, FSTR_MSG_FILE_EXISTS, 0);
        return _State;
      }
      if (_SD->FileExist(_TempFName)) {
        _Console->ShowMsgBoxID(CMsgBox::MBC_OK, FSTR_MSG_FILE_EXISTS, 0);
        return _State;
      }
      strncpy(_CurFName, _TempFName, SysMaxNameLen);

      SD_IF_ERROR(_SD->FileOpen(SysTemplateFileName, _ShowFile));
      SD_IF_ERROR(_SD->FileSave(_CurFName, _ShowFile));
      SD_IF_ERROR(_SD->FileList.Refresh());

      _State = S_IDLE;
      
      _Console->ShowMsgBoxID(CMsgBox::MBC_OK, FSTR_MSG_FILE_SAVED, 0);
      return _State;

    case S_OPEN:
      fSelect.GetText(_TempFName, SysMaxNameLen);
      if (_FNameEmpty(_TempFName)) {
        _Console->ShowMsgBoxID(CMsgBox::MBC_OK, FSTR_MSG_FILENAME_EMPTY, 0);
        return _State;
      }
      if (!_SD->FileExist(_TempFName)) {
        _Console->ShowMsgBoxID(CMsgBox::MBC_OK, FSTR_MSG_FILE_EXISTS, 0);
        return _State;
      }

      strncpy(_CurFName, _TempFName, SysMaxNameLen);

      SD_IF_ERROR(_SD->FileOpen(_CurFName, _ShowFile));

      _State = S_IDLE;
      return _State;

    case S_SAVE_AS:
      fFileName.GetText(_TempFName, SysMaxNameLen);   
      if (_FNameEmpty(_TempFName)) {
        _Console->ShowMsgBoxID(CMsgBox::MBC_OK, FSTR_MSG_FILENAME_EMPTY, 0);
        return _State;
      }
      if (_FNameTemplate(_TempFName)) {
        _Console->ShowMsgBoxID(CMsgBox::MBC_OK, FSTR_MSG_FILENAME_TEMPLATE, 0);
        return _State;        
      }
      if (_SD->FileExist(_TempFName)) {
        _State = S_SAVE_AS_REWRITE_ACK;
        _Console->ShowMsgBoxID(CMsgBox::MBC_YES_NO, FSTR_MSG_FILE_EXISTS_REWRITE, 0);
        return _State;
      }
      
      strncpy(_CurFName, _TempFName, SysMaxNameLen);
      
      SD_IF_ERROR(_SD->FileSave(_CurFName, _ShowFile));
      SD_IF_ERROR(_SD->FileList.Refresh());
      _State = S_IDLE;
      
      _Console->ShowMsgBoxID(CMsgBox::MBC_OK, FSTR_MSG_FILE_SAVED, 0);
      return _State;

    case S_DELETE: 
      fSelect.GetText(_TempFName, SysMaxNameLen);
      if (_FNameEmpty(_TempFName)) {
        _Console->ShowMsgBoxID(CMsgBox::MBC_OK, FSTR_MSG_FILENAME_EMPTY, 0);
        return _State;
      }
      if (_FNameTemplate(_TempFName)) {
        _Console->ShowMsgBoxID(CMsgBox::MBC_OK, FSTR_MSG_FILENAME_TEMPLATE, 0);
        return _State;
      }
      if (_FNameCmp(_CurFName, _TempFName)) {
        _Console->ShowMsgBoxID(CMsgBox::MBC_OK, FSTR_MSG_FILE_DEL_OPENED, 0);
        return _State;
      }
      
      _State = S_DELETE_ACK;
      _Console->ShowMsgBoxID(CMsgBox::MBC_YES_NO, FSTR_MSG_FILE_DEL_ACK, 0);
      return _State;
    break;

    default: break;
    }
  break;  

  case CFILE_CANCEL: _State = S_IDLE; return _State;
  }
  }

  if (CmdGroup == CGR_SYS) {
    switch (_State)
    {
    case S_DELETE_ACK:
       if (CMD == CMsgBox::MB_YES) {
        SD_IF_ERROR(_SD->FileDelete(_TempFName));
        SD_IF_ERROR(_SD->FileList.Refresh());
        _State = S_IDLE;

        _Console->ShowMsgBoxID(CMsgBox::MBC_OK, FSTR_MSG_FILE_DELETED, 0);
      return _State;
      }
      if (CMD == CMsgBox::MB_NO) {
        _State = S_DELETE;
        return _State;
      }
    break;

    case S_SAVE_AS_REWRITE_ACK:
       if (CMD == CMsgBox::MB_YES) {
        SD_IF_ERROR(_SD->FileSave(_TempFName, _ShowFile));
        SD_IF_ERROR(_SD->FileList.Refresh());
        _State = S_IDLE;

        _Console->ShowMsgBoxID(CMsgBox::MBC_OK, FSTR_MSG_FILE_SAVED, 0);
      return _State;
      }
      if (CMD == CMsgBox::MB_NO) {
        _State = S_SAVE_AS;
        return _State;
      }
    break;

    default: if (CMD == CMsgBox::MB_OK) return _State;
    break;
    }
  }
  
  return S_IDLE;
}