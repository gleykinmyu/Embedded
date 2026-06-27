#pragma once

/**
 * `AppUI<>` + `idmap::Table`: Compiled / Discover / Flash,
 * сопоставление panel `.id` ↔ compiled id в ctor виджетов.
 */

#include <cstddef>
#include <cstdint>

#include "../comp/nexAppUI.hpp"
#include "nexIdMap.hpp"
#include "../core/nexTimeout.hpp"

namespace nex {

class SmartApp : public AppUI<kDefaultMaxPages> {
public:
    explicit SmartApp(BIF::IByteStream& stream, Rect screen, AppTiming timing,
        idmap::Table& id_map_table) noexcept;

    void startDiscover() noexcept;

    /** `true` пока фаза Discover не `Idle` / `Done` / `Failed`. */
    [[nodiscard]] bool isDiscoverBusy() const noexcept;

    /** Загрузка NXCI из буфера; при успехе — `applyFromTable()` и режим Flash. */
    bool idMapLoadFromBuffer(const uint8_t* buf, std::size_t len) noexcept;

    /** UART: `objname -> id=N` для ctor (до `applyFromTable`). @return число расхождений. */
    uint16_t printIdMapDiff() noexcept;

    /** Хук после завершения Discover (`success` — без `Failed`). */
    virtual void onDiscoverComplete(bool success) noexcept { (void)success; }

    void onPageChange(const msg::evPage& e) noexcept override;

    void update() noexcept override;

protected:
    /** Discover: перехват `get id` и `Route::compIdMapPoll`. */
    bool dispatchResponse(const Message& m, bool txIdle) noexcept override;
    void dispatchEvent(const Message& m) noexcept override;
    void onTimeout(const Transaction& active) noexcept override;

private:
    enum class IdMapMode : uint8_t { Compiled = 0, Discover = 1, Flash = 2 };

    enum class DiscoverPhase : uint8_t {
        Idle,
        SwitchPage,
        GetId,
        Done,
        Failed,
    };

    void applyFromTable() noexcept;
    void discoverArm() noexcept;
    void discoverBegin() noexcept;
    void discoverTick(uint32_t now_ms) noexcept;
    void discoverFail(const char* reason) noexcept;
    void discoverFinishSuccess() noexcept;
    void discoverAdvanceCompId() noexcept;
    void discoverNextPage() noexcept;
    void discoverEnqueueGetId(uint8_t comp_slot) noexcept;
    void discoverOnPollResponse(uint8_t comp_slot, uint8_t panel_id) noexcept;
    void discoverOnPageChanged(uint8_t page) noexcept;

    [[nodiscard]] bool discoverCanProbe() const noexcept;
    [[nodiscard]] uint8_t discoverComponentCount(uint8_t page_index) noexcept;
    [[nodiscard]] bool discoverHasComponent(uint8_t page_index, uint8_t compiled_id) noexcept;

    idmap::Table& _table;
    IdMapMode _mode = IdMapMode::Compiled;
    DiscoverPhase _phase = DiscoverPhase::Idle;
    uint8_t _page_index = 0u;
    uint8_t _scan_id = 1u;
    uint8_t _polled = 0u;
    MsTimer _switchPageTimeout{};
    uint32_t _switch_page_last_sendme_ms = 0u;
};

} // namespace nex
