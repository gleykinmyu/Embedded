/**
 * @file new_smcp/proto/bitsery.hpp
 * @brief BitSery для кадров: биты с младшего, хвост без префикса длины.
 *
 * putBits — поле [0, max], ширина = сколько бит нужно, чтобы записать max.
 * max — потолок поля, не последний именованный код: op 4 бита при enum до 2.
 * WireTail — остаток буфера как есть. Длину задаёт размер кадра, не байт размера.
 * CanDataTail — байты payload[2..] после 29-битного CAN ID.
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <type_traits>

#include <bitsery/bitsery.h>
#include <bitsery/ext/value_range.h>

#include "comm/payload.hpp"

namespace smcp::proto {

namespace detail {

template <typename A, typename = void>
struct is_bitsery_writer : std::false_type {};

template <typename A>
struct is_bitsery_writer<
    A,
    std::void_t<decltype(std::declval<A&>().template writeBytes<1>(std::uint8_t{}))>>
    : std::true_type {};

} // namespace detail

/** true на Serializer, false на Deserializer. */
template <typename S>
inline constexpr bool isWriter = detail::is_bitsery_writer<
    std::remove_reference_t<decltype(std::declval<S&>().adapter())>>::value;

/** [0, max] в битовый поток. Вызывать из enableBitPacking. */
template <typename S, typename T>
void putBits(S& s, T& v, T max)
{
    s.ext(v, bitsery::ext::ValueRange<T>{T{}, max});
}

/** Сколько байт во входном кадре. Нужно хвосту: конец буфера адаптер сам не отдаёт. */
struct WireCtx {
    std::size_t size = 0;
};

/** Байты Payload до конца буфера, без префикса длины. */
struct WireTail {
    template <typename Ser, typename T, typename Fnc>
    void serialize(Ser& ser, const T& obj, Fnc&&) const
    {
        const comm::PayloadReader in{obj};
        if (in.len() == 0u) {
            return;
        }
        ser.adapter().template writeBuffer<1>(in.data(), in.len());
    }

    template <typename Des, typename T, typename Fnc>
    void deserialize(Des& des, T& obj, Fnc&&) const
    {
        auto& r = des.adapter();
        const auto* ctx = des.template contextOrNull<WireCtx>();
        if (ctx == nullptr || r.error() != bitsery::ReaderError::NoError) {
            if (r.error() == bitsery::ReaderError::NoError) {
                r.error(bitsery::ReaderError::InvalidData);
            }
            return;
        }
        const std::size_t pos = r.currentReadPos();
        if (pos > ctx->size) {
            r.error(bitsery::ReaderError::DataOverflow);
            return;
        }
        const std::size_t n = ctx->size - pos;
        if (n > T::kLenMax) {
            r.error(bitsery::ReaderError::DataOverflow);
            return;
        }
        uint8_t tmp[T::kLenMax];
        if (n != 0u) {
            r.template readBuffer<1>(tmp, n);
        }
        obj.clear();
        if (n != 0u) {
            comm::PayloadWriter{obj}.push(tmp, n);
        }
    }
};

/**
 * Хвост после base::Header: в слове ID уже лежат payload[0] (младшие 4 бита) и payload[1].
 * Здесь только payload[2..]. На чтении payload собирается заново.
 */
struct CanDataTail {
    uint8_t head0 = 0;
    uint8_t head1 = 0;

    template <typename Ser, typename T, typename Fnc>
    void serialize(Ser& ser, const T& obj, Fnc&&) const
    {
        const comm::PayloadReader in{obj};
        if (in.len() <= 2u) {
            return;
        }
        ser.adapter().template writeBuffer<1>(
            in.data() + 2, static_cast<std::size_t>(in.len() - 2u));
    }

    template <typename Des, typename T, typename Fnc>
    void deserialize(Des& des, T& obj, Fnc&&) const
    {
        auto& r = des.adapter();
        const auto* ctx = des.template contextOrNull<WireCtx>();
        if (ctx == nullptr || r.error() != bitsery::ReaderError::NoError) {
            if (r.error() == bitsery::ReaderError::NoError) {
                r.error(bitsery::ReaderError::InvalidData);
            }
            return;
        }
        const std::size_t pos = r.currentReadPos();
        if (pos > ctx->size) {
            r.error(bitsery::ReaderError::DataOverflow);
            return;
        }
        const std::size_t n = ctx->size - pos;
        if (n + 2u > T::kLenMax) {
            r.error(bitsery::ReaderError::DataOverflow);
            return;
        }
        uint8_t tmp[T::kLenMax];
        if (n != 0u) {
            r.template readBuffer<1>(tmp, n);
        }
        obj.clear();
        comm::PayloadWriter w{obj};
        w.push(head0);
        w.push(head1);
        if (n != 0u) {
            w.push(tmp, n);
        }
    }
};

} // namespace smcp::proto

namespace bitsery::traits {

template <typename T>
struct ExtensionTraits<smcp::proto::WireTail, T> {
    using TValue = void;
    static constexpr bool SupportValueOverload = false;
    static constexpr bool SupportObjectOverload = true;
    static constexpr bool SupportLambdaOverload = false;
};

template <typename T>
struct ExtensionTraits<smcp::proto::CanDataTail, T> {
    using TValue = void;
    static constexpr bool SupportValueOverload = false;
    static constexpr bool SupportObjectOverload = true;
    static constexpr bool SupportLambdaOverload = false;
};

} // namespace bitsery::traits
