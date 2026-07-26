/**
 * @file w25q_show_file.hpp
 * @brief IFile на последнем 4K-секторе W25Q (зеркало шоу = SMCP, имя в Header::name).
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>

#include "board.hpp"
#include "impl/w25q.hpp"
#include "iFileSystem.hpp"
#include "smcp/debug.hpp"
#include "smcp/group.hpp"
#include "smcp/show_file.hpp"

namespace smcp {
namespace file {

class W25qShowFile final : public BIF::IFile {
public:
    static constexpr uint32_t kSectorSize = PHL::W25Q::kSectorSize;
    static constexpr uint32_t kSectorAddr = PHL::W25Q::kSize - PHL::W25Q::kSectorSize;

    static_assert(sizeof(Header) + 2u * sizeof(SectionDesc)
                          + 8u /* SETT: MConsole::kSettingsWireSize */
                          + smcp::kGroupMaxCount * sizeof(smcp::Group)
                      <= kSectorSize,
                  "SMCP show must fit in one W25Q sector");

    explicit W25qShowFile(PHL::W25Q& flash) noexcept
        : _flash(flash)
    {}

    [[nodiscard]] bool open(const char* /*path*/, bool for_write) noexcept override
    {
        close();
        _forWrite = for_write;
        _pos = 0u;
        _dirty = false;

        if (for_write) {
            std::memset(_buf, 0xFF, sizeof(_buf));
            _size = 0u;
            _open = true;
            SMCP_DBG("[SMCP] W25Q open write sector=0x%06lX\n",
                static_cast<unsigned long>(kSectorAddr));
            return true;
        }

        board.watchdog.kick();
        if (!_flash.read(kSectorAddr, _buf, sizeof(_buf))) {
            SMCP_DBG("[SMCP] W25Q open read fail @0x%06lX\n",
                static_cast<unsigned long>(kSectorAddr));
            return false;
        }

        const auto& hdr = *reinterpret_cast<const Header*>(_buf);
        if (hdr.magic == kMagic && hdr.total_size > 0u && hdr.total_size <= kSectorSize) {
            _size = static_cast<std::size_t>(hdr.total_size);
        } else {
            /* Дать Reader прочитать заголовок и вернуть BadMagic/BadVersion/… */
            _size = sizeof(Header);
        }
        _open = true;
        SMCP_DBG("[SMCP] W25Q open read size=%u\n", static_cast<unsigned>(_size));
        return true;
    }

    void close() noexcept override
    {
        /* Не auto-sync: flush только через sync() из Writer::finalize.
         * Иначе close-on-error повторно жжёт сектор / пишет хвост без заголовка. */
        if (_open) {
            SMCP_DBG("[SMCP] W25Q close dirty=%u size=%u\n", _dirty ? 1u : 0u,
                static_cast<unsigned>(_size));
        }
        _open = false;
        _forWrite = false;
        _dirty = false;
        _pos = 0u;
        _size = 0u;
    }

    [[nodiscard]] bool read(uint8_t* data, std::size_t size) noexcept override
    {
        if (!_open || data == nullptr || _pos + size > _size) {
            return false;
        }
        std::memcpy(data, _buf + _pos, size);
        _pos += size;
        return true;
    }

    [[nodiscard]] bool write(const uint8_t* data, std::size_t size) noexcept override
    {
        if (!_open || !_forWrite || data == nullptr) {
            return false;
        }
        if (_pos + size > kSectorSize) {
            return false;
        }
        std::memcpy(_buf + _pos, data, size);
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
        const std::size_t lim = _forWrite ? kSectorSize : _size;
        if (offset > lim) {
            return false;
        }
        _pos = offset;
        return true;
    }

    [[nodiscard]] bool sync() noexcept override
    {
        if (!_open || !_forWrite) {
            return _open;
        }
        if (_size == 0u) {
            _dirty = false;
            return true;
        }
        /* После erase хвост сектора = 0xFF; достаточно запрограммировать SMCP. */
        board.watchdog.kick();
        SMCP_DBG("[SMCP] W25Q sync @0x%06lX len=%u\n",
            static_cast<unsigned long>(kSectorAddr), static_cast<unsigned>(_size));
        if (!_flash.eraseSector(kSectorAddr)) {
            SMCP_DBG("[SMCP] W25Q erase FAIL @0x%06lX sr=0x%02X\n",
                static_cast<unsigned long>(kSectorAddr),
                static_cast<unsigned>(_flash.readStatus1()));
            return false;
        }
        board.watchdog.kick();
        if (!_flash.program(kSectorAddr, _buf, _size)) {
            SMCP_DBG("[SMCP] W25Q program FAIL @0x%06lX len=%u sr=0x%02X\n",
                static_cast<unsigned long>(kSectorAddr), static_cast<unsigned>(_size),
                static_cast<unsigned>(_flash.readStatus1()));
            return false;
        }
        board.watchdog.kick();
        _dirty = false;
        SMCP_DBG("[SMCP] W25Q sync OK len=%u\n", static_cast<unsigned>(_size));
        return true;
    }

    [[nodiscard]] bool isOpen() const noexcept override { return _open; }
    [[nodiscard]] std::size_t tell() const noexcept override { return _pos; }
    [[nodiscard]] std::size_t size() const noexcept override { return _size; }

private:
    PHL::W25Q& _flash;
    alignas(Header) uint8_t _buf[kSectorSize]{};
    std::size_t _size = 0u;
    std::size_t _pos = 0u;
    bool _open = false;
    bool _forWrite = false;
    bool _dirty = false;
};

} // namespace file
} // namespace smcp
