#ifndef SDCARD_H
#define SDCARD_H

#include <cstring>

#include "System.hpp"

extern "C" {
#include "ff.h"
}

#define FLASH_ERRSTR_LENGTH 48

#define FLASH_ERR_LIST \
    FLASH_STRING(OK,              "SD CARD: ALL OK") \
    FLASH_STRING(BEGIN,           "SD CARD: Error open SD CARD.") \
    FLASH_STRING(DIR_OPEN,        "SD CARD: Error open directory.") \
    FLASH_STRING(FILE_QT,         "SD CARD: File quantity more, than 64.") \
    FLASH_STRING(FILE_OPEN,       "SD CARD: Error file opening") \
    FLASH_STRING(FILE_NOT_EXISTS, "SD CARD: File not exists") \
    FLASH_STRING(FILE_READ,       "SD CARD: Error read file.") \
    FLASH_STRING(FILE_WRITE,      "SD CARD: Error write file.") \
    FLASH_STRING(FILE_DELETE,     "SD CARD: Error delete file") \
    FLASH_STRING(UNKNOWN,         "SD CARD: Unknown error.") \

enum ESDErrors {
#define FLASH_STRING(m, s) SDE_##m,
    FLASH_ERR_LIST SDE_QT
#undef FLASH_STRING
};

static const char _ErrorStrings[][FLASH_ERRSTR_LENGTH] = {
#define FLASH_STRING(m, s) s,
    FLASH_ERR_LIST "End of list"
#undef FLASH_STRING
};



const char* _GetErrorStr(ESDErrors Err);

class WinchSD {
public:
    explicit WinchSD(FATFS* fs, const char* volumeRoot);

    class SD_FileName {
    public:
        char     Name[SysMaxNameLen] = {};
        uint8_t  Length              = 0;
        void     ClearName()
        {
            memset(Name, 0, sizeof Name);
            Length = 0;
        }
    };

    class SD_FileList {
    public:
        explicit SD_FileList(WinchSD* sd) : _SD(sd) {}

        SD_FileName Files[SysFileMaxQt];
        uint8_t     FilesQt = 0;

        void     ClearNames()
        {
            for (uint8_t i = 0; i < SysFileMaxQt; i++)
                Files[i].ClearName();
        }
        ESDErrors Refresh();

    private:
        WinchSD* _SD;
    };

    ESDErrors FileOpen(const char* FileName, WinchShowFile* File);
    ESDErrors FileSave(const char* FileName, WinchShowFile* File);
    ESDErrors FileDelete(const char* FileName);
    bool      FileExist(const char* FileName);

    const char* GetErrorText();

    SD_FileList FileList;

private:
    ESDErrors   _Error = SDE_OK;
    FATFS*      _fs    = nullptr;
    const char* _vol   = nullptr;

    bool        makePath(char* out, size_t outLen, const char* fileName) const;
    ESDErrors   mapFatResult(FRESULT fr) const;

    static char KOI8R_to_ASCII_7bit(char c);
    static char ASCII_7bit_to_KOI8R(char c);
    static void ConvertStrToASCII(char* Str, size_t Size);
    static void ConvertStrFromASCII(char* Str, size_t Size);
};

#endif
