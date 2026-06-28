#include "SDCard.hpp"

#include <cstdio>
#include <cstring>

bool WinchSD::makePath(char* out, size_t outLen, const char* fileName) const
{
    if (!out || outLen == 0 || !_vol || !fileName)
        return false;
    const int n = snprintf(out, outLen, "%s%s", _vol, fileName);
    return n > 0 && static_cast<size_t>(n) < outLen;
}

ESDErrors WinchSD::mapFatResult(FRESULT fr) const
{
    switch (fr) {
    case FR_OK:
        return SDE_OK;
    case FR_NO_FILE:
    case FR_INVALID_NAME:
        return SDE_FILE_NOT_EXISTS;
    case FR_NO_PATH:
    case FR_NOT_READY:
    case FR_DISK_ERR:
    case FR_NOT_ENABLED:
        return SDE_BEGIN;
    case FR_DENIED:
    case FR_WRITE_PROTECTED:
        return SDE_FILE_WRITE;
    case FR_INT_ERR:
    case FR_INVALID_OBJECT:
    case FR_INVALID_PARAMETER:
    default:
        return SDE_UNKNOWN;
    }
}

WinchSD::WinchSD(FATFS* fs, const char* volumeRoot) : FileList(this), _fs(fs), _vol(volumeRoot)
{
}

ESDErrors WinchSD::SD_FileList::Refresh()
{
    _SD->_Error = SDE_OK;
    ClearNames();

    if (_SD->_fs == nullptr || _SD->_vol == nullptr) {
        _SD->_Error = SDE_BEGIN;
        return _SD->_Error;
    }

    char root[8] = {};
    if (snprintf(root, sizeof root, "%s", _SD->_vol) <= 0) {
        _SD->_Error = SDE_BEGIN;
        return _SD->_Error;
    }

    DIR     dir{};
    FILINFO fno{};
    uint8_t fileQt = 0;

    FRESULT fr = f_opendir(&dir, root);
    if (fr != FR_OK) {
        _SD->_Error = (fr == FR_NO_PATH || fr == FR_NOT_READY) ? SDE_BEGIN : SDE_DIR_OPEN;
        return _SD->_Error;
    }

    for (;;) {
        fr = f_readdir(&dir, &fno);
        if (fr != FR_OK || fno.fname[0] == 0)
            break;
        if (fno.fattrib & AM_DIR)
            continue;

        memset(Files[fileQt].Name, 0, sizeof Files[fileQt].Name);
        strncpy(Files[fileQt].Name, fno.fname, SysMaxNameLen - 1U);
        Files[fileQt].Length = static_cast<uint8_t>(strnlen(Files[fileQt].Name, SysMaxNameLen));

        WinchSD::ConvertStrFromASCII(Files[fileQt].Name, SysMaxNameLen);

        fileQt++;
        if (fileQt == SysFileMaxQt) {
            _SD->_Error = SDE_FILE_QT;
            break;
        }
    }

    f_closedir(&dir);
    FilesQt = fileQt;
    return _SD->_Error;
}

ESDErrors WinchSD::FileOpen(const char* FileName, WinchShowFile* File)
{
    char     path[SysMaxNameLen + 8] = {};
    char     name[SysMaxNameLen + 1] = {};
    _Error                             = SDE_OK;

    if (!File || !FileName) {
        _Error = SDE_UNKNOWN;
        return _Error;
    }

    strncpy(name, FileName, SysMaxNameLen);
    ConvertStrToASCII(name, SysMaxNameLen);

    if (!makePath(path, sizeof path, name)) {
        _Error = SDE_UNKNOWN;
        return _Error;
    }

    FIL fil{};
    FRESULT fr = f_open(&fil, path, FA_READ);
    if (fr != FR_OK) {
        _Error = (fr == FR_NO_FILE) ? SDE_FILE_NOT_EXISTS : SDE_FILE_OPEN;
        return _Error;
    }

    UINT br = 0;
    fr       = f_read(&fil, File, sizeof(WinchShowFile), &br);
    f_close(&fil);

    if (fr != FR_OK || br != sizeof(WinchShowFile)) {
        _Error = SDE_FILE_READ;
        return _Error;
    }

    return _Error;
}

ESDErrors WinchSD::FileSave(const char* FileName, WinchShowFile* File)
{
    char path[SysMaxNameLen + 8]  = {};
    char name[SysMaxNameLen + 1] = {};
    _Error                        = SDE_OK;

    if (!File || !FileName) {
        _Error = SDE_UNKNOWN;
        return _Error;
    }

    strncpy(name, FileName, SysMaxNameLen);
    ConvertStrToASCII(name, SysMaxNameLen);

    if (!makePath(path, sizeof path, name)) {
        _Error = SDE_UNKNOWN;
        return _Error;
    }

    (void)f_unlink(path);

    FIL fil{};
    FRESULT fr = f_open(&fil, path, FA_WRITE | FA_CREATE_ALWAYS);
    if (fr != FR_OK) {
        _Error = SDE_FILE_OPEN;
        return _Error;
    }

    UINT bw = 0;
    fr      = f_write(&fil, File, sizeof(WinchShowFile), &bw);
    f_sync(&fil);
    f_close(&fil);

    if (fr != FR_OK || bw != sizeof(WinchShowFile)) {
        _Error = SDE_FILE_WRITE;
        return _Error;
    }

    return _Error;
}

ESDErrors WinchSD::FileDelete(const char* FileName)
{
    char path[SysMaxNameLen + 8]  = {};
    char name[SysMaxNameLen + 1] = {};
    _Error                        = SDE_OK;

    strncpy(name, FileName, SysMaxNameLen);
    ConvertStrToASCII(name, SysMaxNameLen);

    if (!makePath(path, sizeof path, name)) {
        _Error = SDE_UNKNOWN;
        return _Error;
    }

    FRESULT fr = f_unlink(path);
    if (fr == FR_NO_FILE) {
        _Error = SDE_FILE_NOT_EXISTS;
        return _Error;
    }
    if (fr != FR_OK) {
        _Error = SDE_FILE_DELETE;
        return _Error;
    }

    return _Error;
}

bool WinchSD::FileExist(const char* FileName)
{
    char path[SysMaxNameLen + 8]  = {};
    char name[SysMaxNameLen + 1] = {};
    _Error                        = SDE_OK;

    strncpy(name, FileName, SysMaxNameLen);
    ConvertStrToASCII(name, SysMaxNameLen);

    if (!makePath(path, sizeof path, name)) {
        _Error = SDE_UNKNOWN;
        return false;
    }

    FILINFO fno{};
    const FRESULT fr = f_stat(path, &fno);
    if (fr != FR_OK) {
        _Error = mapFatResult(fr);
        return false;
    }
    return (fno.fattrib & AM_DIR) == 0;
}

const char* WinchSD::GetErrorText() { return _GetErrorStr(_Error); }

inline char WinchSD::KOI8R_to_ASCII_7bit(char c)
{
    const auto uc = static_cast<unsigned char>(c);

    if (uc <= 0x3FU)
        return c;

    if (uc >= 0xC0U && uc <= 0xFEU)
        return static_cast<char>(uc & 0x7FU);

    return '$';
}

inline char WinchSD::ASCII_7bit_to_KOI8R(char c)
{
    const auto uc = static_cast<unsigned char>(c);

    if (uc <= 0x3FU)
        return c;

    if (uc >= 0x40U && uc <= 0x7EU)
        return static_cast<char>(uc | 0x80U);

    return '$';
}

void WinchSD::ConvertStrToASCII(char* Str, size_t Size)
{
    for (size_t i = 0; i < Size && Str[i] != '\0'; i++)
        Str[i] = KOI8R_to_ASCII_7bit(Str[i]);
}

void WinchSD::ConvertStrFromASCII(char* Str, size_t Size)
{
    for (size_t i = 0; i < Size && Str[i] != '\0'; i++)
        Str[i] = ASCII_7bit_to_KOI8R(Str[i]);
}

static char _ErrorStr[FLASH_ERRSTR_LENGTH] = {};

const char* _GetErrorStr(ESDErrors Err)
{
    if (Err >= 0 && Err < SDE_QT) {
        strncpy(_ErrorStr, _ErrorStrings[Err], FLASH_ERRSTR_LENGTH - 1U);
        _ErrorStr[FLASH_ERRSTR_LENGTH - 1U] = '\0';
    } else {
        strncpy(_ErrorStr, _ErrorStrings[SDE_UNKNOWN], FLASH_ERRSTR_LENGTH - 1U);
        _ErrorStr[FLASH_ERRSTR_LENGTH - 1U] = '\0';
    }
    return _ErrorStr;
}