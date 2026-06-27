/*!
 * EasyNextionLibrary.h — Nextion HMI без Arduino (BIF::IHardwareSerial).
 */
#pragma once

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include "../Interfaces/iserial.hpp"

class EasyNex {
public:
    using MillisFn = uint32_t (*)();
    using DelayFn  = void (*)(uint32_t ms);

    EasyNex(BIF::IHardwareSerial& serial, MillisFn millisFn, DelayFn delayFn);

    void begin(uint32_t baud = 9600U);

    void writeNum(const char* compName, uint32_t val);
    /// Только команда (например "page pgWork") — без кавычек.
    void writeStr(const char* command);
    /// Атрибут текста: component + значение в кавычках.
    void writeStr(const char* command, const char* txt);

    void NextionListen();

    uint32_t readNumber(const char* component);
    /// Читает текст компонента; при ошибке возвращает false, иначе null-terminated строка в buf.
    bool readStr(const char* textComponent, char* buf, size_t bufSize);

    int readByte();

    virtual void ReadCustomCMD() {}

    int         currentPageId     = 0;
    int         lastCurrentPageId = 0;
    uint8_t     cmdGroup          = 0;
    uint8_t     cmdLength          = 0;

private:
    BIF::IHardwareSerial& _serial;
    MillisFn              _millis = nullptr;
    DelayFn               _delay  = nullptr;

    void readCommand();

    int  readOneByte();
    bool waitByte(uint8_t& out, uint32_t timeoutMs);
    void writeRaw(const uint8_t* data, size_t len);
    void writeTerm();

    void drainIncoming(uint32_t timeoutMs);

    char          _start_char = 0;
    uint32_t      _tmr1      = 0;
    bool          _cmdFound   = false;
    uint8_t       _cmd1      = 0;
    uint8_t       _len        = 0;

    uint8_t       _numericBuffer[4]{};
    uint32_t      _numberValue = 0;
};
