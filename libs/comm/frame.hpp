/**
 * @file comm/frame.hpp
 * @brief comm::Frame — статическая композиция заголовка и вложенного объекта.
 *
 * Header хранится по значению, Payload — невладеющей ссылкой.
 * Временный Payload привязать нельзя: его время жизни короче Frame.
 * Frame живёт только на время синхронного send. Транспорт и протоколы
 * не сохраняют ссылки на него после возврата из send.
 * Составной кадр через Frame не читается: приём идёт последовательным чтением.
 */

#pragma once

#include <type_traits>
#include <utility>

namespace comm {

template <typename Header, typename Payload>
struct Frame {
    Header hdr;
    const Payload& payload;

    Frame(Header header, const Payload& body) noexcept(std::is_nothrow_move_constructible<Header>::value)
        : hdr(std::move(header))
        , payload(body)
    {}

    Frame(Header, Payload&&) = delete;

    Frame(const Frame&) = delete;
    Frame& operator=(const Frame&) = delete;

    template <typename S>
    void serialize(S& s)
    {
        s.object(hdr);
        s.object(payload);
    }
};

template <typename Header, typename Payload>
Frame(Header, const Payload&) -> Frame<std::decay_t<Header>, Payload>;

} // namespace comm
