#pragma once
#include <stddef.h>
#include <stdint.h>
#include <stm32f4xx_hal.h>
#include "ibyte_stream.hpp"
#include "core/gpio.h"

namespace PHL {

/**
 * Winbond W25Q16 (16 Mbit = 2 MiB) поверх BIF::IByteStream (SPI full-duplex) + soft CS.
 *
 * Каждый write() на SPI-потоке тактирует MOSI и кладёт MISO в RX —
 * обмен: purge → write → flush → read (чанками, чтобы не переполнить RX-кольцо).
 *
 * write() стирает затронутые 4K-секторы целиком (остальные байты сектора → 0xFF).
 * program() пишет только в уже стёртую область (биты только 1→0).
 */
class W25Q {
public:
    static constexpr uint32_t kPageSize   = 256U;
    static constexpr uint32_t kSectorSize = 4096U;
    static constexpr uint32_t kBlockSize  = 65536U;
    static constexpr uint32_t kSize       = 2U * 1024U * 1024U; ///< W25Q16JV

    static constexpr uint8_t kCmdWriteEnable = 0x06U;
    static constexpr uint8_t kCmdReadStatus1 = 0x05U;
    static constexpr uint8_t kCmdReadData    = 0x03U;
    static constexpr uint8_t kCmdPageProgram = 0x02U;
    static constexpr uint8_t kCmdSectorErase = 0x20U;
    static constexpr uint8_t kCmdJedecId     = 0x9FU;

    static constexpr uint8_t kSrBusy = 0x01U;
    static constexpr uint8_t kSrWel  = 0x02U;

    static constexpr uint8_t kJedecMfr  = 0xEFU;
    static constexpr uint8_t kJedecType = 0x40U;
    static constexpr uint8_t kJedecCap  = 0x15U;

    static constexpr uint32_t kTimeoutPageMs   = 10U;
    static constexpr uint32_t kTimeoutSectorMs = 500U;

    /// Чанк обмена ≤ типичного RX-кольца SpiStream (256); запас от overrun.
    static constexpr size_t kXferChunk = 128U;

    /**
     * @param stream SPI ByteStream (полный дуплекс), уже open().
     * @param cs     soft CS, active low; должен жить не меньше драйвера (PortX::pin<N>).
     */
    W25Q(BIF::IByteStream& stream, const GPIO::Pin& cs) noexcept
        : _stream(stream)
        , _cs(cs)
    {
        _cs.Init(GPIO::Mode::Output, GPIO::Pull::None, GPIO::Speed::High);
        deselect();
    }

    BIF::IByteStream& stream() noexcept { return _stream; }
    const GPIO::Pin& cs() const noexcept { return _cs; }

    bool begin() noexcept
    {
        uint8_t mfr = 0, type = 0, cap = 0;
        if (!readJedec(mfr, type, cap))
            return false;
        return mfr == kJedecMfr && type == kJedecType && cap == kJedecCap;
    }

    bool readJedec(uint8_t& mfr, uint8_t& type, uint8_t& cap) noexcept
    {
        const uint8_t tx[4] = {kCmdJedecId, 0xFFU, 0xFFU, 0xFFU};
        uint8_t rx[4] = {};
        select();
        const bool ok = transfer(tx, rx, 4);
        deselect();
        if (!ok)
            return false;
        mfr = rx[1];
        type = rx[2];
        cap = rx[3];
        return true;
    }

    uint8_t readStatus1() noexcept
    {
        const uint8_t tx[2] = {kCmdReadStatus1, 0xFFU};
        uint8_t rx[2] = {};
        select();
        const bool ok = transfer(tx, rx, 2);
        deselect();
        return ok ? rx[1] : 0xFFU;
    }

    bool isBusy() noexcept { return (readStatus1() & kSrBusy) != 0U; }

    bool waitWhileBusy(uint32_t timeout_ms) noexcept
    {
        const uint32_t t0 = HAL_GetTick();
        while (isBusy()) {
            if ((HAL_GetTick() - t0) >= timeout_ms)
                return false;
        }
        return true;
    }

    bool read(uint32_t addr, void* dst, size_t len) noexcept
    {
        if (dst == nullptr || len == 0U)
            return len == 0U;
        if (!checkRange(addr, len))
            return false;
        if (!waitWhileBusy(kTimeoutSectorMs))
            return false;

        auto* out = static_cast<uint8_t*>(dst);
        const uint8_t hdr[4] = {
            kCmdReadData,
            static_cast<uint8_t>((addr >> 16) & 0xFFU),
            static_cast<uint8_t>((addr >> 8) & 0xFFU),
            static_cast<uint8_t>(addr & 0xFFU),
        };

        select();
        bool ok = transfer(hdr, nullptr, 4);
        if (ok)
            ok = transfer(nullptr, out, len);
        deselect();
        return ok;
    }

    bool eraseSector(uint32_t addr) noexcept
    {
        if (addr >= kSize)
            return false;
        if (!waitWhileBusy(kTimeoutSectorMs))
            return false;
        if (!writeEnable())
            return false;

        const uint32_t a = addr & ~(kSectorSize - 1U);
        const uint8_t cmd[4] = {
            kCmdSectorErase,
            static_cast<uint8_t>((a >> 16) & 0xFFU),
            static_cast<uint8_t>((a >> 8) & 0xFFU),
            static_cast<uint8_t>(a & 0xFFU),
        };
        select();
        const bool ok = transfer(cmd, nullptr, 4);
        deselect();
        return ok && waitWhileBusy(kTimeoutSectorMs);
    }

    bool program(uint32_t addr, const void* src, size_t len) noexcept
    {
        if (src == nullptr || len == 0U)
            return len == 0U;
        if (!checkRange(addr, len))
            return false;

        const auto* in = static_cast<const uint8_t*>(src);
        while (len > 0U) {
            const uint32_t pageOff = addr & (kPageSize - 1U);
            const size_t chunk = minSize(len, static_cast<size_t>(kPageSize - pageOff));
            if (!programPage(addr, in, chunk))
                return false;
            addr += static_cast<uint32_t>(chunk);
            in += chunk;
            len -= chunk;
        }
        return true;
    }

    /**
     * Запись с авто-erase секторов.
     * Затронутый 4K-сектор стирается целиком; программируется только переданный диапазон.
     */
    bool write(uint32_t addr, const void* src, size_t len) noexcept
    {
        if (src == nullptr || len == 0U)
            return len == 0U;
        if (!checkRange(addr, len))
            return false;

        const auto* in = static_cast<const uint8_t*>(src);
        while (len > 0U) {
            const uint32_t sector = addr & ~(kSectorSize - 1U);
            const uint32_t off = addr - sector;
            const size_t chunk = minSize(len, static_cast<size_t>(kSectorSize - off));

            if (!eraseSector(sector))
                return false;
            if (!program(addr, in, chunk))
                return false;

            addr += static_cast<uint32_t>(chunk);
            in += chunk;
            len -= chunk;
        }
        return true;
    }

private:
    BIF::IByteStream& _stream;
    const GPIO::Pin& _cs;

    void select() noexcept
    {
        _cs.Clear();
        _stream.purge();
        _stream.purgeOutput();
    }

    void deselect() noexcept
    {
        _stream.flush();
        _cs.Set();
    }

    /**
     * Полный дуплекс через ByteStream: write тактирует, flush ждёт TX, read забирает MISO.
     * tx == nullptr → на шину 0xFF; rx == nullptr → ответ отбрасывается.
     */
    bool transfer(const uint8_t* tx, uint8_t* rx, size_t n) noexcept
    {
        if (!_stream.isOpen() || n == 0U)
            return n == 0U;

        uint8_t txBuf[kXferChunk];
        uint8_t rxBuf[kXferChunk];
        for (size_t i = 0; i < kXferChunk; ++i)
            txBuf[i] = 0xFFU;

        size_t off = 0;
        while (off < n) {
            const size_t chunk = minSize(n - off, kXferChunk);
            const uint8_t* tptr = tx ? (tx + off) : txBuf;
            uint8_t* rptr = rx ? (rx + off) : rxBuf;

            if (!writeAll(tptr, chunk))
                return false;
            _stream.flush();
            if (!readAll(rptr, chunk))
                return false;

            off += chunk;
        }
        return true;
    }

    bool writeAll(const uint8_t* data, size_t n) noexcept
    {
        size_t done = 0;
        while (done < n) {
            const size_t w = _stream.write(data + done, n - done);
            if (w == 0U) {
                // Backpressure: ждём место в TX (IRQ опустошает кольцо).
                if (!_stream.isOpen())
                    return false;
                continue;
            }
            done += w;
        }
        return true;
    }

    bool readAll(uint8_t* data, size_t n) noexcept
    {
        size_t done = 0;
        const uint32_t t0 = HAL_GetTick();
        while (done < n) {
            const size_t r = _stream.read(data + done, n - done);
            if (r == 0U) {
                if (!_stream.isOpen())
                    return false;
                if ((HAL_GetTick() - t0) >= kTimeoutPageMs)
                    return false;
                continue;
            }
            done += r;
        }
        return true;
    }

    static bool checkRange(uint32_t addr, size_t len) noexcept
    {
        if (len == 0U)
            return true;
        if (addr >= kSize)
            return false;
        return (kSize - addr) >= len;
    }

    static size_t minSize(size_t a, size_t b) noexcept { return (a < b) ? a : b; }

    bool writeEnable() noexcept
    {
        const uint8_t cmd = kCmdWriteEnable;
        select();
        const bool ok = transfer(&cmd, nullptr, 1);
        deselect();
        return ok && ((readStatus1() & kSrWel) != 0U);
    }

    bool programPage(uint32_t addr, const uint8_t* data, size_t len) noexcept
    {
        if (len == 0U || len > kPageSize)
            return false;
        if ((addr & (kPageSize - 1U)) + len > kPageSize)
            return false;
        if (!waitWhileBusy(kTimeoutPageMs))
            return false;
        if (!writeEnable())
            return false;

        const uint8_t hdr[4] = {
            kCmdPageProgram,
            static_cast<uint8_t>((addr >> 16) & 0xFFU),
            static_cast<uint8_t>((addr >> 8) & 0xFFU),
            static_cast<uint8_t>(addr & 0xFFU),
        };
        select();
        bool ok = transfer(hdr, nullptr, 4);
        if (ok)
            ok = transfer(data, nullptr, len);
        deselect();
        return ok && waitWhileBusy(kTimeoutPageMs);
    }
};

} // namespace PHL
