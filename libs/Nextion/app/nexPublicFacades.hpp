#pragma once

#include "../core/nexCommands.hpp"

namespace nex {

class Application;

/** Фасад `cmd::Eeprom`; объявляется в приложении при необходимости. */
class AppEeprom {
public:
    explicit AppEeprom(Application& a) noexcept;

    /** `wept` / `rept` без transparent — только преамбула. */
    void write(const AttrRef& variableOrConstant, uint32_t eepromStart) const noexcept;
    void read(const AttrRef& destVariable, uint32_t eepromStart) const noexcept;
    /** `wept` / `rept`: преамбула + transparent (`Transaction::Kind::TransparentTx` / `RawDataRx`). */
    void write_t(uint32_t eepromStart, const uint8_t* buffer, uint32_t byteCount) const noexcept;
    void read_t(uint32_t eepromStart, uint8_t* buffer, uint32_t byteCount) const noexcept;

private:
    Application& _app;
};

/** Фасад `cmd::File` и `cmd::Directory`; объявляется в приложении при необходимости. */
class AppFileSystem {
public:
    explicit AppFileSystem(Application& a) noexcept;

    void file_remove(const char* pathQuoted) const noexcept;
    void file_rename(const char* pathFromQuoted, const char* pathToQuoted) const noexcept;
    void file_find(const char* pathQuoted, const AttrRef& dstNumAttr) const noexcept;
    void file_create(const char* pathQuoted, uint32_t reservedSize) const noexcept;
    /** `rdfile`: преамбула + приём transparent (`Transaction::Kind::RawDataRx`). */
    void file_read_t(const char* pathQuoted, uint32_t offset, uint8_t* buffer, uint32_t byteCount, uint32_t crcOption) const noexcept;
    /** `twfile`: преамбула + передача transparent (`Transaction::Kind::TransparentTx`). */
    void file_write_t(const char* pathQuoted, const uint8_t* buffer, uint32_t byteCount) const noexcept;

    void dir_remove(const char* pathQuoted) const noexcept;
    void dir_create(const char* pathQuoted) const noexcept;
    void dir_rename(const char* pathFromQuoted, const char* pathToQuoted) const noexcept;
    void dir_find(const char* pathQuoted, const AttrRef& dstNumAttr) const noexcept;

private:
    Application& _app;
};

/** Один аудиоканал (`play`, `audioN`: stop/pause/resume). */
class AppAudioChannel {
    friend class AppAudio;

    Application& _app;
    uint8_t _id;

    AppAudioChannel(Application& a, uint8_t id) noexcept;

public:
    void play(uint32_t resourceId, uint32_t loop01) const noexcept;
    void stop() const noexcept;
    void pause() const noexcept;
    void resume() const noexcept;
};

/** EQ, громкость и каналы; объявляется в приложении при необходимости. */
class AppAudio {
public:
    explicit AppAudio(Application& a) noexcept;

    void setEqLow(uint8_t level) const noexcept;
    void setEqMid(uint8_t level) const noexcept;
    void setEqHigh(uint8_t level) const noexcept;
    /** Полоса EQ `eq0`…`eq9` (`band` 0…9). */
    void setEqBand(uint8_t band, uint8_t level) const noexcept;
    void setVolume(uint8_t level) const noexcept;

    /** Каналы `audio0` / `audio1`. */
    AppAudioChannel ch[2];

private:
    Application& _app;
};

} // namespace nex
