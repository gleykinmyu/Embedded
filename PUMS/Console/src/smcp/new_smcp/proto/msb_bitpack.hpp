/**
 * @file new_smcp/proto/msb_bitpack.hpp
 * @brief MSB-first битовый адаптер Bitsery.
 *
 * Внутри байта бит 7 идёт первым, дальше к биту 0. Следующий байт — больший индекс.
 * В поле первым пишется его старший бит. Между полями выравнивания нет.
 * Деструктор байт не добивает: один адаптер держит курсор на всю цепочку заголовков.
 * align() и flush() добивают текущий байт явно.
 *
 * writeBits — числовое поле MSB-first, без Endianness.
 * writeBytes/valueNb — байты с учётом Endianness конфигурации, с текущей битовой позиции.
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <type_traits>

#include <bitsery/bitsery.h>
#include <bitsery/details/adapter_common.h>

namespace smcp::proto::msb {

enum class WriteError : uint8_t {
    NoError = 0,
    BufferOverflow,
    InvalidBits,
};

namespace detail {

inline constexpr uint64_t lowMask(size_t bits) noexcept
{
    if (bits >= 64u) {
        return ~uint64_t{0};
    }
    return (uint64_t{1} << bits) - 1u;
}

} // namespace detail

template <typename Config = bitsery::DefaultConfig>
class OutputAdapter {
public:
    using BitPackingEnabled = OutputAdapter<Config>;
    using TConfig = Config;
    using TValue = uint8_t;

    OutputAdapter(uint8_t* data, size_t size) noexcept
        : _data(data)
        , _cap(size)
    {
    }

    template <size_t SIZE, typename T>
    void writeBytes(const T& value)
    {
        static_assert(std::is_integral<T>::value, "");
        static_assert(sizeof(T) == SIZE, "");
        T stored = value;
        if (bitsery::details::ShouldSwap<Config, T>::value) {
            stored = bitsery::details::swap(stored);
        }
        const auto* bytes = reinterpret_cast<const uint8_t*>(&stored);
        for (size_t i = 0; i < SIZE; ++i) {
            writeBits(bytes[i], 8u);
        }
    }

    template <size_t SIZE, typename T>
    void writeBuffer(const T* buf, size_t count)
    {
        static_assert(std::is_integral<T>::value, "");
        static_assert(sizeof(T) == SIZE, "");
        for (size_t i = 0; i < count; ++i) {
            writeBytes<SIZE>(buf[i]);
        }
    }

    template <typename T>
    void writeBits(const T& value, size_t bits)
    {
        static_assert(std::is_unsigned<T>::value, "");
        if (_err != WriteError::NoError) {
            return;
        }
        if (bits == 0u || bits > sizeof(T) * 8u) {
            fail(WriteError::InvalidBits);
            return;
        }
        uint64_t v = static_cast<uint64_t>(value) & detail::lowMask(bits);
        while (bits > 0u) {
            const size_t inByte = _bits % 8u;
            const size_t space = 8u - inByte;
            const size_t take = bits < space ? bits : space;
            const size_t shift = bits - take;
            const uint64_t chunk = (v >> shift) & detail::lowMask(take);
            const size_t index = _bits / 8u;
            if (index >= _cap) {
                fail(WriteError::BufferOverflow);
                return;
            }
            const unsigned destLow = static_cast<unsigned>(space - take);
            const auto placed = static_cast<uint8_t>(chunk << destLow);
            if (inByte == 0u) {
                _data[index] = placed;
            } else {
                _data[index] = static_cast<uint8_t>(_data[index] | placed);
            }
            if (shift == 0u) {
                v = 0;
            } else {
                v &= detail::lowMask(shift);
            }
            _bits += take;
            bits -= take;
        }
    }

    void align()
    {
        const size_t inByte = _bits % 8u;
        if (inByte == 0u) {
            return;
        }
        writeBits(uint8_t{0}, 8u - inByte);
    }

    void flush() { align(); }

    void currentWritePos(size_t pos)
    {
        align();
        if (pos > _cap) {
            fail(WriteError::BufferOverflow);
            return;
        }
        _bits = pos * 8u;
    }

    size_t currentWritePos() const noexcept { return _bits / 8u; }

    size_t writtenBytesCount() const noexcept { return (_bits + 7u) / 8u; }

    size_t bitPos() const noexcept { return _bits; }

    WriteError error() const noexcept { return _err; }

    void reset(uint8_t* data, size_t size) noexcept
    {
        _data = data;
        _cap = size;
        _bits = 0;
        _err = WriteError::NoError;
    }

private:
    void fail(WriteError err) noexcept
    {
        if (_err == WriteError::NoError) {
            _err = err;
        }
    }

    uint8_t* _data = nullptr;
    size_t _cap = 0;
    size_t _bits = 0;
    WriteError _err = WriteError::NoError;
};

template <typename Config = bitsery::DefaultConfig>
class InputAdapter {
public:
    using BitPackingEnabled = InputAdapter<Config>;
    using TConfig = Config;
    using TValue = uint8_t;

    InputAdapter(const uint8_t* data, size_t size) noexcept
        : _data(data)
        , _cap(size)
    {
    }

    template <size_t SIZE, typename T>
    void readBytes(T& value)
    {
        static_assert(std::is_integral<T>::value, "");
        static_assert(sizeof(T) == SIZE, "");
        uint8_t bytes[8]{};
        for (size_t i = 0; i < SIZE; ++i) {
            uint8_t b = 0;
            readBits(b, 8u);
            bytes[i] = b;
        }
        T stored{};
        std::memcpy(&stored, bytes, SIZE);
        if (bitsery::details::ShouldSwap<Config, T>::value) {
            stored = bitsery::details::swap(stored);
        }
        value = stored;
    }

    template <size_t SIZE, typename T>
    void readBuffer(T* buf, size_t count)
    {
        static_assert(std::is_integral<T>::value, "");
        static_assert(sizeof(T) == SIZE, "");
        for (size_t i = 0; i < count; ++i) {
            readBytes<SIZE>(buf[i]);
        }
    }

    template <typename T>
    void readBits(T& value, size_t bits)
    {
        static_assert(std::is_unsigned<T>::value, "");
        if (_err != bitsery::ReaderError::NoError) {
            value = {};
            return;
        }
        if (bits == 0u || bits > sizeof(T) * 8u) {
            fail(bitsery::ReaderError::InvalidData);
            value = {};
            return;
        }
        uint64_t acc = 0;
        size_t left = bits;
        while (left > 0u) {
            const size_t inByte = _bits % 8u;
            const size_t space = 8u - inByte;
            const size_t take = left < space ? left : space;
            const size_t index = _bits / 8u;
            if (index >= _cap) {
                fail(bitsery::ReaderError::DataOverflow);
                value = {};
                return;
            }
            const unsigned destLow = static_cast<unsigned>(space - take);
            const uint8_t chunk = static_cast<uint8_t>(
                (_data[index] >> destLow) & static_cast<uint8_t>(detail::lowMask(take)));
            acc = (acc << take) | chunk;
            _bits += take;
            left -= take;
        }
        value = static_cast<T>(acc);
    }

    void align()
    {
        const size_t inByte = _bits % 8u;
        if (inByte == 0u) {
            return;
        }
        uint8_t pad = 0;
        readBits(pad, 8u - inByte);
        if (TConfig::CheckDataErrors && pad != 0u) {
            fail(bitsery::ReaderError::InvalidData);
        }
    }

    void currentReadPos(size_t pos)
    {
        if (pos > _cap) {
            fail(bitsery::ReaderError::DataOverflow);
            return;
        }
        _bits = pos * 8u;
    }

    size_t currentReadPos() const noexcept { return (_bits + 7u) / 8u; }

    void currentReadEndPos(size_t pos) noexcept { _end = pos; }

    size_t currentReadEndPos() const noexcept { return _end == 0u ? _cap : _end; }

    size_t bitPos() const noexcept { return _bits; }

    size_t bitsRemaining() const noexcept
    {
        const size_t total = _cap * 8u;
        return _bits >= total ? 0u : total - _bits;
    }

    bool isCompletedSuccessfully() const noexcept
    {
        return _err == bitsery::ReaderError::NoError && currentReadPos() == _cap && _bits == _cap * 8u;
    }

    bitsery::ReaderError error() const noexcept { return _err; }

    void error(bitsery::ReaderError err) noexcept { fail(err); }

private:
    void fail(bitsery::ReaderError err) noexcept
    {
        if (_err == bitsery::ReaderError::NoError) {
            _err = err;
        }
    }

    const uint8_t* _data = nullptr;
    size_t _cap = 0;
    size_t _end = 0;
    size_t _bits = 0;
    bitsery::ReaderError _err = bitsery::ReaderError::NoError;
};

/** Счётчик бит без буфера. Размер в байтах — общий, без округления каждого поля. */
template <typename Config = bitsery::DefaultConfig>
class MeasureAdapter {
public:
    using BitPackingEnabled = MeasureAdapter<Config>;
    using TConfig = Config;
    using TValue = uint8_t;

    template <size_t SIZE, typename T>
    void writeBytes(const T&)
    {
        static_assert(std::is_integral<T>::value, "");
        static_assert(sizeof(T) == SIZE, "");
        addBits(SIZE * 8u);
    }

    template <size_t SIZE, typename T>
    void writeBuffer(const T*, size_t count)
    {
        static_assert(std::is_integral<T>::value, "");
        static_assert(sizeof(T) == SIZE, "");
        addBits(SIZE * 8u * count);
    }

    template <typename T>
    void writeBits(const T&, size_t bits)
    {
        static_assert(std::is_unsigned<T>::value, "");
        if (bits == 0u || bits > sizeof(T) * 8u) {
            if (_err == WriteError::NoError) {
                _err = WriteError::InvalidBits;
            }
            return;
        }
        addBits(bits);
    }

    void align()
    {
        const size_t inByte = _bits % 8u;
        if (inByte != 0u) {
            _bits += 8u - inByte;
        }
    }

    void flush() { align(); }

    void currentWritePos(size_t pos)
    {
        align();
        _bits = pos * 8u;
    }

    size_t currentWritePos() const noexcept { return _bits / 8u; }

    size_t writtenBytesCount() const noexcept { return (_bits + 7u) / 8u; }

    size_t bitPos() const noexcept { return _bits; }

    WriteError error() const noexcept { return _err; }

private:
    void addBits(size_t bits) noexcept
    {
        if (_err == WriteError::NoError) {
            _bits += bits;
        }
    }

    size_t _bits = 0;
    WriteError _err = WriteError::NoError;
};

} // namespace smcp::proto::msb
