#include "nexObjManager.hpp"

#include <type_traits>

namespace nex {

ObjManager::ObjManager(BIF::IByteStream& stream) noexcept : _gateway(stream) {}

bool ObjManager::registerPage(Page& page) noexcept {
    const uint8_t id = page.ID;
    Page* const slot = _pages[id];
    if (slot != nullptr && slot != &page)
        return false;
    _pages[id] = &page;
    return true;
}

void ObjManager::dispatchMessage(const Message& m) noexcept {
    std::visit(
        [this](auto&& arg) {
            using T = std::decay_t<decltype(arg)>;
            if constexpr (std::is_same_v<T, msg::TouchCompEvent>) {
                if (Page* const p = _pages[arg.page_id])
                    p->dispatchTouch(arg);
            } else if constexpr (std::is_same_v<T, msg::PageEvent>) {
                _currentPageId = arg.page_id;
            }
        },
        m);
}

bool ObjManager::poll(Message& m) noexcept {
    if (!_gateway.receive(m))
        return false;
    dispatchMessage(m);
    return true;
}

} // namespace nex
