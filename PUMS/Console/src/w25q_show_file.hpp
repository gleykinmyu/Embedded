/**
 * @file w25q_show_file.hpp
 * @brief IFile — LKG/зеркало шоу (SMCP) на W25Q; для sf::Fio это обычный bak.
 *
 * A/B ping-pong в конце W25Q (magic + seq + CRC16). Слот = kSectorsPerSlot × 4K;
 * сейчас 1 сектор/слот. Без секторного RAM-буфера: read/CRC — стрим; write —
 * erase inactive на open, program по write(); sync дописывает SlotHdr.
 * Обрыв на inactive не портит active. Fio про A/B не знает.
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>

#include "board.hpp"
#include "crc.hpp"
#include "debug.hpp"
#include "impl/w25q.hpp"
#include "iFileSystem.hpp"
#include "showFile.hpp"
#include "smcp/show/axis_pool.hpp"
#include "smcp/show/group.hpp"

namespace smcp {
namespace file {

class W25qShowFile final : public BIF::IFile {
public:
    static constexpr uint32_t kSectorSize = PHL::W25Q::kSectorSize;
    /** Секторов на один A/B-слот (рост payload → увеличить; erase на open × N). */
    static constexpr uint32_t kSectorsPerSlot = 1u;
    static constexpr uint32_t kSlotBytes = kSectorsPerSlot * kSectorSize;
    static constexpr uint32_t kSlotCount = 2u;
    /** Хвост flash под A+B. */
    static constexpr uint32_t kRegionBytes = kSlotCount * kSlotBytes;
    static_assert(kRegionBytes <= PHL::W25Q::kSize, "W25Q A/B region exceeds chip");

    /** Слот A — ниже; B — у конца чипа. */
    static constexpr uint32_t kSlotAddrA = PHL::W25Q::kSize - kRegionBytes;
    static constexpr uint32_t kSlotAddrB = kSlotAddrA + kSlotBytes;

    /** @deprecated старое имя; физически = слот B. */
    static constexpr uint32_t kSectorAddr = kSlotAddrB;

    /** Заголовок слота A/B (перед SMCP payload). */
    struct SlotHdr {
        uint32_t magic = 0;   /**< kSlotMagic («LKGS») — валидный слот. */
        uint32_t seq = 0;     /**< Монотонный счётчик; больше → новее (ping-pong). */
        uint32_t size = 0;    /**< Длина SMCP payload после заголовка, байт. */
        uint16_t crc16 = 0;   /**< CRC16-CCITT по payload (init 0xFFFF). */
        uint16_t reserved = 0; /**< Выравнивание до 16 байт; писать 0. */
    };
    static_assert(sizeof(SlotHdr) == 16u);

    static constexpr uint32_t kSlotMagic = 0x53474B4Cu; /**< "LKGS". */
    static constexpr uint32_t kPayloadMax = kSlotBytes - sizeof(SlotHdr);
    static constexpr std::size_t kCrcChunk = 64u;

    static_assert(sizeof(sf::Header) + 4u * sizeof(sf::SectionDesc)
                          + 8u /* SETT */
                          + smcp::show::kShowTypeCount * sizeof(smcp::show::ShowType)
                          + smcp::show::kPatchCount * sizeof(smcp::show::Patch)
                          + smcp::kGroupMaxCount * sizeof(smcp::Group)
                      <= kPayloadMax,
                  "SMCP show must fit in W25Q A/B slot payload");

    explicit W25qShowFile(PHL::W25Q& flash) noexcept
        : _flash(flash)
    {}

    [[nodiscard]] static constexpr uint32_t slotAddr(uint8_t index) noexcept
    {
        return index == 0u ? kSlotAddrA : kSlotAddrB;
    }

    [[nodiscard]] bool open(const char* /*path*/, bool for_write) noexcept override
    {
        close();
        _forWrite = for_write;
        _pos = 0u;
        _dirty = false;

        if (!probeActive()) {
            _active = 0u;
            _seq = 0u;
            _size = 0u;
        }

        if (for_write) {
            _writeSlot = static_cast<uint8_t>(1u - _active);
            if (!eraseSlot(_writeSlot)) {
                return false;
            }
            _size = 0u;
            _open = true;
            SF_DBG("[SF] W25Q open write active=%u seq=%lu → slot=%u\n",
                static_cast<unsigned>(_active),
                static_cast<unsigned long>(_seq),
                static_cast<unsigned>(_writeSlot));
            return true;
        }

        _open = true;
        SF_DBG("[SF] W25Q open read active=%u seq=%lu size=%u\n",
            static_cast<unsigned>(_active),
            static_cast<unsigned long>(_seq),
            static_cast<unsigned>(_size));
        return true;
    }

    void close() noexcept override
    {
        if (_open) {
            SF_DBG("[SF] W25Q close dirty=%u size=%u\n", _dirty ? 1u : 0u,
                static_cast<unsigned>(_size));
        }
        _open = false;
        _forWrite = false;
        _dirty = false;
        _pos = 0u;
        /* _size/_active/_seq оставляем — следующий open write знает LKG. */
    }

    [[nodiscard]] bool read(uint8_t* data, std::size_t size) noexcept override
    {
        if (!_open || data == nullptr || _pos + size > _size) {
            return false;
        }
        if (size == 0u) {
            return true;
        }
        /* В режиме записи читаем уже прошитое в inactive-слот (read-back). */
        const uint8_t slot = _forWrite ? _writeSlot : _active;
        board.watchdog.kick();
        if (!_flash.read(payloadAddr(slot) + static_cast<uint32_t>(_pos), data, size)) {
            return false;
        }
        _pos += size;
        return true;
    }

    [[nodiscard]] bool write(const uint8_t* data, std::size_t size) noexcept override
    {
        if (!_open || !_forWrite || data == nullptr) {
            return false;
        }
        if (_pos + size > kPayloadMax) {
            return false;
        }
        if (size != 0u) {
            const uint32_t addr =
                slotAddr(_writeSlot) + static_cast<uint32_t>(sizeof(SlotHdr) + _pos);
            board.watchdog.kick();
            if (!_flash.program(addr, data, size)) {
                return false;
            }
        }
        _pos += size;
        if (_pos > _size) {
            _size = _pos;
        }
        _dirty = true;
        return true;
    }

    [[nodiscard]] bool seek(std::size_t offset) noexcept override
    {
        if (!_open) {
            return false;
        }
        const std::size_t lim = _forWrite ? kPayloadMax : _size;
        if (offset > lim) {
            return false;
        }
        _pos = offset;
        return true;
    }

    /**
     * Дописывает SlotHdr в уже стёртый/прошитый inactive (CRC — стрим payload).
     * Успех → слот активен; обрыв оставляет старый LKG.
     */
    [[nodiscard]] bool sync() noexcept override
    {
        if (!_open || !_forWrite) {
            return _open;
        }
        if (_size == 0u) {
            _dirty = false;
            return true;
        }
        if (_size > kPayloadMax) {
            return false;
        }

        const uint32_t addr = slotAddr(_writeSlot);
        const uint32_t next_seq = _seq + 1u;
        uint16_t crc = 0;
        if (!crcPayload(addr + sizeof(SlotHdr), _size, crc)) {
            return false;
        }

        SlotHdr hdr{};
        hdr.magic = kSlotMagic;
        hdr.seq = next_seq;
        hdr.size = static_cast<uint32_t>(_size);
        hdr.crc16 = crc;
        hdr.reserved = 0;

        board.watchdog.kick();
        SF_DBG("[SF] W25Q sync → slot=%u @0x%06lX seq=%lu len=%u\n",
            static_cast<unsigned>(_writeSlot),
            static_cast<unsigned long>(addr),
            static_cast<unsigned long>(next_seq),
            static_cast<unsigned>(_size));

        if (!_flash.program(addr, reinterpret_cast<const uint8_t*>(&hdr), sizeof(hdr))) {
            SF_DBG("[SF] W25Q program hdr FAIL @0x%06lX sr=0x%02X\n",
                static_cast<unsigned long>(addr),
                static_cast<unsigned>(_flash.readStatus1()));
            return false;
        }
        board.watchdog.kick();

        _active = _writeSlot;
        _seq = next_seq;
        _dirty = false;
        SF_DBG("[SF] W25Q sync OK active=%u seq=%lu\n",
            static_cast<unsigned>(_active),
            static_cast<unsigned long>(_seq));
        return true;
    }

    [[nodiscard]] bool isOpen() const noexcept override { return _open; }
    [[nodiscard]] std::size_t tell() const noexcept override { return _pos; }
    [[nodiscard]] std::size_t size() const noexcept override { return _size; }

    [[nodiscard]] uint8_t activeSlot() const noexcept { return _active; }
    [[nodiscard]] uint32_t activeSeq() const noexcept { return _seq; }

private:
    struct SlotInfo {
        bool ok = false;
        uint32_t seq = 0;
        uint32_t size = 0;
    };

    [[nodiscard]] static constexpr uint32_t payloadAddr(uint8_t index) noexcept
    {
        return slotAddr(index) + static_cast<uint32_t>(sizeof(SlotHdr));
    }

    /** Стереть все сектора слота (kSectorsPerSlot). */
    [[nodiscard]] bool eraseSlot(uint8_t index) noexcept
    {
        const uint32_t base = slotAddr(index);
        for (uint32_t i = 0u; i < kSectorsPerSlot; ++i) {
            const uint32_t addr = base + i * kSectorSize;
            board.watchdog.kick();
            if (!_flash.eraseSector(addr)) {
                SF_DBG("[SF] W25Q erase FAIL @0x%06lX sr=0x%02X\n",
                    static_cast<unsigned long>(addr),
                    static_cast<unsigned>(_flash.readStatus1()));
                return false;
            }
        }
        return true;
    }

    [[nodiscard]] bool crcPayload(uint32_t addr, std::size_t len, uint16_t& out) noexcept
    {
        MISC::Crc16Ccitt crc;
        uint8_t chunk[kCrcChunk]{};
        std::size_t left = len;
        uint32_t at = addr;
        while (left > 0u) {
            const std::size_t n = (left < kCrcChunk) ? left : kCrcChunk;
            board.watchdog.kick();
            if (!_flash.read(at, chunk, n)) {
                return false;
            }
            crc.update(chunk, n);
            at += static_cast<uint32_t>(n);
            left -= n;
        }
        out = crc.value();
        return true;
    }

    [[nodiscard]] bool probeSlot(uint8_t index, SlotInfo& info) noexcept
    {
        info = {};
        const uint32_t addr = slotAddr(index);
        SlotHdr hdr{};
        board.watchdog.kick();
        if (!_flash.read(addr, &hdr, sizeof(hdr))) {
            SF_DBG("[SF] W25Q read FAIL slot=%u @0x%06lX\n",
                static_cast<unsigned>(index),
                static_cast<unsigned long>(addr));
            return false;
        }

        if (hdr.magic != kSlotMagic || hdr.size == 0u || hdr.size > kPayloadMax) {
            return false;
        }
        uint16_t crc = 0;
        if (!crcPayload(addr + sizeof(SlotHdr), hdr.size, crc) || crc != hdr.crc16) {
            return false;
        }
        info.ok = true;
        info.seq = hdr.seq;
        info.size = hdr.size;
        SF_DBG("[SF] W25Q slot=%u seq=%lu size=%lu\n",
            static_cast<unsigned>(index),
            static_cast<unsigned long>(info.seq),
            static_cast<unsigned long>(info.size));
        return true;
    }

    [[nodiscard]] bool probeActive() noexcept
    {
        SlotInfo a{};
        SlotInfo b{};
        const bool ok_a = probeSlot(0, a);
        const bool ok_b = probeSlot(1, b);
        if (!ok_a && !ok_b) {
            return false;
        }

        if (ok_a && (!ok_b || a.seq >= b.seq)) {
            _active = 0u;
            _seq = a.seq;
            _size = a.size;
        } else {
            _active = 1u;
            _seq = b.seq;
            _size = b.size;
        }
        return true;
    }

    PHL::W25Q& _flash;
    std::size_t _size = 0u;
    std::size_t _pos = 0u;
    uint32_t _seq = 0u;
    uint8_t _active = 0u;
    uint8_t _writeSlot = 0u;
    bool _open = false;
    bool _forWrite = false;
    bool _dirty = false;
};

} // namespace file
} // namespace smcp
