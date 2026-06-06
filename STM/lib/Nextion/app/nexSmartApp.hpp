#pragma once

/**
 * @file nexSmartApp.hpp
 *
 * `Application` с сопоставлением panel `.id` ↔ compiled id (Discover / Flash).
 */

#include <cstddef>
#include <cstdint>

#include "nexApplication.hpp"
#include "../idmap/nexIdMap.hpp"

namespace nex {

class SmartApp : public Application {
public:
    explicit SmartApp(BIF::IByteStream& stream, Rect screen, Application::ClockMsFn clockMs,
        idmap::Table& id_map_table) noexcept;

    void startDiscover() noexcept;

    [[nodiscard]] bool isDiscoverBusy() const noexcept;

    bool idMapLoadFromBuffer(const uint8_t* buf, std::size_t len) noexcept;

    /** UART: `objname -> id=N` для ctor (до `applyFromTable`). @return число расхождений. */
    uint16_t printIdMapDiff() noexcept;

    virtual void onDiscoverComplete(bool success) noexcept { (void)success; }

    void onPageChange(uint8_t page_id) noexcept override;

    void update() noexcept override;

protected:
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
    void discoverOnPageChanged(uint8_t page_id) noexcept;

    [[nodiscard]] bool discoverCanProbe() const noexcept;
    [[nodiscard]] static uint8_t discoverPageIdForIndex(uint8_t page_index) noexcept;
    [[nodiscard]] uint8_t discoverComponentCount(uint8_t page_index) noexcept;
    [[nodiscard]] bool discoverHasComponent(uint8_t page_index, uint8_t compiled_id) noexcept;

    idmap::Table& _table;
    IdMapMode _mode = IdMapMode::Compiled;
    DiscoverPhase _phase = DiscoverPhase::Idle;
    uint8_t _page_index = 0u;
    uint8_t _scan_id = 1u;
    uint8_t _polled = 0u;
    uint32_t _deadline_ms = 0u;
    uint32_t _switch_page_last_sendme_ms = 0u;
};

} // namespace nex
