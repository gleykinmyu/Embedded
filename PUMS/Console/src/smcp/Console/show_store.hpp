/**
 * @file show_store.hpp
 * @brief ShowStore — live/staging + main/bak на носителях, каталог через IBrowser.
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

class ShowStore {
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

    ShowStore(IBrowser& browser, IShowFile& live, IShowFile& incoming, IFile& main,
              IFile& bak) noexcept
        : _browser(browser)
        , _live(live)
        , _incoming(incoming)
        , _main(main)
        , _bak(bak)
    {}

    ShowStore(const ShowStore&) = delete;
    ShowStore& operator=(const ShowStore&) = delete;

    [[nodiscard]] Status status() const noexcept { return _status; }

    void newShow() noexcept;
    [[nodiscard]] bool restore() noexcept;
    [[nodiscard]] bool openShow(const char* name) noexcept;
    [[nodiscard]] bool saveShow() noexcept;
    [[nodiscard]] bool saveShowAs(const char* name, bool confirmed = false) noexcept;
    [[nodiscard]] bool removeShow(const char* name) noexcept;

    [[nodiscard]] static const char* showBaseName(const char* path) noexcept;

private:
    [[nodiscard]] bool fail(Status st) noexcept
    {
        _status = st;
        return false;
    }
    [[nodiscard]] bool ok() noexcept
    {
        _status = Status::Ok;
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
