/**
 * @file fiomanager.hpp
 * @brief FIOManager — live/staging + main/bak на носителях, каталог через IBrowser.
 *
 * IFile не внутри IShowFile: шоу — RAM, IFile — ручка тома. Live пишется в оба слота.
 * Один и тот же IFile на main и bak — резерва нет (сравнение по адресу).
 * Пустой path у load — читать резерв. Резерв пишется только в save.
 */

#pragma once

#include "smcp/Console/browser.hpp"
#include "smcp/Console/show_model.hpp"

namespace smcp {
namespace file {

class FIOManager {
public:
    enum class Status : uint8_t {
        Ok = 0,
        NoShowOpen,        /**< Save без открытого имени. */
        MissingSection,    /**< После load нет обязательной секции. */
        InvalidData,       /**< Секция отвергла payload (isValid). */
        OpenFileProtected, /**< Нельзя удалить сейчас открытый файл. */
        BrowserFail,       /**< Каталог. Код — IBrowser::status(). */
        MainFail,          /**< Основной IFile. Код — IShowFile::status(). */
        BakFail,           /**< Резерв. Код — IShowFile::status(). */
        RestoreFail,       /**< Не удалось прочитать резерв. */
    };

    /** Успешная операция; наследник (пульт) обновляет сессию / UI. */
    enum class Event : uint8_t {
        New = 0,  /**< Live очищен, edited. */
        Loaded,   /**< open / restore приняты. */
        Saved,    /**< Запись на носитель. */
        Removed,  /**< Файл каталога удалён, live не трогали. */
    };

    FIOManager(IBrowser& browser, IShowFile& live, IShowFile& incoming, IFile& main,
               IFile& bak) noexcept
        : _browser(browser)
        , _live(live)
        , _incoming(incoming)
        , _main(main)
        , _bak(bak)
    {}

    FIOManager(const FIOManager&) = delete;
    FIOManager& operator=(const FIOManager&) = delete;
    virtual ~FIOManager() = default;

    [[nodiscard]] Status status() const noexcept { return _status; }
    [[nodiscard]] static const char* cstr(Status st) noexcept;

    void newShow() noexcept;
    [[nodiscard]] bool restore() noexcept;
    [[nodiscard]] bool openShow(const char* name) noexcept;
    [[nodiscard]] bool saveShow() noexcept;
    /**
     * Сохранить как @a name: запись в tmp, replaceWith, затем bak.
     * Файл уже есть и !@a confirmed → BrowserFail / FileExists.
     */
    [[nodiscard]] bool saveShowAs(const char* name, bool confirmed = false) noexcept;
    [[nodiscard]] bool removeShow(const char* name) noexcept;
    [[nodiscard]] static const char* showBaseName(const char* path) noexcept;

protected:
    /** Только после успеха. По умолчанию no-op. */
    virtual void onEvent(Event ev) noexcept { (void)ev; }

private:
    [[nodiscard]] bool fail(Status st) noexcept
    {
        _status = st;
        return false;
    }
    [[nodiscard]] bool succeed(Event ev) noexcept
    {
        _status = Status::Ok;
        onEvent(ev);
        return true;
    }

    [[nodiscard]] bool acceptLoaded() noexcept;
    [[nodiscard]] bool ensureDir() noexcept;
    [[nodiscard]] bool hasBak() const noexcept { return &_main != &_bak; }
    [[nodiscard]] bool loadShows(const char* path) noexcept;
    [[nodiscard]] bool saveMain(const char* path) noexcept;
    [[nodiscard]] bool syncBak() noexcept;

    IBrowser& _browser;
    IShowFile& _live;
    IShowFile& _incoming;
    IFile& _main;
    IFile& _bak;
    Status _status = Status::Ok;
};

} // namespace file
} // namespace smcp
