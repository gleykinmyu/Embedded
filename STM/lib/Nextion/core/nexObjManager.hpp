#pragma once

#include <cstdint>
#include "../comp/nexComponentBase.hpp"
#include "../nexGateway.hpp"
#include "nexFifo.hpp"

namespace nex {

/**
 * Центр управления объектами HMI: поток байтов → `Gateway`, страницы и их компоненты.
 * Регистрируйте каждую `Page` через `registerPage`; входящие сообщения — `dispatchMessage` или `poll`.
 */
class ObjManager {
public:
    explicit ObjManager(BIF::IByteStream& stream) noexcept;

    Gateway& gateway() noexcept { return _gateway; }
    const Gateway& gateway() const noexcept { return _gateway; }

    /**
     * Привязка страницы к индексу `page.ID` (как на панели Nextion).
     * @return false — слот уже занят другим экземпляром `Page`.
     */
    bool registerPage(Page& page) noexcept;

    [[nodiscard]] Page* page(uint8_t id) noexcept { return _pages[id]; }
    [[nodiscard]] const Page* page(uint8_t id) const noexcept { return _pages[id]; }

    /** Последний индекс из кадра 0x66; `0xFF`, пока события смены страницы не было. */
    uint8_t currentPageId() const noexcept { return _currentPageId; }

    /** Разбор `Message`: touch 0x65 → `Page::dispatchTouch`, page 0x66 → `currentPageId`. */
    void dispatchMessage(const Message& m) noexcept;

    /**
     * Один шаг приёма из `Gateway::receive` и маршрутизация при успешном кадре.
     * @return false — полного кадра ещё нет (как у `Gateway::receive`).
     */
    bool poll(Message& m) noexcept;

private:
    Gateway _gateway;
    Page* _pages[256]{};
    uint8_t _currentPageId = 0xFF;
};

} // namespace nex
