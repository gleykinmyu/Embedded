#include "memstat.hpp"

#include <cstdio>

#include <stm32f4xx.h>

extern "C" {
extern char end;
extern char _sdata;
extern char _edata;
extern char _sbss;
extern char _ebss;
extern char _estack;
extern char _Min_Stack_Size;
extern char _Min_Heap_Size;
char* _sbrk(int incr);
}

namespace memstat {
namespace {

constexpr uint32_t kStackFill = 0xA5A5A5A5u;

struct State {
    uint32_t minSp = 0xFFFFFFFFu;
    bool painted = false;
};

State g_state{};

uint32_t estackAddr() noexcept
{
    return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(&_estack));
}

uint32_t stackRegionLo() noexcept
{
    return estackAddr()
        - static_cast<uint32_t>(reinterpret_cast<uintptr_t>(&_Min_Stack_Size));
}

uint32_t minStackBytes() noexcept
{
    return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(&_Min_Stack_Size));
}

uint32_t minHeapBytes() noexcept
{
    return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(&_Min_Heap_Size));
}

char* heapStart() noexcept
{
    return &end;
}

char* heapBreak() noexcept
{
    return _sbrk(0);
}

void paintStackWatermark() noexcept
{
    const uint32_t lo = stackRegionLo();
    const uint32_t hi = estackAddr();
    auto* word = reinterpret_cast<volatile uint32_t*>(lo);
    while (reinterpret_cast<uintptr_t>(word) < static_cast<uintptr_t>(hi)) {
        *word++ = kStackFill;
    }
    g_state.painted = true;
}

std::size_t stackPeakFromWatermark() noexcept
{
    if (!g_state.painted) {
        return 0u;
    }

    const uint32_t lo = stackRegionLo();
    const uint32_t hi = estackAddr();
    auto* p = reinterpret_cast<const uint32_t*>(lo);
    const auto* const endWord = reinterpret_cast<const uint32_t*>(hi);
    while (p < endWord && *p == kStackFill) {
        ++p;
    }
    return static_cast<std::size_t>(hi - reinterpret_cast<uintptr_t>(p));
}

std::size_t stackPeakFromMinSp() noexcept
{
    if (g_state.minSp == 0xFFFFFFFFu) {
        return 0u;
    }
    return static_cast<std::size_t>(estackAddr() - g_state.minSp);
}

std::size_t stackPeakCombined() noexcept
{
    const std::size_t wm = stackPeakFromWatermark();
    const std::size_t sp = stackPeakFromMinSp();
    return (wm > sp) ? wm : sp;
}

} // namespace

void boot() noexcept
{
    g_state.minSp = __get_MSP();
    paintStackWatermark();
}

void trackStackMin() noexcept
{
    const uint32_t sp = __get_MSP();
    if (sp < g_state.minSp) {
        g_state.minSp = sp;
    }
}

std::size_t staticBytes() noexcept
{
    return static_cast<std::size_t>(&_edata - &_sdata)
        + static_cast<std::size_t>(&_ebss - &_sbss);
}

std::size_t heapUsedBytes() noexcept
{
    char* const brk = heapBreak();
    if (brk == nullptr) {
        return 0u;
    }
    const ptrdiff_t used = brk - heapStart();
    return (used > 0) ? static_cast<std::size_t>(used) : 0u;
}

std::size_t stackUsedNowBytes() noexcept
{
    return static_cast<std::size_t>(estackAddr() - __get_MSP());
}

std::size_t stackPeakBytes() noexcept
{
    return stackPeakCombined();
}

std::size_t heapStackGapBytes() noexcept
{
    const uint32_t msp = __get_MSP();
    const uint32_t heapTop = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(heapBreak()));
    if (msp <= heapTop) {
        return 0u;
    }
    return static_cast<std::size_t>(msp - heapTop);
}

Snapshot snapshot() noexcept
{
    Snapshot s{};
    s.staticBytes = staticBytes();
    s.heapUsed = heapUsedBytes();
    s.heapReserved = minHeapBytes();
    s.stackUsedNow = stackUsedNowBytes();
    s.stackPeak = stackPeakBytes();
    s.stackReserved = minStackBytes();
    s.gapBytes = heapStackGapBytes();
    s.ramTotal = static_cast<std::size_t>(reinterpret_cast<uintptr_t>(&_estack)
        - reinterpret_cast<uintptr_t>(&_sdata));
    return s;
}

void format(const Snapshot& snap, char* buf, const std::size_t cap) noexcept
{
    if (buf == nullptr || cap == 0u) {
        return;
    }

    std::snprintf(buf, cap, "S:%u/%u H:%u G:%u",
        static_cast<unsigned>(snap.stackPeak),
        static_cast<unsigned>(snap.stackReserved),
        static_cast<unsigned>(snap.heapUsed),
        static_cast<unsigned>(snap.gapBytes));
    buf[cap - 1u] = '\0';
}

} // namespace memstat

extern "C" void HAL_SYSTICK_Callback(void)
{
    memstat::trackStackMin();
}
