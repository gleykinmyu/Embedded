#pragma once

/**
 * Сохранение диагностики HardFault / MemManage / BusFault / UsageFault в BKPSRAM
 * (сохраняется через SystemReset при питании backup-домена) и разбор при следующем старте.
 */

#include <cstdint>

namespace CrashDump {

inline constexpr uint32_t kMagic = 0xCFA117EDu;

enum class Kind : uint32_t {
    None = 0u,
    HardFault = 1u,
    MemManage = 2u,
    BusFault = 3u,
    UsageFault = 4u,
};

struct Record {
    uint32_t magic = 0u;
    Kind kind = Kind::None;
    uint32_t cfsr = 0u;
    uint32_t hfsr = 0u;
    uint32_t dfsr = 0u;
    uint32_t mmfar = 0u;
    uint32_t bfar = 0u;
    uint32_t afsr = 0u;
    uint32_t r0 = 0u;
    uint32_t r1 = 0u;
    uint32_t r2 = 0u;
    uint32_t r3 = 0u;
    uint32_t r12 = 0u;
    uint32_t lr = 0u;
    uint32_t pc = 0u;
    uint32_t xpsr = 0u;
    uint32_t msp = 0u;
    uint32_t psp = 0u;
};

/** Включить MemManage / BusFault / UsageFault (иначе эскалация в HardFault). */
void enableFaultTraps() noexcept;

/**
 * Если в BKPSRAM есть валидная запись — скопировать в out и очистить слот.
 * @return true, если dump был.
 */
[[nodiscard]] bool take(Record& out) noexcept;

/** Имя kind для логов. */
[[nodiscard]] const char* kindName(Kind k) noexcept;

} // namespace CrashDump
