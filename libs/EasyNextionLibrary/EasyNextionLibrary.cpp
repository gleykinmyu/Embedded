#include "EasyNextionLibrary.hpp"

#include <cstdio>
#include <inttypes.h>

EasyNex::EasyNex(BIF::IHardwareSerial& serial, MillisFn millisFn, DelayFn delayFn)
    : _serial(serial), _millis(millisFn), _delay(delayFn)
{
}

void EasyNex::writeRaw(const uint8_t* data, size_t len)
{
    if (!data || len == 0)
        return;
    size_t off = 0;
    while (off < len) {
        const size_t n = _serial.write(data + off, len - off);
        if (n == 0)
            break;
        off += n;
    }
}

void EasyNex::writeTerm()
{
    static const uint8_t kTerm[] = {0xFF, 0xFF, 0xFF};
    writeRaw(kTerm, sizeof kTerm);
    _serial.flush();
}

void EasyNex::begin(uint32_t baud)
{
    _serial.open(baud);
    if (_delay)
        _delay(100);
    _serial.purge();
}

void EasyNex::writeNum(const char* compName, uint32_t val)
{
    if (!compName)
        return;
    char line[96];
    const int n = snprintf(line, sizeof line, "%s=%" PRIu32, compName, val);
    if (n > 0 && static_cast<size_t>(n) < sizeof line)
        writeRaw(reinterpret_cast<const uint8_t*>(line), static_cast<size_t>(n));
    writeTerm();
}

void EasyNex::writeStr(const char* command)
{
    if (!command)
        return;
    writeRaw(reinterpret_cast<const uint8_t*>(command), strlen(command));
    writeTerm();
}

void EasyNex::writeStr(const char* command, const char* txt)
{
    if (!command)
        return;
    if (!txt) {
        writeStr(command);
        return;
    }
    char line[256];
    const int n = snprintf(line, sizeof line, "%s=\"%s\"", command, txt);
    if (n > 0 && static_cast<size_t>(n) < sizeof line)
        writeRaw(reinterpret_cast<const uint8_t*>(line), static_cast<size_t>(n));
    writeTerm();
}

void EasyNex::drainIncoming(uint32_t timeoutMs)
{
    if (!_millis)
        return;
    _tmr1 = _millis();
    while (_serial.available() > 0) {
        if ((_millis() - _tmr1) > timeoutMs)
            break;
        const size_t before = _serial.available();
        NextionListen();
        if (_serial.available() >= before)
            (void)readOneByte();
    }
}

bool EasyNex::readStr(const char* textComponent, char* buf, size_t bufSize)
{
    if (!textComponent || !buf || bufSize == 0)
        return false;

    drainIncoming(1000);

    writeRaw(reinterpret_cast<const uint8_t*>("get "), 5);
    writeRaw(reinterpret_cast<const uint8_t*>(textComponent), strlen(textComponent));
    writeTerm();

    if (!_millis)
        return false;

    _tmr1 = _millis();
    while (_serial.available() < 4) {
        if ((_millis() - _tmr1) > 400U) {
            if (bufSize)
                buf[0] = '\0';
            return false;
        }
    }

    if (_serial.available() < 4) {
        if (bufSize)
            buf[0] = '\0';
        return false;
    }

    uint8_t sc = 0;
    if (!waitByte(sc, 100U)) {
        if (bufSize)
            buf[0] = '\0';
        return false;
    }

    _tmr1 = _millis();
    while (sc != 0x70U) {
        if (_serial.available() > 0) {
            if (!waitByte(sc, 10U)) {
                if (bufSize)
                    buf[0] = '\0';
                return false;
            }
        }
        if ((_millis() - _tmr1) > 100U) {
            if (bufSize)
                buf[0] = '\0';
            return false;
        }
    }

    size_t out = 0;
    uint8_t endBytes = 0;
    _tmr1 = _millis();
    for (;;) {
        uint8_t ch = 0;
        if (!waitByte(ch, 1000U)) {
            if (bufSize)
                buf[0] = '\0';
            return false;
        }
        if (ch == 0xFFU) {
            endBytes++;
            if (endBytes >= 3U)
                break;
        } else {
            endBytes = 0;
            if (out + 1U >= bufSize) {
                if (bufSize)
                    buf[0] = '\0';
                return false;
            }
            buf[out++] = static_cast<char>(ch);
        }
    }
    buf[out] = '\0';
    return true;
}

uint32_t EasyNex::readNumber(const char* component)
{
    if (!component)
        return 777777U;

    _numberValue = 777777U;
    bool endOfCommandFound = false;

    drainIncoming(1000);

    writeRaw(reinterpret_cast<const uint8_t*>("get "), 5);
    writeRaw(reinterpret_cast<const uint8_t*>(component), strlen(component));
    writeTerm();

    if (!_millis)
        return _numberValue;

    _tmr1 = _millis();
    while (_serial.available() < 8) {
        if ((_millis() - _tmr1) > 400U)
            return _numberValue;
    }

    if (_serial.available() < 8)
        return _numberValue;

    uint8_t lead = 0;
    if (!waitByte(lead, 100U))
        return _numberValue;

    _tmr1 = _millis();
    while (lead != 0x71U) {
        if (_serial.available() > 0) {
            if (!waitByte(lead, 10U))
                return _numberValue;
        }
        if ((_millis() - _tmr1) > 100U)
            return _numberValue;
    }

    for (int i = 0; i < 4; i++) {
        uint8_t b = 0;
        if (!waitByte(b, 200U))
            return _numberValue;
        _numericBuffer[i] = b;
    }

    uint8_t endBytes = 0;
    _tmr1 = _millis();
    while (!endOfCommandFound) {
        uint8_t b = 0;
        if (!waitByte(b, 1000U))
            return 777777U;
        if (b == 0xFFU) {
            endBytes++;
            if (endBytes == 3U)
                endOfCommandFound = true;
        } else {
            return 777777U;
        }
    }

    _numberValue = _numericBuffer[3];
    _numberValue <<= 8;
    _numberValue |= _numericBuffer[2];
    _numberValue <<= 8;
    _numberValue |= _numericBuffer[1];
    _numberValue <<= 8;
    _numberValue |= _numericBuffer[0];
    return _numberValue;
}

int EasyNex::readOneByte()
{
    uint8_t b = 0;
    if (_serial.read(&b, 1) != 1)
        return -1;
    return static_cast<int>(b);
}

bool EasyNex::waitByte(uint8_t& out, uint32_t timeoutMs)
{
    if (!_millis) {
        if (_serial.read(&out, 1) != 1)
            return false;
        return true;
    }
    const uint32_t t0 = _millis();
    while (_serial.available() == 0) {
        if ((_millis() - t0) > timeoutMs)
            return false;
    }
    return _serial.read(&out, 1) == 1;
}

int EasyNex::readByte()
{
    uint8_t b = 0;
    if (!waitByte(b, 100U))
        return -1;
    return static_cast<int>(b);
}

void EasyNex::NextionListen()
{
    if (!_millis)
        return;

    if (_serial.available() <= 2)
        return;

    int r = readOneByte();
    if (r < 0)
        return;
    _start_char = static_cast<char>(r);

    _tmr1 = _millis();
    while (static_cast<unsigned char>(_start_char) != static_cast<unsigned char>('#')) {
        if (_serial.available() > 0) {
            r = readOneByte();
            if (r < 0)
                return;
            _start_char = static_cast<char>(r);
        }
        if ((_millis() - _tmr1) > 100U)
            return;
    }

    uint8_t lenByte = 0;
    if (!waitByte(lenByte, 100U))
        return;
    _len = lenByte;

    _tmr1 = _millis();
    _cmdFound = true;
    while (_serial.available() < _len) {
        if ((_millis() - _tmr1) > 100U) {
            _cmdFound = false;
            break;
        }
    }

    if (!_cmdFound)
        return;

    if (!waitByte(_cmd1, 50U))
        return;

    readCommand();
}

void EasyNex::readCommand()
{
    switch (_cmd1) {
    case 'P':
        lastCurrentPageId = currentPageId;
        {
            uint8_t pid = 0;
            if (waitByte(pid, 50U))
                currentPageId = static_cast<int>(pid);
        }
        break;

    default:
        cmdGroup  = _cmd1;
        cmdLength = _len;
        ReadCustomCMD();
        break;
    }
}
