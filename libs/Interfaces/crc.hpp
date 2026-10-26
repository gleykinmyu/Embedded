/**
 * @file crc.hpp
 * @brief Объекты Crc16Ccitt и Crc32 (ISO-HDLC).
 */

#pragma once

#include <cstddef>
#include <cstdint>

namespace MISC {

/**
 * CRC16-CCITT: poly 0x1021, init 0xFFFF, xorout 0x0000, refin/refout false.
 *
 *   Crc16Ccitt c;
 *   c.update(chunk1, n1);
 *   c.update(chunk2, n2);
 *   const uint16_t v = c.value();
 */
class Crc16Ccitt {
public:
    static constexpr uint16_t kPoly = 0x1021u;
    static constexpr uint16_t kInit = 0xFFFFu;
    static constexpr uint16_t kXorOut = 0x0000u;

    constexpr Crc16Ccitt() noexcept = default;

    void init() noexcept { _crc = kInit; }

    void update(const uint8_t* data, std::size_t len) noexcept
    {
        if (data == nullptr && len != 0u) {
            return;
        }
        uint16_t crc = _crc;
        for (std::size_t i = 0; i < len; ++i) {
            crc = static_cast<uint16_t>(crc ^ (static_cast<uint16_t>(data[i]) << 8));
            for (uint8_t b = 0; b < 8u; ++b) {
                if ((crc & 0x8000u) != 0u) {
                    crc = static_cast<uint16_t>((crc << 1) ^ kPoly);
                } else {
                    crc = static_cast<uint16_t>(crc << 1);
                }
            }
        }
        _crc = crc;
    }

    /** Итоговый CRC (с xorout). Состояние объекта не меняет. */
    [[nodiscard]] constexpr uint16_t value() const noexcept { return static_cast<uint16_t>(_crc ^ kXorOut); }

    /** One-shot: CRC всего буфера. */
    [[nodiscard]] static uint16_t compute(const uint8_t* data, std::size_t len) noexcept
    {
        Crc16Ccitt c;
        c.update(data, len);
        return c.value();
    }

private:
    uint16_t _crc = kInit;
};

/**
 * CRC32 ISO-HDLC: poly 0xEDB88320, init/xorout 0xFFFFFFFF, refin/refout true.
 *
 *   Crc32 c;
 *   c.update(chunk1, n1);
 *   c.update(chunk2, n2);
 *   const uint32_t v = c.value();
 */
class Crc32 {
public:
    static constexpr uint32_t kPoly = 0xEDB88320u;
    static constexpr uint32_t kInit = 0xFFFFFFFFu;
    static constexpr uint32_t kXorOut = 0xFFFFFFFFu;

    constexpr Crc32() noexcept = default;

    void init() noexcept { _crc = kInit; }

    void update(const uint8_t* data, std::size_t len) noexcept
    {
        if (data == nullptr && len != 0u) {
            return;
        }
        uint32_t crc = _crc;
        for (std::size_t i = 0; i < len; ++i) {
            crc ^= data[i];
            for (uint8_t b = 0; b < 8u; ++b) {
                const uint32_t mask = static_cast<uint32_t>(-(static_cast<int32_t>(crc & 1u)));
                crc = (crc >> 1) ^ (kPoly & mask);
            }
        }
        _crc = crc;
    }

    /** Итоговый CRC (с xorout). Состояние объекта не меняет. */
    [[nodiscard]] constexpr uint32_t value() const noexcept { return _crc ^ kXorOut; }

    /** One-shot: CRC всего буфера. */
    [[nodiscard]] static uint32_t compute(const uint8_t* data, std::size_t len) noexcept
    {
        Crc32 c;
        c.update(data, len);
        return c.value();
    }

private:
    uint32_t _crc = kInit;
};

} // namespace MISC
