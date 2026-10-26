/**
 * @file fio.hpp
 * @brief Fio — live + main (SD) / bak (W25) через IBrowser.
 *
 * Одна копия шоу в RAM. W25 A/B — от обрыва питания; битый SD → useBak (LKG).
 * openShow: TagMask (default = все, частичный open); save — всегда целиком.
 * Overflow — без bak-fallback; повтор с allowTruncate (как confirmed у save).
 */

#pragma once

#include "browser.hpp"
#include "showFile.hpp"

namespace sf {

class Fio {
public:
    enum class Status : uint8_t {
        Ok = 0,            /**< Live согласован с SD (или newShow). */
        BakOnly,           /**< Live только на bak / LKG. */
        MissingSection,    /**< Нет обязательной секции. */
        InvalidData,       /**< Секция отвергла payload (isValid). */
        Overflow,          /**< Ёмкость секции; IShow::status() == Overflow. */
        OpenFileProtected, /**< Нельзя удалить сейчас открытый файл. */
        BrowserFail,       /**< Каталог. Код — IBrowser::status(). */
        MainFail,          /**< SD IFile. Код — IShow::status(). */
        BakFail,           /**< bak пуст/битый. */
    };

    enum class Event : uint8_t {
        New = 0,
        Loaded,
        Saved,
        Removed,
    };

    Fio(IBrowser& browser, IShow& live, IFile& main, IFile& bak) noexcept
        : _browser(browser)
        , _live(live)
        , _main(main)
        , _bak(bak)
    {}

    Fio(const Fio&) = delete;
    Fio& operator=(const Fio&) = delete;
    virtual ~Fio() = default;

    [[nodiscard]] Status status() const noexcept { return _status; }
    [[nodiscard]] static const char* cstr(Status st) noexcept;

    void newShow() noexcept;

    /**
     * SD → live: open по маске (не merge — остальные секции очищаются; частичный — без имени).
     * Плохо → bak целиком (BakOnly), кроме Overflow / MissingSection до изменения live.
     * @a allowTruncate — как confirmed у saveShowAs.
     */
    [[nodiscard]] bool openShow(const char* name, const TagMask& tags = TagMask::all,
                                bool allowTruncate = false) noexcept;

    /** bak → live целиком, BakOnly. */
    [[nodiscard]] bool useBak() noexcept;

    /** Имя+SD → SD + bak; иначе только bak → BakOnly. Шоу всегда целиком. */
    [[nodiscard]] bool saveShow() noexcept;

    /** tmp → replaceWith на SD, затем bak. Шоу всегда целиком. */
    [[nodiscard]] bool saveShowAs(const char* name, bool confirmed = false) noexcept;

    [[nodiscard]] bool removeShow(const char* name) noexcept;
    [[nodiscard]] static const char* showBaseName(const char* path) noexcept;

protected:
    virtual void onEvent(Event ev) noexcept { (void)ev; }

private:
    [[nodiscard]] bool fail(Status st) noexcept;
    [[nodiscard]] bool ok(Event ev, Status st = Status::Ok) noexcept;

    [[nodiscard]] bool ensureDir() noexcept;
    [[nodiscard]] bool load(const char* path, const TagMask& tags, bool allowTruncate) noexcept;
    [[nodiscard]] bool store(IFile& io, const char* path, Status failSt) noexcept;

    IBrowser& _browser;
    IShow& _live;
    IFile& _main;
    IFile& _bak;
    Status _status = Status::Ok;
};

} // namespace sf
