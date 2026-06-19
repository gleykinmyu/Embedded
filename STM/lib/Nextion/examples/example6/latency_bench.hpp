#pragma once

/**
 * Результаты замера; прогон — в `app.hpp` (`LatencyBenchApp::measureOne`).
 */

#include <algorithm>
#include <cstdint>

#include "nex.hpp"

namespace nex::examples::ex6 {

struct BenchResult {
    const char* label = nullptr;
    uint8_t page_id = 0u;
    uint8_t comp_id = 0u;
    uint32_t ms = 0u;
    unsigned got_success = 0u;
    /** 0 — таймаут; 0x01 — Success; иначе код panel-статуса при FAIL. */
    uint8_t rx_status = 0u;
};

class LatencyRecorder {
public:
    static constexpr unsigned kMaxResults = 128u;

    void add(const char* label, uint8_t page_id, uint8_t comp_id, uint32_t ms, unsigned got_success,
        uint8_t rx_status = 0u) noexcept
    {
        if (_count >= kMaxResults)
            return;
        _results[_count++] = BenchResult{label, page_id, comp_id, ms, got_success, rx_status};
    }

    void printSummary() const noexcept
    {
        if (_count == 0u) {
            NEX_DBG("[ex6] no measurements\n");
            return;
        }

        BenchResult sorted[kMaxResults]{};
        for (unsigned i = 0u; i < _count; ++i)
            sorted[i] = _results[i];
        std::sort(sorted, sorted + _count, [](const BenchResult& a, const BenchResult& b) noexcept {
            return a.ms > b.ms;
        });

        uint32_t sum = 0u;
        uint32_t min_ms = sorted[0].ms;
        uint32_t max_ms = sorted[0].ms;
        unsigned ok_total = 0u;
        for (unsigned i = 0u; i < _count; ++i) {
            sum += sorted[i].ms;
            ok_total += sorted[i].got_success;
            if (sorted[i].ms < min_ms)
                min_ms = sorted[i].ms;
            if (sorted[i].ms > max_ms)
                max_ms = sorted[i].ms;
        }

        NEX_DBG("\n[ex6] === latency summary (%u cmds, matched Success, bkcmd=Always) ===\n",
            static_cast<unsigned>(_count));
        NEX_DBG("[ex6] min=%lu ms  max=%lu ms  avg=%lu ms  ok=%u/%u\n",
            static_cast<unsigned long>(min_ms), static_cast<unsigned long>(max_ms),
            static_cast<unsigned long>(sum / _count), ok_total, static_cast<unsigned>(_count));

        for (unsigned i = 0u; i < _count; ++i) {
            const BenchResult& r = sorted[i];
            if (r.got_success) {
                NEX_DBG("[ex6] %4lu ms  p%u c%u  OK  %s\n", static_cast<unsigned long>(r.ms),
                    static_cast<unsigned>(r.page_id), static_cast<unsigned>(r.comp_id), r.label);
            } else if (r.rx_status != 0u) {
                NEX_DBG("[ex6] %4lu ms  p%u c%u  FAIL  %s  (rx 0x%02X)\n",
                    static_cast<unsigned long>(r.ms), static_cast<unsigned>(r.page_id),
                    static_cast<unsigned>(r.comp_id), r.label, static_cast<unsigned>(r.rx_status));
            } else {
                NEX_DBG("[ex6] %4lu ms  p%u c%u  FAIL  %s  (timeout)\n",
                    static_cast<unsigned long>(r.ms), static_cast<unsigned>(r.page_id),
                    static_cast<unsigned>(r.comp_id), r.label);
            }
        }
        NEX_DBG("[ex6] === end summary ===\n\n");
    }

    [[nodiscard]] unsigned count() const noexcept { return _count; }

private:
    BenchResult _results[kMaxResults]{};
    unsigned _count = 0u;
};

} // namespace nex::examples::ex6
